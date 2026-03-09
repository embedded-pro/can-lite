add_library(can_lite.integration_tests.steps OBJECT
    CanBusSteps.cpp
    CategoryDiscoverySteps.cpp
    ErrorHandlingSteps.cpp
    FocMotorSteps.cpp
    RateLimitingSteps.cpp
    SequenceValidationSteps.cpp
    SystemCategorySteps.cpp
)

target_link_libraries(can_lite.integration_tests.steps PUBLIC
    can_lite.integration_tests.support
    cucumber_cpp.runner
    GTest::gtest
    GTest::gmock
)
