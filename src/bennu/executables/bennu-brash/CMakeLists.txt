include_directories(
  ${bennu_INCLUDES}
)

link_directories(
  ${Boost_LIBRARY_DIRS}
)

add_executable(bennu-brash
  main.cpp
)

target_link_libraries(bennu-brash
  ${Boost_LIBRARIES}
)

INSTALL(TARGETS bennu-brash
  RUNTIME DESTINATION bin
)

