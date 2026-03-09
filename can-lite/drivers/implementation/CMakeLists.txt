if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_library(can_lite.drivers.socketcan ${EMIL_EXCLUDE_FROM_ALL})

    target_include_directories(can_lite.drivers.socketcan PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    )

    target_link_libraries(can_lite.drivers.socketcan PUBLIC
        can_lite.drivers.interface
    )

    target_sources(can_lite.drivers.socketcan PRIVATE
        SocketCan.cpp
        SocketCan.hpp
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    option(CAN_LITE_DRIVER_KVASER "Build Kvaser CANlib driver (requires Kvaser CANlib SDK)" OFF)
    option(CAN_LITE_DRIVER_PCAN "Build PCAN-Basic driver (requires PEAK PCAN-Basic SDK)" OFF)
    option(CAN_LITE_DRIVER_CANABLE "Build CANable SLCAN driver" ON)

    if(CAN_LITE_DRIVER_KVASER)
        find_path(KVASER_INCLUDE_DIR canlib.h)
        find_library(KVASER_LIBRARY canlib32)

        if(KVASER_INCLUDE_DIR AND KVASER_LIBRARY)
            add_library(can_lite.drivers.kvaser ${EMIL_EXCLUDE_FROM_ALL})

            target_include_directories(can_lite.drivers.kvaser PUBLIC
                "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
                "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
            )

            target_include_directories(can_lite.drivers.kvaser PRIVATE
                ${KVASER_INCLUDE_DIR}
            )

            target_link_libraries(can_lite.drivers.kvaser PUBLIC
                can_lite.drivers.interface
                ${KVASER_LIBRARY}
            )

            target_sources(can_lite.drivers.kvaser PRIVATE
                Kvaser.cpp
                Kvaser.hpp
            )
        else()
            message(WARNING "CAN_LITE_DRIVER_KVASER is ON but Kvaser CANlib SDK not found")
        endif()
    endif()

    if(CAN_LITE_DRIVER_PCAN)
        find_path(PCAN_INCLUDE_DIR PCANBasic.h)
        find_library(PCAN_LIBRARY PCANBasic)

        if(PCAN_INCLUDE_DIR AND PCAN_LIBRARY)
            add_library(can_lite.drivers.pcan ${EMIL_EXCLUDE_FROM_ALL})

            target_include_directories(can_lite.drivers.pcan PUBLIC
                "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
                "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
            )

            target_include_directories(can_lite.drivers.pcan PRIVATE
                ${PCAN_INCLUDE_DIR}
            )

            target_link_libraries(can_lite.drivers.pcan PUBLIC
                can_lite.drivers.interface
                ${PCAN_LIBRARY}
            )

            target_sources(can_lite.drivers.pcan PRIVATE
                Pcan.cpp
                Pcan.hpp
            )
        else()
            message(WARNING "CAN_LITE_DRIVER_PCAN is ON but PCAN-Basic SDK not found")
        endif()
    endif()

    if(CAN_LITE_DRIVER_CANABLE)
        add_library(can_lite.drivers.canable ${EMIL_EXCLUDE_FROM_ALL})

        target_include_directories(can_lite.drivers.canable PUBLIC
            "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
            "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
        )

        target_link_libraries(can_lite.drivers.canable PUBLIC
            can_lite.drivers.interface
        )

        target_sources(can_lite.drivers.canable PRIVATE
            Canable.cpp
            Canable.hpp
        )
    endif()
endif()
