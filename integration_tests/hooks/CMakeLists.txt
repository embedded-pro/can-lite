add_library(can_lite.integration_tests.hooks OBJECT
    Hooks.cpp
)

target_link_libraries(can_lite.integration_tests.hooks PUBLIC
    can_lite.integration_tests.support
    cucumber_cpp.runner
)
