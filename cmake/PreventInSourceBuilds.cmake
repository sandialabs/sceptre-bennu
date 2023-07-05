# Prevent building in the source tree.
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR "
    ERROR: CMake generation is not allowed within the source directory!

    You *MUST* remove the generated CMake files and try again
    from another directory:

        rm -r CMakeCache.txt CMakeFiles/
        mkdir build
        cd build
        cmake ..
")
endif()
