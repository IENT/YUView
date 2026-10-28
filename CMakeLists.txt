cmake_minimum_required(VERSION 3.21)

project(YUView LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

option(YUVIEW_BUILD_TESTS "Build YUView unit tests" OFF)

execute_process(COMMAND git describe --tags WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_VARIABLE YUVIEW_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT YUVIEW_VERSION)
  set(YUVIEW_VERSION 0)
endif()
message(STATUS "YUView version: ${YUVIEW_VERSION}")

execute_process(COMMAND git rev-parse HEAD WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_VARIABLE YUVIEW_HASH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT YUVIEW_HASH)
  set(YUVIEW_HASH 0)
endif()
message(STATUS "YUView commit hash: ${YUVIEW_HASH}")

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGL Xml Concurrent Network)
find_package(Qt6${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets OpenGL Xml Concurrent Network)

add_subdirectory(YUViewLib)
add_subdirectory(YUViewApp)

if(YUVIEW_BUILD_TESTS)
  enable_testing()
  add_subdirectory(YUViewUnitTest)
endif()