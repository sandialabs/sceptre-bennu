#!/usr/bin/env bash
# Docker verification harness for bennu. See README.md.
# host:  [BASE_IMAGE=ubuntu:26.04] [PLATFORM=linux/amd64] run.sh [-s SRC_DIR] [-v CACHE_VOLUME] <build|smoke|ctest|pytest|lint|throughput|all>
set -eo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ "$1" != "--inside" ]; then
    SRC="$HERE/../.."; VOL=bennu-verify-cache
    while getopts "s:v:" o; do case $o in s) SRC=$OPTARG;; v) VOL=$OPTARG;; *) exit 2;; esac; done
    shift $((OPTIND-1))
    [ -n "$1" ] || { sed -n 2,3p "$0"; exit 2; }
    SRC="$(cd "$SRC" && pwd)"
    # BASE_IMAGE picks the Ubuntu release (default 22.04); PLATFORM (e.g. linux/amd64) forces an emulated arch.
    BASE_IMAGE=${BASE_IMAGE:-ubuntu:22.04}; IMG=bennu-verify:${BASE_IMAGE#*:}-lint
    [ -z "$PLATFORM" ] || { IMG=$IMG-${PLATFORM#*/}; PLAT=(--platform "$PLATFORM"); }
    case ${BASE_IMAGE#*:} in 22.04) CV=15;; 24.04) CV=18;; *) CV=21;; esac   # clang-format/tidy packaged per release
    docker image inspect "$IMG" >/dev/null 2>&1 \
        || docker build "${PLAT[@]}" --build-arg BASE_IMAGE="$BASE_IMAGE" --build-arg CLANG_VERSION="${CLANG_VERSION:-$CV}" --build-arg BSDTAR_SHIM="$([ "${BASE_IMAGE#*:}" = 22.04 ] || echo 1)" -t "$IMG" "$HERE"
    exec docker run --rm "${PLAT[@]}" -v "$SRC:/src:ro" -v "$HERE:/harness:ro" -v "$VOL:/cache" -v bennu-ccache:/ccache \
        -e CLANG_VERSION="${CLANG_VERSION:-$CV}" -e BASE -e THROUGHPUT_SECS -e THROUGHPUT_TAGS -e THROUGHPUT_CYCLE_MS -e THROUGHPUT_MAX_CYCLE_S -e BENNU_LOG_LEVEL \
        "$IMG" bash /harness/run.sh --inside "$@"
fi

# ---- inside the container ----
shift
W=/cache/src; B=/cache/build
# ccache is shared across volumes so per-agent volumes stay warm
export CCACHE_DIR=/ccache/ccache

sync_src() {  # copy (not mount) so parallel runs and the build tree never touch the host checkout
    mkdir -p $W $B
    rsync -a --delete --exclude .git --exclude build --exclude .beads --exclude '*.pyc' --exclude __pycache__ /src/ $W/
}
build() {
    sync_src
    cd $B
    cmake -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER_LAUNCHER=ccache $W
    make -j"$(nproc)"
}
install_() { [ -f $B/Makefile ] || build; (cd $B && make install >/dev/null) && ldconfig; }

ctest_() { sync_src; (cd $B && make -j"$(nproc)") && install_; ln -sfn $W/data /cache/data; cd $B && ctest --output-on-failure; }
pytest_() {
    sync_src
    [ -d $W/src/pybennu/tests ] || { echo "no src/pybennu/tests, skipping"; return 0; }
    cd $W/src/pybennu && PYTHONPATH=$W/src/pybennu python3 -m pytest tests -q
}
lint() {  # python/codespell on the synced copy; cpp-check needs git so it runs against /src (read-only)
    sync_src
    local rc=0
    cd $W
    make -k python-check codespell || rc=1
    # git clang-format writes a temp index into the git dir, and /src is read-only: use a copy of .git
    rm -rf /tmp/srcgit && cp -a /src/.git /tmp/srcgit
    git config --global --add safe.directory /src
    GIT_DIR=/tmp/srcgit GIT_WORK_TREE=/src make -C /src cpp-check CLANG_FORMAT=clang-format-"$CLANG_VERSION" GIT_CLANG_FORMAT=git-clang-format-"$CLANG_VERSION" ${BASE:+BASE=$BASE} || rc=1
    return $rc
}
throughput() {  # ~N output-mapped tags change every cycle; count WRITEs arriving at a mock provider
    install_
    local n=${THROUGHPUT_TAGS:-50} ms=${THROUGHPUT_CYCLE_MS:-100} secs=${THROUGHPUT_SECS:-60}
    local t=/harness/throughput ep=tcp://127.0.0.1:5555
    python3 $t/gen.py "$n" "$ms" "$ep" >/tmp/tp.xml
    # INFO by default: DEBUG logs every logic rule each cycle, which skews the measurement at high tag counts
    export BENNU_LOG_LEVEL=${BENNU_LOG_LEVEL:-INFO}
    python3 $t/provider.py "$ep" "$secs" p0 &
    local pp=$!
    bennu-field-device --f /tmp/tp.xml >/tmp/tp-fd.out 2>&1 &
    local fp=$!
    wait $pp; local rc=$?
    kill $fp 2>/dev/null || true
    echo "(tags=$n cycle-time=${ms}ms window=${secs}s)"
    return $rc
}

for m in "$@"; do
    [ "$m" = all ] && set -- build ctest pytest throughput
done
for m in "$@"; do
    echo "=== $m ==="
    case $m in
        build) build;; smoke|ctest) ctest_;; pytest) pytest_;;
        lint) lint;; throughput) throughput;; *) echo "unknown mode $m"; exit 2;;
    esac
done
