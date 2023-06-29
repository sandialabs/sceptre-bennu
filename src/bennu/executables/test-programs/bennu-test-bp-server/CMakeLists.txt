include_directories(
  ${bennu_INCLUDES}
)

link_directories(
  ${Boost_LIBRARY_DIRS}
)

add_executable(bennu-test-bp-server
  main.cpp
)

target_link_libraries(bennu-test-bp-server
  ${Boost_LIBRARIES}
  bennu-distributed
)

install(TARGETS bennu-test-bp-server
  RUNTIME DESTINATION bin
)

