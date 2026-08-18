add_library(can_lite.categories.system ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(can_lite.categories.system PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(can_lite.categories.system PUBLIC
    can_lite.core
)

target_sources(can_lite.categories.system PRIVATE
    CanSystemCategoryClient.cpp
    CanSystemCategoryClient.hpp
    CanSystemCategoryServer.cpp
    CanSystemCategoryServer.hpp
)

emil_install(can_lite.categories.system
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_subdirectory(test)
