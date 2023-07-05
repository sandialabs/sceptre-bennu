include_directories(
  ${bennu_INCLUDES}
)

link_directories(
  ${Boost_LIBRARY_DIRS}
)

add_definitions(-DBOOST_ALL_NO_LIB -DBOOST_ALL_DYN_LINK)

file(GLOB_RECURSE bennu-modbus-tcp_SRC *.cpp *.hpp)

add_library(bennu-modbus-tcp SHARED
  ${bennu-modbus-tcp_SRC}
)

target_link_libraries(bennu-modbus-tcp
  ${Boost_LIBRARIES}
  bennu-modbus-protocol
  bennu-comms
)

set_target_properties(bennu-modbus-tcp
  PROPERTIES
  VERSION ${bennu_VERSION}
  SOVERSION ${bennu_VERSION_MAJOR}
)

install(TARGETS bennu-modbus-tcp
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
