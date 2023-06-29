include_directories(
  ${bennu_INCLUDES}
)

link_directories(
  ${Boost_LIBRARY_DIRS}
)

add_definitions(-DBOOST_ALL_NO_LIB -DBOOST_ALL_DYN_LINK)

file(GLOB_RECURSE bennu-parsers_SRC *.cpp *.hpp)

add_library(bennu-parsers SHARED
  ${bennu-parsers_SRC}
)

target_link_libraries(bennu-parsers
  ${Boost_LIBRARIES}
  bennu-loaders
)

set_target_properties(bennu-parsers
  PROPERTIES
  VERSION ${bennu_VERSION}
  SOVERSION ${bennu_VERSION_MAJOR}
)

install(TARGETS bennu-parsers
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
