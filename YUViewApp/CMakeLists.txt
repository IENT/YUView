cmake_minimum_required(VERSION 3.16.0)

project(YUViewApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


### Require out-of-source builds
file(TO_CMAKE_PATH "${PROJECT_BINARY_DIR}/CMakeLists.txt" LOC_PATH)
if(EXISTS "${LOC_PATH}")
    message(FATAL_ERROR "You cannot build in a source directory (or any directory with a CMakeLists.txt file). Please make a build subdirectory.")
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS "ui")

find_package(Qt6 COMPONENTS Gui Widgets OpenGL Xml Concurrent Network REQUIRED)

# get source files
file( GLOB_RECURSE SRC_FILES "src/*.cpp" )

# get include files
file( GLOB_RECURSE INC_FILES "src/*.h" )


add_executable( YUView  ${SRC_FILES}  ${INC_FILES}  )
target_link_libraries( YUView
    YUViewLib
    Qt6::Gui
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::Xml
    Qt6::Concurrent
    Qt6::Network
)
target_include_directories(YUView PUBLIC
    "src"
    "../YUViewLib/src"
)