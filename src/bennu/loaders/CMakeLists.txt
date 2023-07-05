include_directories(
  ${bennu_INCLUDES}
)

link_directories(
  ${Boost_LIBRARY_DIRS}
)

add_definitions(-DBOOST_ALL_NO_LIB -DBOOST_ALL_DYN_LINK)

file(GLOB_RECURSE bennu-loaders_SRC *.cpp *.hpp)

add_library(bennu-loaders SHARED
  ${bennu-loaders_SRC}
)

target_link_libraries(bennu-loaders
  ${Boost_LIBRARIES}
  dl
)

set_target_properties(bennu-loaders
  PROPERTIES
  VERSION ${bennu_VERSION}
  SOVERSION ${bennu_VERSION_MAJOR}
)

install(TARGETS bennu-loaders
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

