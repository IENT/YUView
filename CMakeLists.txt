cmake_minimum_required(VERSION 3.16)

project(YUView)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS gui widgets opengl xml concurrent network)

qt_standard_project_setup()
set(CMAKE_AUTORCC ON)

add_subdirectory(YUViewApp)
add_subdirectory(YUViewLib)
