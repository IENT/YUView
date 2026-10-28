cmake_minimum_required(VERSION 3.21)

project(YUView VERSION 0.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

option(YUVIEW_BUILD_TESTS "Build YUView unit tests" OFF)

execute_process(COMMAND git describe --tags WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_VARIABLE YUVIEW_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
execute_process(COMMAND git rev-parse HEAD WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_VARIABLE YUVIEW_HASH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT YUVIEW_VERSION)
  set(YUVIEW_VERSION 0)
endif()
if(NOT YUVIEW_HASH)
  set(YUVIEW_HASH 0)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets OpenGL Xml Concurrent Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets OpenGL Xml Concurrent Network)

file(GLOB_RECURSE YUVIEW_LIB_SOURCES CONFIGURE_DEPENDS YUViewLib/src/*.cpp YUViewLib/src/*.h)
file(GLOB YUVIEW_LIB_FORMS CONFIGURE_DEPENDS YUViewLib/ui/*.ui)

add_library(YUViewLib STATIC
  ${YUVIEW_LIB_SOURCES}
  ${YUVIEW_LIB_FORMS}
  YUViewLib/images/images.qrc
  YUViewLib/docs/docs.qrc
)
set_target_properties(YUViewLib PROPERTIES
  AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/YUViewLib/ui")
target_include_directories(YUViewLib PUBLIC YUViewLib/src)
target_link_libraries(YUViewLib PUBLIC
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Gui
  Qt${QT_VERSION_MAJOR}::Widgets
  Qt${QT_VERSION_MAJOR}::OpenGL
  Qt${QT_VERSION_MAJOR}::Xml
  Qt${QT_VERSION_MAJOR}::Concurrent
  Qt${QT_VERSION_MAJOR}::Network
)
target_compile_definitions(YUViewLib PUBLIC
  YUVIEW_VERSION="${YUVIEW_VERSION}"
  YUVIEW_HASH="${YUVIEW_HASH}"
)

if(WIN32)
  target_compile_definitions(YUViewLib PUBLIC NOMINMAX)
endif()

add_executable(YUView WIN32 YUViewApp/src/yuviewapp.cpp)
set_target_properties(YUView PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/YUViewApp"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/YUViewApp"
)
target_link_libraries(YUView PRIVATE YUViewLib)

if(WIN32)
  target_sources(YUView PRIVATE YUViewApp/images/WindowsAppIcon.rc)
elseif(APPLE)
  set_target_properties(YUView PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_ICON_FILE YUView.icns
  )
  set_source_files_properties(YUViewApp/images/YUView.icns PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources
  )
  target_sources(YUView PRIVATE YUViewApp/images/YUView.icns)
endif()

if(UNIX AND NOT APPLE)
  include(GNUInstallDirs)
  install(TARGETS YUView RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  install(FILES packaging/linux/de.rwth_aachen.ient.YUView.appdata.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
  install(FILES packaging/linux/de.rwth_aachen.ient.YUView.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
  install(FILES packaging/linux/de.rwth_aachen.ient.YUView.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/mime/packages)
  foreach(size 32 64 128 256 512)
    install(FILES packaging/linux/icons/${size}x${size}/de.rwth_aachen.ient.YUView.png
      DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps)
  endforeach()
endif()

if(YUVIEW_BUILD_TESTS)
  enable_testing()
  find_package(GTest CONFIG QUIET)
  if(NOT TARGET GTest::gtest)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/submodules/googletest/CMakeLists.txt")
      message(FATAL_ERROR "Unit tests need GoogleTest. Initialize submodules or install GoogleTest.")
    endif()
    add_subdirectory(submodules/googletest)
  endif()

  file(GLOB_RECURSE YUVIEW_TEST_SOURCES CONFIGURE_DEPENDS
    YUViewUnitTest/*.cpp YUViewUnitTest/*.h)
  add_executable(YUViewUnitTest ${YUVIEW_TEST_SOURCES})
  set_target_properties(YUViewUnitTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/YUViewUnitTest")
  target_include_directories(YUViewUnitTest PRIVATE YUViewUnitTest/common)
  target_link_libraries(YUViewUnitTest PRIVATE YUViewLib GTest::gtest GTest::gmock)
  add_test(NAME YUViewUnitTest COMMAND YUViewUnitTest)
endif()