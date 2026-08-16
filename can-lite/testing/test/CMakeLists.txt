add_executable(can_lite.testing_test)
emil_build_for(can_lite.testing_test HOST All BOOL CAN_LITE_BUILD_TESTS)
emil_add_test(can_lite.testing_test)

target_link_libraries(can_lite.testing_test PUBLIC
    gmock_main
    can_lite.testing
    can_lite.server
    can_lite.client
    can_lite.transport
    can_lite.test_util
    infra.timer_test_helper
)

target_sources(can_lite.testing_test PRIVATE
    TestEchoCategory.cpp
)
