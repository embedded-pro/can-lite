add_library(can_lite.testing ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(can_lite.testing PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(can_lite.testing PUBLIC
    can_lite.core
)

target_sources(can_lite.testing PRIVATE
    EchoCategoryDefinitions.hpp
    EchoCategoryServer.hpp
    EchoCategoryServer.cpp
    EchoCategoryClient.hpp
    EchoCategoryClient.cpp
    VirtualCan.hpp
    VirtualCan.cpp
)

add_subdirectory(test)
