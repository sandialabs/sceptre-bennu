include_directories(
  ${bennu_INCLUDES}
  ../protocol/cpp/libs/include
)

link_directories(
  ${Boost_LIBRARY_DIRS}
)

add_definitions(-DBOOST_ALL_NO_LIB -DBOOST_ALL_DYN_LINK)

file(GLOB_RECURSE bennu-dnp3-module_SRC *.cpp *.hpp)

add_library(bennu-dnp3-module SHARED
  ${bennu-dnp3-module_SRC}
)

target_link_libraries(bennu-dnp3-module
  ${Boost_LIBRARIES}
  opendnp3
  bennu-comms
)

set_target_properties(bennu-dnp3-module
  PROPERTIES
  VERSION ${bennu_VERSION}
  SOVERSION ${bennu_VERSION_MAJOR}
)

install(TARGETS bennu-dnp3-module
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
