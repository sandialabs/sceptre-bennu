include_directories(
  ${bennu_INCLUDES}
)

file(GLOB_RECURSE bennu-comms_SRC *.cpp *.hpp)

add_library(bennu-comms SHARED
  ${bennu-comms_SRC}
  StatusMessage.h
)

target_link_libraries(bennu-comms
  bennu-io-modules
)

set_target_properties(bennu-comms
  PROPERTIES
  VERSION ${bennu_VERSION}
  SOVERSION ${bennu_VERSION_MAJOR}
)

install(TARGETS bennu-comms
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

