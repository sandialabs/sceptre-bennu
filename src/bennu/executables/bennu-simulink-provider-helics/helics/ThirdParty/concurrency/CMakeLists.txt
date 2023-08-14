# ~~~
# Copyright (c) 2017-2019, Battelle Memorial Institute; Lawrence Livermore
# National Security, LLC; Alliance for Sustainable Energy, LLC.
# See the top-level NOTICE for additional details.
# All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
# ~~~

cmake_minimum_required(VERSION 3.4)
cmake_policy(VERSION 3.4)

project (GMLC_CONCURRENCY VERSION 0.2.1)

# -----------------------------------------------------------------------------
# GMLC concurrency library Version number
#-----------------------------------------------------------------------------
set (GMLC_CONCURRENCY_VERSION_BUILD )
set (GMLC_CONCURRENCY_DATE "05-26-21")

set(GMLC_CONCURRENCY_VERSION_STRING "${GMLC_CONCURRENCY_VERSION} (${GMLC_CONCURRENCY_DATE})")

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    # User settable
    set(concurrency_CXX_STD "14"  CACHE STRING "The CMake standard to require")

  #  set(CUR_PROJ ON)
    set(CMAKE_CXX_STANDARD ${CONTAINER_CXX_STD})
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

if (NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 14)
endif()
# -----------------------------------------------------------------------------
# set the module path and include some common macros
# -----------------------------------------------------------------------------
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/config/cmake/")
include(extraMacros)
include(CMakeDependentOption)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Prohibit in-source build
if("${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}")
  message(
    FATAL_ERROR
      "In-source build is not supported. Please, use an empty directory for building the project."
  )
endif()

add_library(concurrency_base INTERFACE)

include(compiler_flags)
message(STATUS "setting version build options to \"${CXX_STANDARD_FLAG}\"")
if(${CXX_STANDARD_FLAG})
  if(MSVC)
		target_compile_options(concurrency_base INTERFACE ${CXX_STANDARD_FLAG})
	else()
    target_compile_options(
      concurrency_base
      INTERFACE $<$<COMPILE_LANGUAGE:CXX>:${CXX_STANDARD_FLAG}}>
    )
  endif(MSVC)
endif(${CXX_STANDARD_FLAG})


# -------------------------------------------------------------
# add threading support
# -------------------------------------------------------------
if(MSYS)
	set(THREADS_PREFER_PTHREAD_FLAG ON)
endif()
find_package(Threads QUIET)

if (TARGET Threads::Threads)
  target_link_libraries(concurrency_base INTERFACE Threads::Threads)
#else()
#  SET(GMLC_CONCURRENCY_BENCHMARK OFF)
endif()

target_include_directories(concurrency_base INTERFACE ThirdParty)

add_subdirectory(gmlc)

option(GMLC_CONCURRENCY_TEST "Enable tests for the concurrency library" ON)
#option(GMLC_CONCURRENCY_BENCHMARK "Enable benchmarks for the concurrency library" ON)

if (GMLC_CONCURRENCY_TEST)
	enable_testing()
	add_subdirectory(tests)
endif()

if (GMLC_CONCURRENCY_BENCHMARK)
    add_subdirectory(benchmarks)
endif()

# -------------------------------------------------------------
# Get some configuration for C++17 as that becomes available
# -------------------------------------------------------------
# message(STATUS ${CMAKE_CXX_FLAGS})

option(GMLC_CONCURRENCY_GENERATE_DOXYGEN_DOC "Generate Doxygen doc target" OFF)

if(GMLC_CONCURRENCY_GENERATE_DOXYGEN_DOC)
	find_package(Doxygen)
	if(DOXYGEN_FOUND)

    show_variable(
      DOXYGEN_OUTPUT_DIR
      PATH
      "location to put Doxygen docs"
      "${PROJECT_BINARY_DIR}/docs"
    )
    configure_file(
      ${CMAKE_CURRENT_SOURCE_DIR}/config/Doxyfile.in
      ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY
    )
    add_custom_target(
      gmlc_concurrency_doc ${DOXYGEN_EXECUTABLE} ${PROJECT_BINARY_DIR}/Doxyfile
			WORKING_DIRECTORY ${DOXYGET_OUTPUT_DIR}
      COMMENT "Generating GMLC CONCURRENCY documentation with Doxygen"
      VERBATIM
		)
	endif(DOXYGEN_FOUND)
endif(GMLC_CONCURRENCY_GENERATE_DOXYGEN_DOC)


