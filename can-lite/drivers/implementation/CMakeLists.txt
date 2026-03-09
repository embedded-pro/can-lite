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
    add_library(can_lite.drivers.kvaser ${EMIL_EXCLUDE_FROM_ALL})

    target_include_directories(can_lite.drivers.kvaser PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    )

    target_link_libraries(can_lite.drivers.kvaser PUBLIC
        can_lite.drivers.interface
    )

    target_sources(can_lite.drivers.kvaser PRIVATE
        Kvaser.cpp
        Kvaser.hpp
    )

    add_library(can_lite.drivers.pcan ${EMIL_EXCLUDE_FROM_ALL})

    target_include_directories(can_lite.drivers.pcan PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    )

    target_link_libraries(can_lite.drivers.pcan PUBLIC
        can_lite.drivers.interface
    )

    target_sources(can_lite.drivers.pcan PRIVATE
        Pcan.cpp
        Pcan.hpp
    )

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
