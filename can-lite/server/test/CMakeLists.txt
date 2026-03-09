add_executable(can_lite.server_test)
emil_build_for(can_lite.server_test HOST All BOOL CAN_LITE_BUILD_TESTS)
emil_add_test(can_lite.server_test)

target_link_libraries(can_lite.server_test PUBLIC
    gmock_main
    can_lite.server
    can_lite.test_util
    infra.timer_test_helper
)

target_sources(can_lite.server_test PRIVATE
    TestCanProtocolServer.cpp
)
