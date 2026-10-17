cmake_minimum_required (VERSION 3.12.0 FATAL_ERROR)

# Get the abbreviated commit hash of the working branch
execute_process(
  COMMAND git rev-parse --short HEAD
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  OUTPUT_VARIABLE bennu_SHORT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

# unix timestamp
string(TIMESTAMP BUILD_TIMESTAMP "%s")

# CMake requires semantic versions with integer components
# Set it to something static so stuff works
# Example: 6.0.1707507861
set(bennu_VERSION "6.0.${BUILD_TIMESTAMP}")

# Add commit hash to the version for use by the Deb package builder
# Example: 6.0.1707507861.94edd945
set(bennu_VERSION_WITH_COMMIT "${bennu_VERSION}.${bennu_SHORT_HASH}")

project(bennu
  VERSION "${bennu_VERSION}"
  DESCRIPTION "SCEPTRE ICS Modeling and Simulation Package"
  HOMEPAGE_URL "https://github.com/sandialabs/sceptre-bennu"
  LANGUAGES C CXX
)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
endif ()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # used by clang-tidy (make cpp-tidy)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -Wall")
set(CMAKE_SKIP_RPATH ON)
set(BUILD_SHARED_LIBS OFF)
add_definitions(-DBENNU_VERSION="${bennu_VERSION}")
add_definitions(-DZMQ_BUILD_DRAFT_API)
add_definitions(-Wno-deprecated)
# suppress boost warnings
add_definitions(-DBOOST_BIND_GLOBAL_PLACEHOLDERS)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(PreventInSourceBuilds)
include(UseCcache)
include(UseGoldLinker)
include(CpackConfig)
find_package(Boost REQUIRED filesystem system date_time serialization program_options)

include(FetchContent)

# Configure libzmq build options
set(ENABLE_DRAFTS ON CACHE BOOL "Build draft classes and methods" FORCE)
set(BUILD_SHARED OFF CACHE BOOL "Build shared library" FORCE)
set(BUILD_STATIC ON CACHE BOOL "Build static library" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "Build tests" FORCE)
set(WITH_DOCS OFF CACHE BOOL "Build documentation" FORCE)

FetchContent_Declare(
    libzmq
    GIT_REPOSITORY https://github.com/zeromq/libzmq.git
    GIT_TAG        v4.3.4
)
FetchContent_GetProperties(libzmq)
if(NOT libzmq_POPULATED)
  FetchContent_Populate(libzmq)
  add_subdirectory(${libzmq_SOURCE_DIR} ${libzmq_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

# libzmq target exports headers and static lib
set(ZMQ_LIB libzmq-static)
set(BUILD_TESTS ON CACHE BOOL "Build tests?" FORCE)

set(bennu_INCLUDES
  ${Boost_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/deps
)


add_subdirectory(src/bennu)

OPTION(BUILD_GOBENNU "Build Golang executable?" OFF)

if(BUILD_GOBENNU)
  add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/src/gobennu/bin/gobennu
    COMMAND make bin/gobennu
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/gobennu
  )

  add_custom_target(gobennu ALL DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/gobennu/bin/gobennu)

  install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/src/gobennu/bin/gobennu
    DESTINATION bin
  )
endif()

# Uninstall target
add_custom_target(uninstall
    "${CMAKE_COMMAND}" -P "${CMAKE_MODULE_PATH}/Uninstall.cmake"
)

option(BUILD_TESTS "Build tests?" ON)
if (BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif (BUILD_TESTS)
