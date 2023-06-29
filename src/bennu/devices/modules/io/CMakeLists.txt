include_directories(
  ${bennu_INCLUDES}
)

link_directories(
  ${Boost_LIBRARY_DIRS}
)

add_definitions(-DBOOST_ALL_NO_LIB -DBOOST_ALL_DYN_LINK)

file(GLOB_RECURSE bennu-io-modules_SRC *.cpp *.hpp)

add_library(bennu-io-modules SHARED
  ${bennu-io-modules_SRC}
)

target_link_libraries(bennu-io-modules
  ${Boost_LIBRARIES}
  bennu-distributed
)

set_target_properties(bennu-io-modules
  PROPERTIES
  VERSION ${bennu_VERSION}
  SOVERSION ${bennu_VERSION_MAJOR}
)

install(TARGETS bennu-io-modules
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
