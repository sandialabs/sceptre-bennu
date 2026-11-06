# [2026.11.06](https://github.com/sandialabs/sceptre-bennu/compare/v2026.11.03...v2026.11.06) - 2026-11-06

### ❌ Breaking

_(gobennu)_ [**breaking**]: remove deprecated gobennu [_(e6b8fe6)_](https://github.com/sandialabs/sceptre-bennu/commit/e6b8fe6081678cd5837518314d7d02e4cddc7b82) ([PR 74](https://github.com/sandialabs/sceptre-bennu/pull/74))

> Removes the deprecated and unused Go implementation of bennu
> (`src/gobennu`) along with all associated build targets, CI jobs,
> test harnesses, developer tooling, and documentation references.

> remove deprecated gobennu

# [2026.11.03](https://github.com/sandialabs/sceptre-bennu/compare/v2026.09.30...v2026.11.03) - 2026-11-03

### 🚀 Features

- _(iec104)_ implement select-before-operate for control commands [_(eecacd2)_](https://github.com/sandialabs/sceptre-bennu/commit/eecacd23d8307a35605a9161aadbb583e8ab30d2) ([PR 68](https://github.com/sandialabs/sceptre-bennu/pull/68))

- _(release)_ use CalVer (vYYYY.MM.DD) for release names and tags [_(2e91e37)_](https://github.com/sandialabs/sceptre-bennu/commit/2e91e37ea4bb395d9f09091488fc9ad77fefc821) ([PR 63](https://github.com/sandialabs/sceptre-bennu/pull/63))

### 🐛 Bug Fixes

- _(pybennu)_ apply every device in a batched OpenDSS write [_(d0774bb)_](https://github.com/sandialabs/sceptre-bennu/commit/d0774bbca484c80a469c58200b755417b3dcada8) ([PR 64](https://github.com/sandialabs/sceptre-bennu/pull/64))

- _(distributed)_ keep publish datagrams within a configurable link MTU [_(e00008e)_](https://github.com/sandialabs/sceptre-bennu/commit/e00008ead695ea13b19e95bb12afb3d71151d1da) ([PR 67](https://github.com/sandialabs/sceptre-bennu/pull/67))

- _(simulink-provider)_ size updates FIFO to hold a full write batch [_(e59691f)_](https://github.com/sandialabs/sceptre-bennu/commit/e59691f4707f794ce6585ccf3b9c80682c23446f) ([PR 66](https://github.com/sandialabs/sceptre-bennu/pull/66))

- _(pybennu)_ separate PowerWorld query tags with commas [_(3315fb2)_](https://github.com/sandialabs/sceptre-bennu/commit/3315fb2fe7bd61f3a9ebedd0cd1d332b72332462) ([PR 65](https://github.com/sandialabs/sceptre-bennu/pull/65))

- _(dnp3)_ register control points as output status, stop event overflow [_(a17b391)_](https://github.com/sandialabs/sceptre-bennu/commit/a17b3915e8fa497827f3925a5a71abdb5da4b803) ([PR 52](https://github.com/sandialabs/sceptre-bennu/pull/52))

- _(gobennu)_ keep failed writes, time out the updater, isolate logic errors [_(ed658d7)_](https://github.com/sandialabs/sceptre-bennu/commit/ed658d7663b2e3afb90813aafe2c3abe8dfcd27b)

- _(pybennu)_ logging, publisher chunking, provider fixes, batch writes [_(62822a1)_](https://github.com/sandialabs/sceptre-bennu/commit/62822a1de015fed29b365afcd6bbf306d29e48fd) ([PR 56](https://github.com/sandialabs/sceptre-bennu/pull/56))

- _(bennu)_ batch field-device writes; 800 tags ~398 s -> 1.0 s per cycle [_(91fd137)_](https://github.com/sandialabs/sceptre-bennu/commit/91fd1372fff72d18eef94d32345e7f40a11b2e28) ([PR 55](https://github.com/sandialabs/sceptre-bennu/pull/55))

### 🚜 Refactor

- _(pybennu)_ log with loguru instead of logging and print [_(2d09959)_](https://github.com/sandialabs/sceptre-bennu/commit/2d09959350e610227a1c26df92d44ba312ed797f)

### 🧹 Chore

- _(lint)_ exclude generated CHANGELOG.md from rumdl [_(5958cdf)_](https://github.com/sandialabs/sceptre-bennu/commit/5958cdf8a8ba5084f6e60ca79cde1c4acac10c4a) ([PR 71](https://github.com/sandialabs/sceptre-bennu/pull/71))

- _(ci)_ add releasaurus release workflow and conventional commit checks [_(d348b03)_](https://github.com/sandialabs/sceptre-bennu/commit/d348b03a8046b4b4bc016a25044782b56661082e) ([PR 69](https://github.com/sandialabs/sceptre-bennu/pull/69))

- _(cmake)_ replace vendored precompiled libzmq with FetchContent [_(1907d2b)_](https://github.com/sandialabs/sceptre-bennu/commit/1907d2b3ad5951d56a924debb3a610f1a557db08) ([PR 62](https://github.com/sandialabs/sceptre-bennu/pull/62))

- _(lint)_ add lint configs and apply mechanical formatting [_(8a61e60)_](https://github.com/sandialabs/sceptre-bennu/commit/8a61e6009eff3e6b49215176300d34ac2b971d1e) ([PR 54](https://github.com/sandialabs/sceptre-bennu/pull/54))

### ⏩ CI/CD

- run formatting and lint hooks through prek [_(cf5f8f8)_](https://github.com/sandialabs/sceptre-bennu/commit/cf5f8f89f2fd9be194755a9f9e4f36eccf056237)

- _(lint)_ enforce lint in CI; docs for logging and testing [_(7d0e245)_](https://github.com/sandialabs/sceptre-bennu/commit/7d0e245e54c7034a816ebad436a5371b22951355)

- pin actions to commit SHAs and move test jobs to ubuntu-24.04 [_(3497d4d)_](https://github.com/sandialabs/sceptre-bennu/commit/3497d4db56659483254e80822614bb60f6f99f0d)

- _(tests)_ add unit tests to build-bennu job via ctest [_(c9c2cf6)_](https://github.com/sandialabs/sceptre-bennu/commit/c9c2cf603fe134997bbbd40979525836c02ffb0b) ([PR 53](https://github.com/sandialabs/sceptre-bennu/pull/53))

## Before automated releases

Release notes above this section are generated from the Conventional Commit
subjects on `main` when each release is made. The notes below were written by
hand before releases were automated.

## [release-20260930-620eb8d] - 2026-09-30

- Update editorconfig and repository cleanup

## [release-20260902-705f238] - 2026-09-02

- Bump pyzmq version to fix build

## [release-20260306-b5f1380] - 2026-03-06

- Documentation updates for sceptre-docs

## [release-20251201-5e12497] - 2025-12-01

- pybennu Makefile cleanup

## [release-20251113-4b1f4a4] - 2025-11-13

- Initial CI build workflow update
