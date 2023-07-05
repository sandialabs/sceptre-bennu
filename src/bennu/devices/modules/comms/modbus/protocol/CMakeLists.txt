include_directories(
  ${bennu_INCLUDES}
)

link_directories(
  ${Boost_LIBRARY_DIRS}
)

set(modbus_HEADERS
  application-layer.hpp
  application-callbacks.hpp
  session-options.hpp
  protocol-stack.hpp
  constants.hpp
  function-codes.hpp
  mbap-header.hpp
  error-codes.hpp
  types.hpp
  objects.hpp
)

set(modbus_SOURCES
  application-layer.cpp
  protocol-stack.cpp
)

add_library(bennu-modbus-protocol SHARED
  ${modbus_HEADERS}
  ${modbus_SOURCES}
)

set_target_properties(bennu-modbus-protocol
  PROPERTIES
  VERSION ${bennu_VERSION}
  SOVERSION ${bennu_VERSION_MAJOR}
)

install(TARGETS bennu-modbus-protocol
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
