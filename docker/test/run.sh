#!/usr/bin/env bash
# Docker verification harness for bennu. See README.md.
# host:  run.sh [-s SRC_DIR] [-v CACHE_VOLUME] <build|smoke|ctest|pytest|gotest|lint|throughput|all>
set -eo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ "$1" != "--inside" ]; then
    SRC="$HERE/../.."; VOL=bennu-verify-cache
    while getopts "s:v:" o; do case $o in s) SRC=$OPTARG;; v) VOL=$OPTARG;; *) exit 2;; esac; done
    shift $((OPTIND-1))
    [ -n "$1" ] || { sed -n 2,3p "$0"; exit 2; }
    SRC="$(cd "$SRC" && pwd)"
    docker image inspect bennu-verify:22.04-lint >/dev/null 2>&1 || docker build -t bennu-verify:22.04-lint "$HERE"
    exec docker run --rm -v "$SRC:/src:ro" -v "$HERE:/harness:ro" -v "$VOL:/cache" -v bennu-ccache:/ccache \
        -e BASE -e THROUGHPUT_SECS -e THROUGHPUT_TAGS -e THROUGHPUT_CYCLE_MS -e THROUGHPUT_MAX_CYCLE_S -e BENNU_LOG_LEVEL \
        bennu-verify:22.04-lint bash /harness/run.sh --inside "$@"
fi

# ---- inside the container ----
shift
W=/cache/src; B=/cache/build
# ccache/go caches are shared across volumes (both are safe for concurrent use) so per-agent volumes stay warm
export CCACHE_DIR=/ccache/ccache GOPATH=/ccache/go GOCACHE=/ccache/gocache

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

smoke() {  # verbatim from .github/workflows/build.yml "test bennu"
    install_
    rm -rf /tmp/smoke && mkdir /tmp/smoke && cd /tmp/smoke
    local cfg=$W/data/configs/ep
    bennu-test-ep-server --d 1 >ep.out 2>&1 &
    bennu-field-device --f $cfg/dnp3-server.xml >fd-server.out 2>&1 &
    bennu-field-device --f $cfg/dnp3-client.xml >fd-client.out 2>&1 &
    sleep 5
    bennu-probe --c query >probe.out
    grep "brkr" probe.out
    sleep 10
    bennu-probe --c read --t brkr >>probe.out
    bennu-probe --c read --t load-power >>probe.out
    sleep 3
    cat probe.out
    grep "brkr:true" probe.out
    grep "load-power:400.5" probe.out
    bennu-probe --c write --t load-breaker-toggle --s false >>probe.out
    bennu-probe --c write --t load-mw-setpoint --v 999 >>probe.out
    sleep 8
    bennu-probe --c read --t brkr >>probe.out
    bennu-probe --c read --t load-power >>probe.out
    sleep 3
    cat probe.out
    grep "brkr:false" probe.out
    grep "load-power:999" probe.out
    echo -e "help\nexit" | bennu-brash >brash.out 2>&1
    grep "SCEPTRE Field-Device" brash.out
    bennu-watcherd --help
    bennu-simulink-provider --help
    bennu-field-deviced --help
    bennu-simulink-provider-helics --help
    echo SMOKE OK
}
ctest_() { install_; ln -sfn $W/data /cache/data; cd $B && ctest --output-on-failure; }
pytest_() {
    sync_src
    [ -d $W/src/pybennu/tests ] || { echo "no src/pybennu/tests, skipping"; return 0; }
    cd $W/src/pybennu && PYTHONPATH=$W/src/pybennu python3 -m pytest tests -q
}
gotest() {
    sync_src
    cd $W/src/gobennu
    PKG_CONFIG_PATH=/opt/zmq-draft/lib/pkgconfig LD_LIBRARY_PATH=/opt/zmq-draft/lib go test ./...
}
lint() {  # python/go/codespell on the synced copy; cpp-check needs git so it runs against /src (read-only)
    sync_src
    local rc=0
    cd $W
    PKG_CONFIG_PATH=/opt/zmq-draft/lib/pkgconfig LD_LIBRARY_PATH=/opt/zmq-draft/lib \
        make -k python-check go-check codespell || rc=1
    # git clang-format writes a temp index into the git dir, and /src is read-only: use a copy of .git
    rm -rf /tmp/srcgit && cp -a /src/.git /tmp/srcgit
    git config --global --add safe.directory /src
    GIT_DIR=/tmp/srcgit GIT_WORK_TREE=/src make -C /src cpp-check CLANG_FORMAT=clang-format-15 GIT_CLANG_FORMAT=git-clang-format-15 ${BASE:+BASE=$BASE} || rc=1
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
    [ "$m" = all ] && set -- build smoke ctest pytest gotest throughput
done
for m in "$@"; do
    echo "=== $m ==="
    case $m in
        build) build;; smoke) smoke;; ctest) ctest_;; pytest) pytest_;;
        gotest) gotest;; lint) lint;; throughput) throughput;; *) echo "unknown mode $m"; exit 2;;
    esac
done
