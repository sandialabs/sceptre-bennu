# The vendored HELICS tree (and the third-party code it bundles) declares cmake_minimum_required < 3.5, which CMake >= 4.0 rejects.
# Raise the floor for the whole configure (CMake 4 variable; ignored by older CMake) rather than editing every
# vendored CMakeLists. NOTE: the vendored tree is also patched for newer toolchains (missing <cstdint>/<cstddef>-style
# includes in libguarded/handles.hpp, stringConversion.h, IpcQueueHelper.cpp); reapply those on a HELICS re-vendor.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
set(HELICS_INSTALL OFF)
set(HELICS_BUILD_APP_LIBRARY OFF)
set(HELICS_BUILD_APP_EXECUTABLES OFF)
set(HELICS_BUILD_CXX_SHARED_LIB ON)
set(HELICS_USE_SYSTEM_ZEROMQ_ONLY ON)
set(HELICS_USE_ZMQ_STATIC_LIBRARY ON)
add_subdirectory(helics)
add_subdirectory(cparse)

include_directories(
  ${bennu_INCLUDES}
)

link_directories(
  ${Boost_LIBRARY_DIRS}
)

add_definitions(-DBOOST_ALL_NO_LIB -DBOOST_ALL_DYN_LINK)

list(APPEND bennu-simulink-provider-helics_SRC
  main.cpp
  HelicsHelper.hpp
  Logic.hpp
)

add_executable(bennu-simulink-provider-helics
  ${bennu-simulink-provider-helics_SRC}
  $<TARGET_OBJECTS:helics-cparse-obj>
)

target_link_libraries(bennu-simulink-provider-helics
  HELICS::helics-shared
)

install(TARGETS bennu-simulink-provider-helics
  RUNTIME DESTINATION bin
)
