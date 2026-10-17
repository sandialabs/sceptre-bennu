# Testing

## C++ (ctest, doctest)

    mkdir build && cd build && cmake .. && make -j$(nproc)
    ctest --output-on-failure

Every `test/test_*.cpp` becomes its own executable and ctest entry (`test/CMakeLists.txt` globs them).

- Unit tests (no network, no running processes): `test_unit_*.cpp`, e.g. `chunkMessage`, `split`,
  `Client::buildWritePayload`, `DataManager` snapshot and compare-and-erase.
- Integration tests: `test_ep_server`, `test_fd_client`, `test_fd_server`, `test_probe_*`, `test_brash`.
  They launch installed binaries and check output, so they need `make install` and a free port 5555.
  They assert exact output text, so log-level changes can break them. `test_ep_server` requires the exact
  startup lines.

### Performance regression guards

The 800-tag slowdown (500 ms sleep per request, one request per tag) passed every integration test, so
these guard against it directly. They check invariants and loose bounds, not exact timings.

- `test_unit_perf`: 100 `Client::send` round trips over inproc zmq must take under 2 s (the old sleep
  made it 50 s), and an 800-tag publish must use close to the minimum number of datagrams.
- `test_unit_outputmodule` "perf: 800 changed tags go out in one request".
- CI step "throughput regression guard (800 tags)" in `build-bennu`: 800 tags on a 1 s cycle against the
  mock provider, failing if the mean cycle is over 2 s (measured: 1.01 s; before batching: ~400 s). Run it
  locally with `THROUGHPUT_TAGS=800 THROUGHPUT_CYCLE_MS=1000 THROUGHPUT_MAX_CYCLE_S=2.0 docker/test/run.sh throughput`.

Wall-clock thresholds are set at about 2x the cycle time so slow CI runners don't flake. They catch
order-of-magnitude regressions, not 10% ones. For finer tracking, record `docker/test/run.sh throughput`
numbers in `performance.md`.

## pybennu (pytest)

    cd src/pybennu && pip install pytest pyzmq "loguru>=0.7,<1" && pytest tests

`tests/` covers chunking (`chunk_message`, 800 tags, trailing comma or not, each chunk under the MTU) and
the provider message handler (QUERY/READ/WRITE parsing via a stub subclass). They avoid the swig/helics
imports.

## gobennu

    cd src/gobennu && go test ./...

Needs libzmq with drafts (`pebbe/zmq4/draft`) and network access for modules on first run.

## Docker harness (`docker/test`)

The macOS host cannot build the C++/Go parts. `docker/test/run.sh` builds `ubuntu:22.04` with the
`build.yml` deps, pytest, Go and a drafts libzmq, then runs in a container:

    docker/test/run.sh build smoke ctest pytest gotest   # or: all
    docker/test/run.sh throughput

Source is mounted read-only and rsynced into a named volume that also holds the build dir and ccache.
Use a separate `-v VOLUME` per source tree or parallel run. See `docker/test/README.md` for options,
the throughput generator and baseline comparisons (`git worktree add /tmp/before HEAD`).

## CI (`.github/workflows/build.yml`)

- `build-bennu`: build, install, smoke script (ep-server, two field devices, probe greps), `ctest`, deb.
- `build-pybennu-dependencies`, `build-pybennu-wheel`: libzmq/helics debs and the wheel.
- `build-pybennu-wheel` also runs `pytest src/pybennu/tests` (with `PYTHONPATH=src/pybennu`).
- `test-gobennu`: builds libzmq 4.3.4 with drafts and runs `go test ./...` in `src/gobennu`. `release` needs it.
