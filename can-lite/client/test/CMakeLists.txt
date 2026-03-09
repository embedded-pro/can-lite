add_executable(can_lite.client_test)
emil_build_for(can_lite.client_test HOST All BOOL CAN_LITE_BUILD_TESTS)
emil_add_test(can_lite.client_test)

target_link_libraries(can_lite.client_test PRIVATE
    can_lite.client
    can_lite.test_util
    gmock_main
)

target_sources(can_lite.client_test PRIVATE
    TestCanProtocolClient.cpp
)
