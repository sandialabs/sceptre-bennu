add_library(main OBJECT _main.cpp)

file(GLOB files "test_*.cpp")
foreach (file ${files})
  get_filename_component(test ${file} NAME_WE)
  add_executable(${test} ${file} $<TARGET_OBJECTS:main>)
  add_test(NAME ${test} COMMAND ${test})
endforeach ()
