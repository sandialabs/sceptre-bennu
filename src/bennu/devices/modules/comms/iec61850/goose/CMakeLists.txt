include_directories(
  ${bennu_INCLUDES}
)

link_directories(
  ${Boost_LIBRARY_DIRS}
)

add_definitions(-DBOOST_ALL_NO_LIB -DBOOST_ALL_DYN_LINK)

set(bennu-iec61850-goose_HEADERS
  IEC61850RTU.hpp
  IEC61850RtuDataHandler.hpp
)

set(bennu-iec61850-goose_SOURCES
  IEC61850RTU.cpp
  IEC61850RtuDataHandler.cpp
)

add_library(bennu-iec61850-goose SHARED
  ${bennu-iec61850-goose_HEADERS}
  ${bennu-iec61850-goose_SOURCES}
)

target_link_libraries(bennu-iec61850-goose
  ${Boost_LIBRARIES}
  bennu-iec61850-protocol
  ccss-devices-iec61850-outstation
  bennu-gryffin-lib
)

set_target_properties(bennu-iec61850-goose
  PROPERTIES
  VERSION ${bennu_VERSION}
  SOVERSION ${bennu_VERSION_MAJOR}
)

install(TARGETS bennu-iec61850-goose
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
