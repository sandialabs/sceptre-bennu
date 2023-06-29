find_package(OpenSSL)

include_directories(
  ${bennu_INCLUDES}
  ${OPENSSL_INCLUDE_DIRS}
)

link_directories(
  ${Boost_LIBRARY_DIRS}
)

add_executable(bennu-watcherd
  picosha2.h
  main.cpp
)

target_link_libraries(bennu-watcherd
  ${OPENSSL_LIBRARIES}
  ${Boost_LIBRARIES}
)

install(TARGETS bennu-watcherd
  RUNTIME DESTINATION bin
)
