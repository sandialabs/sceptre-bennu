# Contributors
This file describes the contributors to the Containers library and the software used as part of this project It is part of the GMLC-TDC project and used in the HELICS library.  HELICS is a joint project between PNNL, LLNL, and NREL, with contributions from many other national Labs
If you would like to contribute to the Containers or HELICS project see [CONTRIBUTING](CONTRIBUTING.md)
## Individual contributors
### Pacific Northwest National Lab

### Lawrence Livermore National Lab
 - Ryan Mast*
 - Philip Top*

### National Renewable Energy Lab
 - Dheepak Krishnamurthy*

### Argonne National Lab

 `*` currently active
 `**` subcontractor

## Used Libraries or Code
### [HELICS](https://github.com/GMLC-TDC/HELICS)  
Most of the original code for this library was pulled from use inside HELICS.  It was pulled out as the containers are not core to HELICS and it was useful as a standalone library so it could have better testing and separation of concerns.  HELICS is released with a BSD-3-Clause license.

### [googleTest](https://github.com/google/googletest)  
  The tests are written to use google test and mock frameworks and is included as a submodule.  Googletest is released with a BSD-3-clause licensed

### [googlebenchmark](https://github.com/google/benchmark/blob/master/LICENSE)  
Optional benchmarks for the containers library are included using Google Benchmark. If desired google benchmarks is downloaded as a submodule and built with the project.  Google benchmark is released with an Apache 2.0 license.

### [moodycamel](https://github.com/ikiller1/moodycamel-ConcurrentQueue)
As part of the benchmark tests one of the comparisons is against the moodycamel concurrentQueue. The queue code is included in the test folder and is released with  simplified BSD license.

### [BOOST](https://www.boost.org)
  Boost is used throughout the code.  The boost optional library can optionally be used. Boost is licensed under the boost license.  And some of the benchmark comparisons are done with boost lock-free queues.

### [c++17 headers](https://github.com/tcbrindle/cpp17_headers)
The containers library makes use of `C++17` headers, but due to `C++14` compatibility requirements these are not available on all supported compilers.  So included library headers are used from @tcbrindle specifically std::optional  These fall under the boost license, this library is an aggregate from a number of different sources, see the readme at the above link for more details.  The Boost versions of these libraries are not used due to incompatibilities through different boost versions that HELICS supports, so a single stable source was used.

### cmake scripts
Several cmake scripts came from other sources and were either used or modified for use in HELICS.
 - Lars Bilke [CodeCoverage.cmake](https://github.com/bilke/cmake-modules/blob/master/CodeCoverage.cmake)
 - CLI11 [CLI11](https://github.com/CLIUtils/CLI11)  while CLI11 was not used directly many of the CI scripts and structure were borrowed to set up the CI builds.  
