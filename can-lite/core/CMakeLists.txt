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
    CanMessageHandler.hpp
    CanMessageType.hpp
    CanPayload.cpp
    CanPayload.hpp
    CanProtocolDefinitions.hpp
    CanSequenceSource.hpp
    CanFrameCodec.cpp
    CanFrameCodec.hpp
    CanFrameTransport.cpp
    CanFrameTransport.hpp
)

emil_install(can_lite.core
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_subdirectory(test)
