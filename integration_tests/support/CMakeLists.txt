add_library(can_lite.integration_tests.support STATIC
    ApplicationFixture.cpp
)

target_include_directories(can_lite.integration_tests.support PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(can_lite.integration_tests.support PUBLIC
    can_lite.server
    can_lite.client
    can_lite.testing
    can_lite.core
    infra.timer
    infra.timer_test_helper
    GTest::gmock
)
