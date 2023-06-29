file(GLOB_RECURSE bacnet_SRC bacnet/*.c bacnet/*.h)
file(GLOB_RECURSE bacnet_SRC_PORT ports/linux/*.c ports/linux/*.h)

add_library(bacnet SHARED
  ../module/Wrapper.h
  ${bacnet_SRC}
  ${bacnet_SRC_PORT}
)

target_include_directories(bacnet PUBLIC
  ./
  ./ports/linux
)

set_target_properties(bacnet PROPERTIES LANGUAGE CXX)

install(TARGETS bacnet
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

target_compile_options(bacnet PRIVATE -Wno-stringop-overflow)
