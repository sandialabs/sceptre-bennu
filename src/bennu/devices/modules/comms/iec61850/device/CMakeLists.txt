include_directories(
  ${bennu_INCLUDES}
)

set(iec61850-device_HEADERS
  attribute-map.hpp
  outstation.hpp
  subscriber.hpp
  publisher.hpp
)

set(iec61850-device_SOURCES
  attribute-map.cpp
  subscriber.cpp
  publisher.cpp
)

add_library(bennu-iec61850-device SHARED
  ${iec61850-device_HEADERS}
  ${iec61850-device_SOURCES}
)

target_link_libraries(bennu-iec61850-device
  bennu-iec61850-protocol
)

set_target_properties(bennu-iec61850-device
  PROPERTIES
  VERSION ${bennu_VERSION}
  SOVERSION ${bennu_VERSION_MAJOR}
)

install(TARGETS bennu-iec61850-device
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
