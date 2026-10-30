if(WIN32)
  add_executable(YUView WIN32 src/yuviewapp.cpp)
elseif(APPLE)
  add_executable(YUView MACOSX_BUNDLE src/yuviewapp.cpp)
else()
  add_executable(YUView src/yuviewapp.cpp)
endif()

set_target_properties(YUView PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/YUViewApp"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/YUViewApp"
)
target_link_libraries(YUView PRIVATE YUViewLib)

if(WIN32)
  target_sources(YUView PRIVATE images/WindowsAppIcon.rc)
  target_include_directories(YUView PRIVATE images)
elseif(APPLE)
  set_target_properties(YUView PROPERTIES
    BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER "de.ient.yuview"
    MACOSX_BUNDLE_BUNDLE_NAME "YUView"
    MACOSX_BUNDLE_BUNDLE_VERSION "${YUVIEW_VERSION}"
    MACOSX_BUNDLE_ICON_FILE YUView.icns
  )
  set_source_files_properties(images/YUView.icns PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources
  )
  target_sources(YUView PRIVATE images/YUView.icns)
endif()

if(UNIX AND NOT APPLE)
  include(GNUInstallDirs)
  install(TARGETS YUView RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  install(FILES ${PROJECT_SOURCE_DIR}/packaging/linux/de.rwth_aachen.ient.YUView.appdata.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
  install(FILES ${PROJECT_SOURCE_DIR}/packaging/linux/de.rwth_aachen.ient.YUView.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
  install(FILES ${PROJECT_SOURCE_DIR}/packaging/linux/de.rwth_aachen.ient.YUView.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/mime/packages)
  foreach(size 32 64 128 256 512)
    install(FILES ${PROJECT_SOURCE_DIR}/packaging/linux/icons/${size}x${size}/de.rwth_aachen.ient.YUView.png
      DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps)
  endforeach()
endif()