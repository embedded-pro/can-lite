add_library(can_lite.tracing ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(can_lite.tracing PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(can_lite.tracing PUBLIC
    can_lite.core
    services.tracer
)

target_sources(can_lite.tracing PRIVATE
    TracingCan.cpp
    TracingCan.hpp
)

emil_install(can_lite.tracing
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_library(can_lite.tracing_iso_tp ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(can_lite.tracing_iso_tp PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(can_lite.tracing_iso_tp PUBLIC
    can_lite.transport
    services.tracer
)

target_sources(can_lite.tracing_iso_tp PRIVATE
    TracingIsoTpTransport.cpp
    TracingIsoTpTransport.hpp
)

emil_install(can_lite.tracing_iso_tp
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_subdirectory(test)
