include_directories(
  ${bennu_INCLUDES}
)

file(GLOB_RECURSE bennu-field-device_SRC *.cpp *.hpp)

add_library(bennu-field-device-base SHARED
  ${bennu-field-device_SRC}
)

target_link_libraries(bennu-field-device-base
  bennu-utility
  bennu-io-modules
  bennu-comms
  bennu-logic
)

set_target_properties(bennu-field-device-base
  PROPERTIES
  VERSION ${bennu_VERSION}
  SOVERSION ${bennu_VERSION_MAJOR}
)

install(TARGETS bennu-field-device-base
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
