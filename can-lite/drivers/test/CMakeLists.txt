add_executable(can_lite.drivers_test)
emil_build_for(can_lite.drivers_test HOST All BOOL CAN_LITE_BUILD_TESTS)
emil_add_test(can_lite.drivers_test)

target_link_libraries(can_lite.drivers_test PRIVATE
    can_lite.drivers.codec
    can_lite.drivers.multi
    can_lite.drivers.slcan
    infra.event_test_helper
    gmock_main
)

target_sources(can_lite.drivers_test PRIVATE
    CanBusAdapterMock.hpp
    SerialPortMock.hpp
    TestGsUsbProtocol.cpp
    TestMultiCanAdapter.cpp
    TestSlcanAdapter.cpp
    TestSlcanCodec.cpp
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(can_lite.drivers_test PRIVATE can_lite.drivers.serial)
    target_sources(can_lite.drivers_test PRIVATE TestPosixSerialPort.cpp)
endif()
