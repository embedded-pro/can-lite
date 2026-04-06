add_library(can_lite.transport_test_util INTERFACE)

target_sources(can_lite.transport_test_util INTERFACE
    IsoTpTestHelper.hpp
)

target_include_directories(can_lite.transport_test_util INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
)

target_link_libraries(can_lite.transport_test_util INTERFACE
    can_lite.test_util
    can_lite.transport
    can_lite.iso_tp
    gmock
)

add_executable(can_lite.transport_test)
emil_build_for(can_lite.transport_test HOST All BOOL CAN_LITE_BUILD_TESTS)
emil_add_test(can_lite.transport_test)

target_link_libraries(can_lite.transport_test PRIVATE
    can_lite.transport_test_util
    infra.timer_test_helper
    gtest_main
)

target_sources(can_lite.transport_test PRIVATE
    TestIsoTpChannel.cpp
    TestIsoTpFrameCodec.cpp
    TestIsoTpReceiver.cpp
    TestIsoTpSender.cpp
    TestIsoTpTransportImpl.cpp
)
