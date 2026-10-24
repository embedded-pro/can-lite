add_library(can_lite.drivers.codec ${EMIL_EXCLUDE_FROM_ALL})

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
