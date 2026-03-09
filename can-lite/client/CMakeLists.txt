add_library(can_lite.client ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(can_lite.client PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_sources(can_lite.client PRIVATE
    CanProtocolClient.cpp
    CanProtocolClient.hpp
)

target_link_libraries(can_lite.client PUBLIC
    can_lite.categories.system
    can_lite.core
)

add_subdirectory(test)
