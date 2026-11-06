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

target_link_libraries(test_unit_distributed bennu-distributed bennu-utility ${Boost_LIBRARIES})
target_link_libraries(test_unit_distributed_udp bennu-distributed bennu-utility ${Boost_LIBRARIES})
target_link_libraries(test_unit_parsers bennu-parsers bennu-utility ${Boost_LIBRARIES})
target_link_libraries(test_unit_config_conformance bennu-parsers bennu-utility ${Boost_LIBRARIES})
# these tests read data/configs from the source tree
target_compile_definitions(test_unit_config_conformance PRIVATE BENNU_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_compile_definitions(test_unit_helics_federate PRIVATE BENNU_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(test_unit_datastore ${Boost_LIBRARIES})
target_link_libraries(test_unit_utility bennu-loaders bennu-utility ${Boost_LIBRARIES})
target_link_libraries(test_unit_loader bennu-loaders bennu-utility dl ${Boost_LIBRARIES})

target_include_directories(test_unit_modbus PRIVATE
  ${bennu_INCLUDES}
  ${CMAKE_SOURCE_DIR}/src/bennu/devices/modules/comms/modbus/module
  ${CMAKE_SOURCE_DIR}/src/bennu/devices/modules/comms/modbus/protocol
)
target_link_libraries(test_unit_modbus -Wl,--no-as-needed bennu-modbus-tcp bennu-modbus-protocol bennu-field-device-base bennu-comms bennu-utility ${Boost_LIBRARIES})

target_include_directories(test_unit_helics_federate PRIVATE
  ${CMAKE_SOURCE_DIR}/src/bennu/executables/bennu-simulink-provider-helics
)
target_link_libraries(test_unit_helics_federate HELICS::helics-shared bennu-utility ${Boost_LIBRARIES})

target_include_directories(test_integration_probe_provider PRIVATE ${bennu_INCLUDES})
target_link_libraries(test_integration_probe_provider bennu-distributed bennu-field-device-base bennu-utility ${ZMQ_LIB} pthread ${Boost_LIBRARIES})

target_include_directories(test_unit_helics_logic PRIVATE
  ${CMAKE_SOURCE_DIR}/src/bennu/executables/bennu-simulink-provider-helics
)
target_sources(test_unit_helics_logic PRIVATE $<TARGET_OBJECTS:helics-cparse-obj>)
target_link_libraries(test_unit_helics_logic ${Boost_LIBRARIES})

target_include_directories(test_unit_dnp3_points PRIVATE
  ${bennu_INCLUDES}
  ${CMAKE_SOURCE_DIR}/src/bennu/devices/modules/comms/dnp3/protocol/cpp/lib/include
  ${CMAKE_SOURCE_DIR}/src/bennu/devices/modules/comms/dnp3/module
)
target_link_libraries(test_unit_dnp3_points -Wl,--no-as-needed bennu-field-device-base bennu-comms bennu-utility bennu-dnp3-module opendnp3 ${Boost_LIBRARIES})

target_include_directories(test_fd_104_sbo PRIVATE
  ${bennu_INCLUDES}
  ${CMAKE_SOURCE_DIR}/src/bennu/devices/modules/comms/iec60870-5/protocol/config
  ${CMAKE_SOURCE_DIR}/src/bennu/devices/modules/comms/iec60870-5/protocol/src/inc/api
  ${CMAKE_SOURCE_DIR}/src/bennu/devices/modules/comms/iec60870-5/protocol/src/inc/internal
  ${CMAKE_SOURCE_DIR}/src/bennu/devices/modules/comms/iec60870-5/protocol/src/common/inc
  ${CMAKE_SOURCE_DIR}/src/bennu/devices/modules/comms/iec60870-5/protocol/src/hal/inc
)
target_link_libraries(test_fd_104_sbo -Wl,--no-as-needed bennu-field-device-base bennu-comms bennu-utility bennu-iec60870-5-module lib60870-shared)
