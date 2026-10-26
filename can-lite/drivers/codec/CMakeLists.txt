add_library(can_lite.drivers.codec STATIC)

target_include_directories(can_lite.drivers.codec PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(can_lite.drivers.codec PUBLIC
    can_lite.drivers.interface
)

target_sources(can_lite.drivers.codec PRIVATE
    GsUsbProtocol.cpp
    GsUsbProtocol.hpp
    SlcanCodec.cpp
    SlcanCodec.hpp
)

emil_install(can_lite.drivers.codec
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
