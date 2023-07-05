include_directories(
  ${bennu_INCLUDES}
  ../protocol/config
  ../protocol/src/inc/api
  ../protocol/src/inc/internal
  ../protocol/src/common/inc
  ../protocol/src/hal/inc
)

link_directories(
  ${Boost_LIBRARY_DIRS}
)

add_definitions(-DBOOST_ALL_NO_LIB -DBOOST_ALL_DYN_LINK)

file(GLOB_RECURSE bennu-iec60870-5-module_SRC *.cpp *.hpp)

add_library(bennu-iec60870-5-module SHARED
  ${bennu-iec60870-5-module_SRC}
)

target_link_libraries(bennu-iec60870-5-module
  lib60870-shared
)

set_target_properties(bennu-iec60870-5-module
  PROPERTIES
  VERSION ${bennu_VERSION}
  SOVERSION ${bennu_VERSION_MAJOR}
)

install(TARGETS bennu-iec60870-5-module
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
