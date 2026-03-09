add_executable(can_lite.categories.foc_motor_test)
emil_build_for(can_lite.categories.foc_motor_test HOST All BOOL CAN_LITE_BUILD_TESTS)
emil_add_test(can_lite.categories.foc_motor_test)

target_link_libraries(can_lite.categories.foc_motor_test PRIVATE
    can_lite.categories.foc_motor
    can_lite.test_util
    gmock_main
)

target_sources(can_lite.categories.foc_motor_test PRIVATE
    TestFocMotorCategoryClient.cpp
    TestFocMotorCategoryServer.cpp
)
