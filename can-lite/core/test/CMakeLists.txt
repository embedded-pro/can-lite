add_library(can_lite.test_util INTERFACE)

target_sources(can_lite.test_util INTERFACE
    CanMock.hpp
)

target_include_directories(can_lite.test_util INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
)

target_link_libraries(can_lite.test_util INTERFACE
    hal.interfaces
    gmock
)

add_executable(can_lite.core_test)
emil_build_for(can_lite.core_test HOST All BOOL CAN_LITE_BUILD_TESTS)
emil_add_test(can_lite.core_test)

target_link_libraries(can_lite.core_test PRIVATE
    can_lite.core
    gtest_main
)

target_sources(can_lite.core_test PRIVATE
    TestCanProtocolCore.cpp
)
