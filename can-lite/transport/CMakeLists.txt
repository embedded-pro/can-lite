add_library(can_lite.transport ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(can_lite.transport PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(can_lite.transport PUBLIC
    can_lite.core
    can_lite.iso_tp
)

target_sources(can_lite.transport PRIVATE
    IsoTpTransport.hpp
    IsoTpTransportImpl.hpp
    IsoTpTransportImpl.cpp
)

add_subdirectory(iso-tp)
add_subdirectory(test)
