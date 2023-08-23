# Contributors

This file describes the contributors to the Utilities library and the software used as part of this project It is part of the GMLC-TDC project and used in the HELICS library. HELICS is a joint project between PNNL, LLNL, and NREL, with contributions from many other national Labs
If you would like to contribute to the Utilities or HELICS project see [CONTRIBUTING](CONTRIBUTING.md)

## Individual contributors

### Pacific Northwest National Lab

### Lawrence Livermore National Lab

- Ryan Mast
- Philip Top

### National Renewable Energy Lab

- Dheepak Krishnamurthy

### Argonne National Lab

## Used Libraries or Code

### [HELICS](https://github.com/GMLC-TDC/HELICS)

Some of the original code for this library was pulled from use inside HELICS. It was pulled out as the utility libraries are not core to HELICS and it was useful as a standalone library so it could have better testing and separation of concerns. HELICS now uses the utilities library instead of maintaining its own code. HELICS is released with a BSD-3-Clause license.

### [GridDyn](https://github.com/LLNL/GridDyn)

Some of the original functions were pulled from GridDyn. It was pulled out as the utility libraries are not core to GridDyn and it was useful as a standalone library so it could have better testing and separation of concerns. GridDyn now uses the utilities library instead of maintaining its own code. GridDyn is released with a BSD-3-Clause license.

### [googleTest](https://github.com/google/googletest)

The tests are written to use google test and mock frameworks and is pulled in as a subproject via git clone or CMake FetchContent. Googletest is released with a BSD-3-clause licensed

### [BOOST](https://www.boost.org)

The Boost demangle function is included and modified to not use any other boost libraries. string_view type can use boost string_view if no other string_view is available, and the string_viewConversion function can use boost::spirit to help with the conversion of floating point types. Boost is licensed under the Boost Software License.

### cmake scripts

Several cmake scripts came from other sources and were either used or modified for use in HELICS.

- Lars Bilke [CodeCoverage.cmake](https://github.com/bilke/cmake-modules/blob/master/CodeCoverage.cmake)
- CLI11 [CLI11](https://github.com/CLIUtils/CLI11) while CLI11 was not used directly many of the CI scripts and structure were borrowed to set up the CI builds.
