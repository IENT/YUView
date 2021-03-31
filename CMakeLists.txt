cmake_minimum_required(VERSION 3.16.0)
project(YUView LANGUAGES CXX)

### Require out-of-source builds
file(TO_CMAKE_PATH "${PROJECT_BINARY_DIR}/CMakeLists.txt" LOC_PATH)
if(EXISTS "${LOC_PATH}")
    message(FATAL_ERROR "You cannot build in a source directory (or any directory with a CMakeLists.txt file). Please make a build subdirectory.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# try to find Qt6 first
find_package(Qt6 COMPONENTS Gui Widgets OpenGL Xml Concurrent Network)
if (Qt6_FOUND)
    message(STATUS "Building YUView using Qt6")
endif()
if (NOT Qt6_FOUND)
    # fallback: try to find Qt5
    find_package(Qt5 COMPONENTS Gui Widgets OpenGL Xml Concurrent Network REQUIRED)
    if (Qt5_FOUND)
        message(STATUS "Building YUView using Qt5")
    endif()
endif()

#####################################################################################################################################
# YUViewLib
#####################################################################################################################################
add_subdirectory( "YUViewLib" )

#####################################################################################################################################
# YUViewApp
#####################################################################################################################################
# get source and include files fo the app
file( GLOB_RECURSE APP_SRC_FILES "YUViewApp/src/*.cpp" )
file( GLOB_RECURSE APP_INC_FILES "YUViewApp/src/*.h" )

add_executable( YUView  ${APP_SRC_FILES}  ${APP_INC_FILES}  )
target_link_libraries( YUView
    YUViewLib
    Qt::Gui
    Qt::Widgets
    Qt::OpenGL
    Qt::Xml
    Qt::Concurrent
    Qt::Network
)
target_include_directories(YUView PUBLIC
    "YUViewApp/src"
    "YUViewLib/src"
)


#####################################################################################################################################
# YUViewUnitTest
#####################################################################################################################################
enable_testing(true) # needs to be before add_subdirectory (or in the root CMake file?)
add_subdirectory( "YUViewUnitTest" )
