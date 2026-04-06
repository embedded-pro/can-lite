add_library(can_lite.iso_tp ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(can_lite.iso_tp PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(can_lite.iso_tp PUBLIC
    hal.interfaces
    infra.util
    infra.timer
)

target_sources(can_lite.iso_tp PRIVATE
    IsoTpTypes.hpp
    IsoTpFrameCodec.hpp
    IsoTpFrameCodec.cpp
    IsoTpSender.hpp
    IsoTpSender.cpp
    IsoTpReceiver.hpp
    IsoTpReceiver.cpp
    IsoTpChannel.hpp
    IsoTpChannelImpl.hpp
    IsoTpChannelImpl.cpp
)
