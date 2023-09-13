# Contributors
This file describes the contributors to the Concurrency library and the software used as part of this project It is part of the GMLC-TDC project and used in the HELICS library.  HELICS is a joint project between PNNL, LLNL, and NREL, with contributions from many other national Labs
If you would like to contribute to the Concurrency or HELICS project see [CONTRIBUTING](CONTRIBUTING.md)
## Individual contributors
### Pacific Northwest National Lab

### Lawrence Livermore National Lab
-   Ryan Mast
-   Philip Top

### National Renewable Energy Lab
-   Dheepak Krishnamurthy

### Argonne National Lab

## Used Libraries or Code

### [HELICS](https://github.com/GMLC-TDC/HELICS-src)  
Most of the original code for this library was pulled from use inside HELICS.  It was pulled out as the containers are not core to HELICS and it was useful as a standalone library so it could have better testing and separation of concerns.  HELICS is released with a BSD-3-Clause license.

### [libGuarded](https://github.com/copperspice/libguarded)
Several of the structures are copied or derived from libguarded. The library was modified to allow use of std::mutex and std::timed_mutex support for the shared_guarded, ordered_guarded class, and also modified to use handles to support unlocking and copying. An atomic_guarded and a staged_guarded class were added for std::atomic like support for allocating classes. libGuarded is licensed under [BSD 2 clause](https://github.com/copperspice/libguarded/blob/master/LICENSE).

### [googleTest](https://github.com/google/googletest)  
  The tests are written to use google test and mock frameworks and is included as a submodule.  Googletest is released with a BSD-3-clause licensed

### [googlebenchmark](https://github.com/google/benchmark)  
Optional benchmarks for the containers library are included using Google Benchmark. If desired google benchmarks is downloaded as a submodule and built with the project.  Google benchmark is released with an [Apache 2.0](https://github.com/google/benchmark/blob/master/LICENSE) license.

### [c++17 headers](https://github.com/tcbrindle/cpp17_headers)
The containers library makes use of `C++17` headers, but due to `C++14` compatibility requirements these are not available on all supported compilers.  So included library headers are used from @tcbrindle specifically std::optional  These fall under the boost license, this library is an aggregate from a number of different sources, see the readme at the above link for more details.  The Boost versions of these libraries are not used due to incompatibilities through different boost versions that HELICS supports, so a single stable source was used.

### cmake scripts
Several cmake scripts came from other sources and were either used or modified for use in HELICS.
-   Lars Bilke [CodeCoverage.cmake](https://github.com/bilke/cmake-modules/blob/master/CodeCoverage.cmake)
-   CLI11 [CLI11](https://github.com/CLIUtils/CLI11)  while CLI11 was not used directly many of the CI scripts and structure were borrowed to set up the CI builds.  
