# bennu Docker verification harness

Builds and tests bennu on ubuntu:22.04 (same deps as `.github/workflows/build.yml`, plus pytest/pyzmq and Go).
The macOS host cannot build the C++/Go parts, so everything runs in a container.

    docker/test/run.sh [-s SRC_DIR] [-v CACHE_VOLUME] <mode>...

Modes: `build`, `smoke` (build.yml "test bennu" script), `ctest`, `pytest` (src/pybennu/tests, skipped if absent),
`gotest` (`go test ./...` in src/gobennu), `lint` (python/go/codespell `make check` plus `cpp-check`; set `BASE=<ref>` to change the diff base; not part of `all` until the tree is clean), `throughput`, `all`.

- The image (`bennu-verify:22.04-lint`) is built on first use from `docker/test/Dockerfile`.
- Source is mounted read-only and rsynced into the volume, so the host tree is never touched. The volume
  (default `bennu-verify-cache`) holds the copy and the build dir; ccache and Go caches live in the shared `bennu-ccache` volume, so new volumes build warm.
- Baseline: `git worktree add /tmp/before HEAD`, then `run.sh -s /tmp/before -v bennu-verify-before throughput`.
  Use a separate `-v` volume per source tree or per parallel run, otherwise runs collide.
- `throughput`: a field device with `THROUGHPUT_TAGS` (50) output-mapped tags that all change every
  `THROUGHPUT_CYCLE_MS` (100 ms) cycle, writing to a mock provider (`throughput/provider.py`) for `THROUGHPUT_SECS`
  (60 s). Sets `BENNU_LOG_LEVEL=DEBUG`. Prints `requests_per_sec`, `tag_writes_per_sec` and `seconds_per_cycle`
  (mean gap between successive writes of the same tag; the end-to-end cycle cost).
