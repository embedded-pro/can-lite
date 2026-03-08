add_library(can_lite.core ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(can_lite.core PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(can_lite.core PUBLIC
    infra.util
    hal.interfaces
)

target_sources(can_lite.core PRIVATE
    CanCategory.cpp
    CanCategory.hpp
    CanMessageType.hpp
    CanProtocolDefinitions.hpp
    CanFrameCodec.cpp
    CanFrameCodec.hpp
    CanFrameTransport.cpp
    CanFrameTransport.hpp
    CanSystemCategory.cpp
    CanSystemCategory.hpp
)

add_subdirectory(test)
