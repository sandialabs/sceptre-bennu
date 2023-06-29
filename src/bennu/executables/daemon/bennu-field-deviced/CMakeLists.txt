include_directories(
  ${bennu_INCLUDES}
)

link_directories(
  ${Boost_LIBRARY_DIRS}
)

add_executable(bennu-field-deviced
  main.cpp
)

target_link_libraries(bennu-field-deviced
  ${Boost_LIBRARIES}
  bennu-parsers
  bennu-utility
  dl
  bennu-brash-server
)

install(TARGETS bennu-field-deviced
  RUNTIME DESTINATION bin
)
