add_executable(can_lite.integration_tests.support_test)
emil_build_for(can_lite.integration_tests.support_test HOST All BOOL CAN_LITE_BUILD_TESTS)
emil_add_test(can_lite.integration_tests.support_test)

target_link_libraries(can_lite.integration_tests.support_test PRIVATE
    can_lite.integration_tests.support
    gtest_main
)

target_sources(can_lite.integration_tests.support_test PRIVATE
    TestVirtualCan.cpp
)
