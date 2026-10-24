function(can_lite_add_driver target)
    add_library(${target} ${EMIL_EXCLUDE_FROM_ALL})

    target_include_directories(${target} PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    )

    target_link_libraries(${target} PUBLIC
        can_lite.drivers.interface
        infra.event
    )

    target_sources(${target} PRIVATE ${ARGN})
endfunction()

can_lite_add_driver(can_lite.drivers.slcan
    SlcanAdapter.cpp
    SlcanAdapter.hpp
)
target_link_libraries(can_lite.drivers.slcan PUBLIC can_lite.drivers.codec)

can_lite_add_driver(can_lite.drivers.multi
    MultiCanAdapter.cpp
    MultiCanAdapter.hpp
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    can_lite_add_driver(can_lite.drivers.socketcan
        SocketCanAdapter.cpp
        SocketCanAdapter.hpp
    )

    can_lite_add_driver(can_lite.drivers.serial
        PosixSerialPort.cpp
        PosixSerialPort.hpp
    )

    set(can_lite_platform_drivers can_lite.drivers.socketcan)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    can_lite_add_driver(can_lite.drivers.serial
        WindowsSerialPort.cpp
        WindowsSerialPort.hpp
    )
    target_link_libraries(can_lite.drivers.serial PRIVATE advapi32)

    can_lite_add_driver(can_lite.drivers.pcan
        ChannelNumber.hpp
        PcanAdapter.cpp
        PcanAdapter.hpp
    )

    can_lite_add_driver(can_lite.drivers.kvaser
        ChannelNumber.hpp
        KvaserAdapter.cpp
        KvaserAdapter.hpp
    )

    can_lite_add_driver(can_lite.drivers.gsusb
        ChannelNumber.hpp
        GsUsbAdapter.cpp
        GsUsbAdapter.hpp
    )
    target_link_libraries(can_lite.drivers.gsusb PUBLIC can_lite.drivers.codec PRIVATE setupapi winusb)

    set(can_lite_platform_drivers can_lite.drivers.pcan can_lite.drivers.kvaser can_lite.drivers.gsusb)
endif()

if(DEFINED can_lite_platform_drivers)
    can_lite_add_driver(can_lite.drivers.platform
        PlatformCanAdapter.cpp
        PlatformCanAdapter.hpp
    )
    target_link_libraries(can_lite.drivers.platform PUBLIC
        can_lite.drivers.multi
        can_lite.drivers.serial
        can_lite.drivers.slcan
        ${can_lite_platform_drivers}
    )
endif()
