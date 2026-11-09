add_executable(can_lite.examples.setpoint_test)
emil_build_for(can_lite.examples.setpoint_test HOST All BOOL CAN_LITE_BUILD_TESTS)
emil_add_test(can_lite.examples.setpoint_test)

target_link_libraries(can_lite.examples.setpoint_test PRIVATE
    can_lite.examples.setpoint
    can_lite.client
    can_lite.test_util
    infra.timer_test_helper
    gmock_main
)

target_sources(can_lite.examples.setpoint_test PRIVATE
    TestSetpointCategoryClient.cpp
    TestSetpointCategoryServer.cpp
)
