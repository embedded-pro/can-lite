add_library(can_lite.integration_tests.steps OBJECT
    CanBusSteps.cpp
    CategoryDiscoverySteps.cpp
    ErrorHandlingSteps.cpp
    FirmwareUpgradeFixture.cpp
    FirmwareUpgradeSteps.cpp
    RateLimitingSteps.cpp
    SequenceValidationSteps.cpp
    SystemCategorySteps.cpp
)

target_include_directories(can_lite.integration_tests.steps PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(can_lite.integration_tests.steps PUBLIC
    can_lite.integration_tests.support
    can_lite.categories.firmware_upgrade
    cucumber_cpp.runner
    GTest::gtest
    GTest::gmock
)
