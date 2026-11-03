add_library(main OBJECT _main.cpp)

# integration tests drive installed binaries; unit tests are pure and built separately
file(GLOB files "test_*.cpp")
file(GLOB unit_files "test_unit_*.cpp")
list(REMOVE_ITEM files ${unit_files})
foreach (file ${files})
  get_filename_component(test ${file} NAME_WE)
  add_executable(${test} ${file} $<TARGET_OBJECTS:main>)
  add_test(NAME ${test} COMMAND ${test})
endforeach ()

foreach (file ${unit_files})
  get_filename_component(test ${file} NAME_WE)
  add_executable(${test} ${file} $<TARGET_OBJECTS:main>)
  target_include_directories(${test} PRIVATE ${bennu_INCLUDES})
  target_link_libraries(${test} ${ZMQ_LIB} pthread)
  add_test(NAME ${test} COMMAND ${test})
endforeach ()

# OutputModule unit test runs against an injected sender; Client.cpp only satisfies start()
target_sources(test_unit_outputmodule PRIVATE
  ${CMAKE_SOURCE_DIR}/src/bennu/devices/modules/io/OutputModule.cpp
  ${CMAKE_SOURCE_DIR}/src/bennu/devices/modules/io/IOModule.cpp
  ${CMAKE_SOURCE_DIR}/src/bennu/distributed/Client.cpp)

# perf guards exercise the real Client over inproc zmq
target_sources(test_unit_perf PRIVATE ${CMAKE_SOURCE_DIR}/src/bennu/distributed/Client.cpp)

target_include_directories(test_fd_104_sbo PRIVATE
  ${bennu_INCLUDES}
  ${CMAKE_SOURCE_DIR}/src/bennu/devices/modules/comms/iec60870-5/protocol/config
  ${CMAKE_SOURCE_DIR}/src/bennu/devices/modules/comms/iec60870-5/protocol/src/inc/api
  ${CMAKE_SOURCE_DIR}/src/bennu/devices/modules/comms/iec60870-5/protocol/src/inc/internal
  ${CMAKE_SOURCE_DIR}/src/bennu/devices/modules/comms/iec60870-5/protocol/src/common/inc
  ${CMAKE_SOURCE_DIR}/src/bennu/devices/modules/comms/iec60870-5/protocol/src/hal/inc
)
target_link_libraries(test_fd_104_sbo -Wl,--no-as-needed bennu-field-device-base bennu-comms bennu-utility bennu-iec60870-5-module lib60870-shared)
