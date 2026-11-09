add_library(can_lite.examples.setpoint STATIC)

target_include_directories(can_lite.examples.setpoint PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(can_lite.examples.setpoint PUBLIC
    can_lite.core
)

target_sources(can_lite.examples.setpoint PRIVATE
    SetpointCategoryClient.cpp
    SetpointCategoryClient.hpp
    SetpointCategoryServer.cpp
    SetpointCategoryServer.hpp
    SetpointDefinitions.hpp
)

add_subdirectory(test)
