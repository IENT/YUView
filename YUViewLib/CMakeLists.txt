
file(GLOB_RECURSE SOURCES src/*.cpp)
file(GLOB_RECURSE HEADERS src/*.h)

file(GLOB_RECURSE FORMS ui/*.ui)
set(CMAKE_AUTOUIC_SEARCH_PATHS ui)

qt_add_library(YUView_lib STATIC
        ${SOURCES}
        ${HEADERS}
        ${FORMS}
        images/images.qrc
        docs/docs.qrc
)

target_link_libraries(YUView_lib PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::Xml Qt6::Concurrent Qt6::Network)

target_include_directories(YUView_lib PUBLIC src)