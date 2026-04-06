add_executable(can_lite.categories.firmware_upgrade_test)
emil_build_for(can_lite.categories.firmware_upgrade_test HOST All BOOL CAN_LITE_BUILD_TESTS)
emil_add_test(can_lite.categories.firmware_upgrade_test)

target_link_libraries(can_lite.categories.firmware_upgrade_test PRIVATE
    can_lite.categories.firmware_upgrade
    can_lite.client
    can_lite.test_util
    infra.timer_test_helper
    gmock_main
)

target_sources(can_lite.categories.firmware_upgrade_test PRIVATE
    TestFirmwareUpgradeCategoryClient.cpp
    TestFirmwareUpgradeCategoryServer.cpp
)
