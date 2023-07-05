include_directories(
  ${bennu_INCLUDES}
)

link_directories(
  ${Boost_LIBRARY_DIRS}
)

add_executable(bennu-simulink-provider
  main.cpp
)

target_link_libraries(bennu-simulink-provider
  ${Boost_LIBRARIES}
  bennu-distributed
  bennu-utility
  bennu-parsers
)

install(TARGETS bennu-simulink-provider
  RUNTIME DESTINATION bin
)

