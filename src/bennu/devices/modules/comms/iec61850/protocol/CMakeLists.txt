include_directories(
  ${bennu_INCLUDES}
)

link_directories(
  ${Boost_LIBRARY_DIRS}
)

set(iec61850_HEADERS
  time.hpp
  data-object.hpp
  common-data.hpp
  copy-policies.hpp
  common-acsi-types.hpp
  data-attribute.hpp
  logical-node.hpp
  logical-device.hpp
  object-reference-parser.hpp
  object-reference-builder.hpp
  basic-types/basic-types.hpp
  basic-types/basic-types-traits.hpp
  basic-types/basic-types-serialization.hpp
  goose/header.hpp
  goose/config.hpp
  goose/parser.hpp
  goose/message.hpp
  goose/data-set.hpp
  goose/session-options.hpp
  goose/application-layer.hpp
  goose/protocol_stack.hpp
  goose/pdu-offsets.hpp
  goose/gocb.hpp
  logical-nodes/lgos.hpp
  logical-nodes/lln0.hpp
  logical-nodes/xcbr.hpp
  common-data-classes/org.hpp
  common-data-classes/dpc.hpp
  common-data-classes/spc.hpp
  common-data-classes/ins.hpp
  common-data-classes/sps.hpp
)

set(iec61850_SOURCES
  logical-node.cpp
  logical-device.cpp
  object-reference-parser.cpp
  object-reference-builder.cpp
  goose/header.cpp
  goose/application-layer.cpp
  goose/protocol_stack.cpp
  logical-nodes/lgos.cpp
  logical-nodes/xcbr.cpp
  basic-types/basic-types.cpp
)

add_library(bennu-iec61850-protocol SHARED
  ${iec61850_HEADERS}
  ${iec61850_SOURCES}
)

set_target_properties(bennu-iec61850-protocol
  PROPERTIES
  VERSION ${bennu_VERSION}
  SOVERSION ${bennu_VERSION_MAJOR}
)

install(TARGETS bennu-iec61850-protocol
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
