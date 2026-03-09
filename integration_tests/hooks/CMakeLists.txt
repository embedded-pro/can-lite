add_library(can_lite.integration_tests.hooks OBJECT
    Hooks.cpp
)

target_link_libraries(can_lite.integration_tests.hooks PUBLIC
    cucumber_cpp.runner
)
