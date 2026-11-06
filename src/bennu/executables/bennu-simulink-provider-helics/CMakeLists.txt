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
