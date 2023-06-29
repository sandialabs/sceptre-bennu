include_directories(
  ${bennu_INCLUDES}
)

link_directories(
  ${Boost_LIBRARY_DIRS}
)

add_executable(bennu-field-device
  main.cpp
)

target_link_libraries(bennu-field-device
  ${Boost_LIBRARIES}
  bennu-parsers
)

install(TARGETS bennu-field-device
  RUNTIME DESTINATION bin
)

