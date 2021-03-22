cmake_minimum_required(VERSION 3.16.0)

project(YUViewLib LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS "ui")

find_package(Qt6 COMPONENTS Gui Widgets OpenGL Xml Concurrent Network REQUIRED)

# get source files
file( GLOB_RECURSE SRC_FILES "src/*.cpp" )

# get include files
file( GLOB_RECURSE INC_FILES "src/*.h" )

# get forms, ui files
file( GLOB FORMS_FILES "ui/*.ui" )

# Resources
file (GLOB RSC_FILES
    "images/images.qrc"
    "docs/docs.qrc"
)

add_library( YUViewLib STATIC ${SRC_FILES}  ${INC_FILES} ${FORMS_FILES}  ${RSC_FILES} )
target_link_libraries( YUViewLib
    Qt6::Gui
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::Xml
    Qt6::Concurrent
    Qt6::Network
)
target_include_directories(YUViewLib PUBLIC "src")