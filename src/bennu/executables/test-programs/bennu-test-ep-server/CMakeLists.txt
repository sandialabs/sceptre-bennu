include_directories(
  ${bennu_INCLUDES}
)

link_directories(
  ${Boost_LIBRARY_DIRS}
)

add_executable(bennu-test-ep-server
  main.cpp
)

target_link_libraries(bennu-test-ep-server
  ${Boost_LIBRARIES}
  bennu-distributed
)

install(TARGETS bennu-test-ep-server
  RUNTIME DESTINATION bin
)

