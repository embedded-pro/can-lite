add_library(can_lite.drivers.hub STATIC)

target_include_directories(can_lite.drivers.hub PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(can_lite.drivers.hub PUBLIC
    can_lite.drivers.codec
    infra.timer
)

target_sources(can_lite.drivers.hub PRIVATE
    VirtualCanBus.cpp
    VirtualCanBus.hpp
)

if(TARGET can_lite.drivers.loopback)
    target_link_libraries(can_lite.drivers.hub PUBLIC can_lite.drivers.loopback)
    target_sources(can_lite.drivers.hub PRIVATE
        VirtualCanHub.cpp
        VirtualCanHub.hpp
    )
endif()

emil_install(can_lite.drivers.hub
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
