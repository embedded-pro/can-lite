add_library(can_lite.drivers ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(can_lite.drivers PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(can_lite.drivers PUBLIC
    can_lite.drivers.interface
)

add_subdirectory(interface)
add_subdirectory(implementation)
