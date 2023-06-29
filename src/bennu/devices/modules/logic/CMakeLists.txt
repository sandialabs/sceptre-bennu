add_subdirectory(cparse)

include_directories(
  ${bennu_INCLUDES}
)

link_directories(
  ${Boost_LIBRARY_DIRS}
)

add_definitions(-DBOOST_ALL_NO_LIB -DBOOST_ALL_DYN_LINK)

list(APPEND bennu-logic_SRC
  LogicModule.cpp
  LogicModule.hpp
)

add_library(bennu-logic SHARED
  ${bennu-logic_SRC}
  $<TARGET_OBJECTS:cparse-obj>
)

target_link_libraries(bennu-logic
  ${Boost_LIBRARIES}
)

set_target_properties(bennu-logic
  PROPERTIES
  VERSION ${bennu_VERSION}
  SOVERSION ${bennu_VERSION_MAJOR}
)

install(TARGETS bennu-logic
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
