include_directories(
  ${bennu_INCLUDES}
  ../protocol
  ../protocol/ports/linux
)

link_directories(
  ${Boost_LIBRARY_DIRS}
)

add_definitions(-DBOOST_ALL_NO_LIB -DBOOST_ALL_DYN_LINK)

file(GLOB_RECURSE bennu-bacnet-module_SRC *.cpp *.hpp *.h)

add_library(bennu-bacnet-module SHARED
  ${bennu-bacnet-module_SRC}
)

target_link_libraries(bennu-bacnet-module
  ${Boost_LIBRARIES}
  bacnet
  bennu-comms
)

set_target_properties(bennu-bacnet-module
  PROPERTIES
  VERSION ${bennu_VERSION}
  SOVERSION ${bennu_VERSION_MAJOR}
)

install(TARGETS bennu-bacnet-module
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
