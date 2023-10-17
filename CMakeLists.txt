cmake_minimum_required (VERSION 3.8.0 FATAL_ERROR)

# Get the latest tag of the working branch
execute_process(
  COMMAND git describe --abbrev=0
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  OUTPUT_VARIABLE bennu_TAG
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get the latest tag and abbreviated commit hash of the working branch
execute_process(
  COMMAND git rev-parse --short HEAD
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  OUTPUT_VARIABLE bennu_TAG_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

project(bennu VERSION ${bennu_TAG} LANGUAGES C CXX)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
endif ()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -Wall")
set(CMAKE_SKIP_RPATH ON)
set(BUILD_SHARED_LIBS OFF)
add_definitions(-DBENNU_VERSION="${bennu_VERSION}")
add_definitions(-DZMQ_BUILD_DRAFT_API)
add_definitions(-Wno-deprecated)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(PreventInSourceBuilds)
include(UseCcache)
include(UseGoldLinker)
include(CpackConfig)
find_package(Boost REQUIRED filesystem system date_time serialization program_options)

set(ZMQ_DIR ${CMAKE_SOURCE_DIR}/src/deps/zmq)
set(ZMQ_LIB ${ZMQ_DIR}/lib/libzmq.a)

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
