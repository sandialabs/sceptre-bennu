include_directories(
  ${bennu_INCLUDES}
)

link_directories(
  ${Boost_LIBRARY_DIRS}
)

add_executable(bennu-probe
  main.cpp
)

target_link_libraries(bennu-probe
  ${Boost_LIBRARIES}
  bennu-distributed
)

install(TARGETS bennu-probe
  RUNTIME DESTINATION bin
)

