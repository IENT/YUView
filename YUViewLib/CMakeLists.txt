cmake_minimum_required(VERSION 3.16.0)

project(YUViewLib LANGUAGES CXX)

### Require out-of-source builds
file(TO_CMAKE_PATH "${PROJECT_BINARY_DIR}/CMakeLists.txt" LOC_PATH)
if(EXISTS "${LOC_PATH}")
    message(FATAL_ERROR "You cannot build in a source directory (or any directory with a CMakeLists.txt file). Please make a build subdirectory.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS "ui")

# try to find Qt6 first
find_package(Qt6 COMPONENTS Gui Widgets OpenGL Xml Concurrent Network)
if (Qt6_FOUND)
    message(STATUS "Building YUViewLib using Qt6")
endif()
if (NOT Qt6_FOUND)
    # fallback: try to find Qt5
    find_package(Qt5 COMPONENTS Gui Widgets OpenGL Xml Concurrent Network REQUIRED)
    if (Qt5_FOUND)
        message(STATUS "Building YUViewLib using Qt5")
    endif()
endif()


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
target_link_libraries( YUViewLib PRIVATE
    Qt::Gui
    Qt::Widgets
    Qt::OpenGL
    Qt::Xml
    Qt::Concurrent
    Qt::Network
)
target_include_directories(YUViewLib PUBLIC "src")