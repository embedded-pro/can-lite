cmake_minimum_required(VERSION 3.24)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(CAN_LITE_STANDALONE On)
endif()

option(CMAKE_COMPILE_WARNING_AS_ERROR "Enable warnings-as-error" On)
option(CAN_LITE_BUILD_TESTS "Enable build of the tests" Off)

if (CAN_LITE_BUILD_TESTS)
    set(BUILD_TESTING On)
endif()

project(can_lite LANGUAGES C CXX ASM VERSION 0.1.0) # x-release-please-version

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED On)

set_directory_properties(PROPERTY USE_FOLDERS ON)

if (CAN_LITE_STANDALONE)
    include(FetchContent)
    FetchContent_Declare(
        emil
        GIT_REPOSITORY https://github.com/embedded-pro/embedded-infra-lib.git
        GIT_TAG        81a7c1d503d68e2ee48f00769bcea4975c0849b1 # Unreleased
    )
    FetchContent_MakeAvailable(emil)

    if (CAN_LITE_BUILD_TESTS)
        set(CCR_FETCH_DEPS On CACHE BOOL "" FORCE)
        set(CCR_BUILD_TESTS Off CACHE BOOL "" FORCE)

        FetchContent_Declare(
            cucumber_cpp
            GIT_REPOSITORY https://github.com/philips-software/amp-cucumber-cpp-runner.git
            GIT_TAG        v4.0.0
        )
        FetchContent_MakeAvailable(cucumber_cpp)
    endif()
endif()

if (CAN_LITE_BUILD_TESTS)
    include(CTest)
    emil_enable_testing()
endif()

add_subdirectory(can-lite)

if (CAN_LITE_BUILD_TESTS)
    add_subdirectory(integration_tests)
endif()

emil_clangformat_directories(can_lite DIRECTORIES can-lite)
emil_folderize_all_targets()
emil_coverage_targets(DIRECTORIES .)

set(CPACK_GENERATOR "ZIP;TGZ")
set(CPACK_SOURCE_IGNORE_FILES ".vs/;.git/;build/;output/;.venv/")
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION "${PROJECT_DESCRIPTION}")
set(CPACK_DEBIAN_PACKAGE_NAME "${CPACK_PACKAGE_NAME}")
set(CPACK_RPM_PACKAGE_NAME "${CPACK_PACKAGE_NAME}")
set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")
set(CPACK_PACKAGE_MAINTAINER "${CPACK_PACKAGE_VENDOR}")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_VENDOR}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

include(CPack)
