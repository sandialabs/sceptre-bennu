include_directories(
  ${bennu_INCLUDES}
)

link_directories(
  ${Boost_LIBRARY_DIRS}
)

file(GLOB_RECURSE bennu-brash-server_SRC *.cpp *.hpp)

add_library(bennu-brash-server SHARED
  ${bennu-brash-server_SRC}
)

target_link_libraries(bennu-brash-server
  ${Boost_LIBRARIES}
)

set_target_properties(bennu-brash-server
  PROPERTIES
  VERSION ${bennu_VERSION}
  SOVERSION ${bennu_VERSION_MAJOR}
)

install(TARGETS bennu-brash-server
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
