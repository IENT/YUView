
qt_add_executable(YUView src/yuviewapp.cpp)

target_link_libraries(YUView PRIVATE YUView_lib Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::Xml Qt6::Concurrent Qt6::Network)

set_target_properties(YUView PROPERTIES
        WIN32_EXECUTABLE ON
)