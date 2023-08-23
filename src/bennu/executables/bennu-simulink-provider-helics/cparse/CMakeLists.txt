list(APPEND cparse_SRC
  Cparse.hpp
  builtin-features/functions.inc
  builtin-features/operations.inc
  builtin-features/operations.inc
  builtin-features/typeSpecificFunctions.inc
  builtin-features.inc
  containers.cpp
  containers.h
  functions.cpp
  functions.h
  packToken.cpp
  packToken.h
  shunting-yard.cpp
  shunting-yard.h
  shunting-yard-exceptions.h
)

add_library(helics-cparse-obj OBJECT
  ${cparse_SRC}
)

set_property(TARGET helics-cparse-obj PROPERTY POSITION_INDEPENDENT_CODE ON)

target_compile_options(helics-cparse-obj PRIVATE -Wno-reorder)
