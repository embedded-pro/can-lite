add_library(can_lite.categories.foc_motor ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(can_lite.categories.foc_motor PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(can_lite.categories.foc_motor PUBLIC
    can_lite.core
)

target_sources(can_lite.categories.foc_motor PRIVATE
    FocMotorCategoryClient.cpp
    FocMotorCategoryClient.hpp
    FocMotorCategoryServer.cpp
    FocMotorCategoryServer.hpp
    FocMotorDefinitions.hpp
)

add_subdirectory(test)
