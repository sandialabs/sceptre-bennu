include_directories(
  ${bennu_INCLUDES}
)

link_directories(
  ${Boost_LIBRARY_DIRS}
)

add_definitions(-DBOOST_ALL_NO_LIB -DBOOST_ALL_DYN_LINK)

file(GLOB_RECURSE bennu-distributed_SRC *.cpp *.hpp)

add_library(bennu-distributed SHARED
  ${bennu-distributed_SRC}
)

target_link_libraries(bennu-distributed
  ${Boost_LIBRARIES}
  ${ZMQ_LIB}
)

set_target_properties(bennu-distributed
  PROPERTIES
  VERSION ${bennu_VERSION}
  SOVERSION ${bennu_VERSION_MAJOR}
)

install(TARGETS bennu-distributed
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
