file(GLOB_RECURSE YUVIEW_LIB_SOURCES CONFIGURE_DEPENDS src/*.cpp src/*.h)
file(GLOB YUVIEW_LIB_FORMS CONFIGURE_DEPENDS ui/*.ui)

add_library(YUViewLib STATIC
  ${YUVIEW_LIB_SOURCES}
  ${YUVIEW_LIB_FORMS}
  images/images.qrc
  docs/docs.qrc
)
set_target_properties(YUViewLib PROPERTIES
  AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/ui")
target_include_directories(YUViewLib PUBLIC src)
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
