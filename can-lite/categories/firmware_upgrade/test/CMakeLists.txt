add_executable(can_lite.categories.firmware_upgrade_test)
emil_build_for(can_lite.categories.firmware_upgrade_test HOST All BOOL CAN_LITE_BUILD_TESTS)
emil_add_test(can_lite.categories.firmware_upgrade_test)

target_link_libraries(can_lite.categories.firmware_upgrade_test PRIVATE
    can_lite.categories.firmware_upgrade
    can_lite.categories.firmware_upgrade.session
    can_lite.categories.firmware_upgrade.session_test_doubles
    can_lite.client
    can_lite.test_util
    hal.interfaces_test_doubles
    hal.synchronous_interfaces_test_doubles
    infra.timer_test_helper
    gmock_main
)

target_sources(can_lite.categories.firmware_upgrade_test PRIVATE
    TestFirmwareUpgradeCategoryClient.cpp
    TestFirmwareUpgradeCategoryServer.cpp
    TestFirmwareUpgradeSession.cpp
)
