cmake_minimum_required(VERSION 3.16.0)

project(YUViewUnitTest LANGUAGES CXX)

### Require out-of-source builds
file(TO_CMAKE_PATH "${PROJECT_BINARY_DIR}/CMakeLists.txt" LOC_PATH)
if(EXISTS "${LOC_PATH}")
    message(FATAL_ERROR "You cannot build in a source directory (or any directory with a CMakeLists.txt file). Please make a build subdirectory.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
# try to find Qt6 first
find_package(Qt6 COMPONENTS Test)
if (Qt6_FOUND)
    message(STATUS "Building YUViewUnitTestusing Qt6")
endif()
if (NOT Qt6_FOUND)
    # fallback: try to find Qt5
    find_package(Qt5 COMPONENTS Test REQUIRED)
    if (Qt5_FOUND)
        message(STATUS "Building YUViewUnitTest using Qt5")
    endif()
endif()

enable_testing(true)

# get all test files
file( GLOB_RECURSE TST_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.cpp" )

# add a target for each test file
FOREACH(test_file_name ${TST_FILES})
    get_filename_component(test_base_name ${test_file_name} NAME_WE )

    message( VERBOSE "Adding executable for ${test_base_name}")

    list(APPEND TEST_LIST ${test_base_name})
    add_executable( ${test_base_name}  ${test_file_name})
    add_test(NAME ${test_base_name} COMMAND ${test_base_name})
    target_link_libraries( ${test_base_name}
        YUViewLib
        Qt::Test
    )
    target_include_directories(${test_base_name}  PUBLIC "../YUViewLib/src")

ENDFOREACH()
