include_directories(
  ${bennu_INCLUDES}
)

link_directories(
  ${Boost_LIBRARY_DIRS}
)

add_definitions(-DBOOST_ALL_NO_LIB -DBOOST_ALL_DYN_LINK)

file(GLOB_RECURSE bennu-utility_SRC *.cpp *.hpp)

add_library(bennu-utility SHARED
  ${bennu-utility_SRC}
)

target_link_libraries(bennu-utility
  ${Boost_LIBRARIES}
  bennu-parsers
)

set_target_properties(bennu-utility
  PROPERTIES
  VERSION ${bennu_VERSION}
  SOVERSION ${bennu_VERSION_MAJOR}
)

install(TARGETS bennu-utility
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

