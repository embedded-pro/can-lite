add_executable(can_lite.tracing_test)
emil_build_for(can_lite.tracing_test HOST All BOOL CAN_LITE_BUILD_TESTS)
emil_add_test(can_lite.tracing_test)

target_link_libraries(can_lite.tracing_test PRIVATE
    can_lite.tracing
    can_lite.tracing_iso_tp
    can_lite.tracing_protocol
    can_lite.test_util
    infra.timer_test_helper
    gtest_main
)

target_sources(can_lite.tracing_test PRIVATE
    TestTracingCan.cpp
    TestTracingCanProtocolObservers.cpp
    TestTracingIsoTpTransport.cpp
)
