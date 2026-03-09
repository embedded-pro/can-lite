add_executable(can_lite.integration_tests)

target_link_libraries(can_lite.integration_tests PRIVATE
    can_lite.integration_tests.steps
    can_lite.integration_tests.hooks
    cucumber_cpp.runner
)

add_test(NAME can_lite.integration_tests COMMAND can_lite.integration_tests -- ${CMAKE_CURRENT_SOURCE_DIR}/../features)
