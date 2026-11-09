add_library(can_lite.categories.firmware_upgrade.session_test_doubles STATIC)
emil_build_for(can_lite.categories.firmware_upgrade.session_test_doubles HOST All BOOL CAN_LITE_BUILD_TESTS)

target_include_directories(can_lite.categories.firmware_upgrade.session_test_doubles PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../..>"
)

target_link_libraries(can_lite.categories.firmware_upgrade.session_test_doubles PUBLIC
    can_lite.categories.firmware_upgrade.session
    gmock
    upgrade.staging
)

target_sources(can_lite.categories.firmware_upgrade.session_test_doubles PRIVATE
    FirmwareUpgradeControlMock.hpp
    TestUpgradePack.cpp
    TestUpgradePack.hpp
)
