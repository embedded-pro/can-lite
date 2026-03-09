add_library(can_lite.drivers.interface INTERFACE)

target_include_directories(can_lite.drivers.interface INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(can_lite.drivers.interface INTERFACE
    infra.util
    hal.interfaces
)

target_sources(can_lite.drivers.interface INTERFACE
    CanAdapter.hpp
)
