add_library(can_lite.server ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(can_lite.server PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(can_lite.server PUBLIC
    can_lite.categories.system
    can_lite.core
    infra.timer
)

target_sources(can_lite.server PRIVATE
    CanProtocolServer.cpp
    CanProtocolServer.hpp
)

emil_install(can_lite.server
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_subdirectory(test)
