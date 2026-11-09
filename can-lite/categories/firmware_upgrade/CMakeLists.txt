add_library(can_lite.categories.firmware_upgrade STATIC)

target_include_directories(can_lite.categories.firmware_upgrade PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(can_lite.categories.firmware_upgrade PUBLIC
    can_lite.core
    infra.timer
)

target_sources(can_lite.categories.firmware_upgrade PRIVATE
    FirmwareUpgradeDefinitions.hpp
    FirmwareUpgradeCategoryServer.hpp
    FirmwareUpgradeCategoryServer.cpp
    FirmwareUpgradeCategoryClient.hpp
    FirmwareUpgradeCategoryClient.cpp
)

emil_install(can_lite.categories.firmware_upgrade
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_library(can_lite.categories.firmware_upgrade.session STATIC)

target_include_directories(can_lite.categories.firmware_upgrade.session PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(can_lite.categories.firmware_upgrade.session PUBLIC
    bootloader
    can_lite.categories.firmware_upgrade
    hal.interfaces
    hal.synchronous_interfaces
    infra.timer
    infra.util
    upgrade.staging
)

target_sources(can_lite.categories.firmware_upgrade.session PRIVATE
    FirmwareStaging.hpp
    FirmwareUpgradeControl.hpp
    FirmwareUpgradeHold.hpp
    FirmwareUpgradeSession.cpp
    FirmwareUpgradeSession.hpp
    FirmwareVersion.hpp
)

emil_install(can_lite.categories.firmware_upgrade.session
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_subdirectory(test)
add_subdirectory(test_doubles)
