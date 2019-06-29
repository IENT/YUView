QT += gui opengl xml concurrent network

TARGET = YUView
TEMPLATE = app
CONFIG += c++11

# Please keep the project file lists sorted by name.

SOURCES += \
    source/bitstreamAnalysisWidget.cpp \
    source/decoderBase.cpp \
    source/decoderDav1d.cpp \
    source/decoderFFmpeg.cpp \
    source/decoderHM.cpp \
    source/decoderLibde265.cpp \
    source/FFMpegLibrariesHandling.cpp \
    source/fileInfoWidget.cpp \
    source/fileSource.cpp \
    source/fileSourceAnnexBFile.cpp \
    source/fileSourceFFmpegFile.cpp \
    source/frameHandler.cpp \
    source/mainwindow.cpp \
    source/parserAnnexB.cpp \
    source/parserAnnexBAVC.cpp \
    source/parserAnnexBHEVC.cpp \
    source/parserAnnexBMpeg2.cpp \
    source/parserAV1OBU.cpp \
    source/parserAVFormat.cpp \
    source/parserBase.cpp \
    source/parserCommon.cpp \
    source/playbackController.cpp \
    source/playlistItem.cpp \
    source/playlistItemCompressedVideo.cpp \
    source/playlistItemContainer.cpp \
    source/playlistItemDifference.cpp \
    source/playlistItemImageFile.cpp \
    source/playlistItemImageFileSequence.cpp \
    source/playlistItemOverlay.cpp \
    source/playlistItemRawFile.cpp \
    source/playlistItems.cpp \
    source/playlistItemStatisticsFile.cpp \
    source/playlistItemStatisticsCSVFile.cpp \
    source/playlistItemStatisticsVTMBMSFile.cpp \
    source/playlistItemText.cpp \
    source/playlistItemWithVideo.cpp \
    source/playlistTreeWidget.cpp \
    source/propertiesWidget.cpp \
    source/separateWindow.cpp \
    source/settingsDialog.cpp \
    source/showColorFrame.cpp \
    source/singleInstanceHandler.cpp \
    source/splitViewWidget.cpp \
    source/statisticHandler.cpp \
    source/statisticsExtensions.cpp \
    source/statisticsstylecontrol.cpp \
    source/statisticsStyleControl_ColorMapEditor.cpp \
    source/typedef.cpp \
    source/updateHandler.cpp \
    source/videoCache.cpp \
    source/videoCacheInfoWidget.cpp \
    source/videoHandler.cpp \
    source/videoHandlerDifference.cpp \
    source/videoHandlerRGB.cpp \
    source/videoHandlerYUV.cpp \
    source/viewStateHandler.cpp \
    source/yuviewapp.cpp

HEADERS += \
    source/bitstreamAnalysisWidget.h \
    source/decoderBase.h \
    source/decoderDav1d.h \
    source/decoderFFmpeg.h \
    source/decoderHM.h \
    source/decoderLibde265.h \
    source/FFMpegLibrariesHandling.h \
    source/FFMpegLibrariesTypes.h \
    source/fileInfoWidget.h \
    source/fileSource.h \
    source/fileSourceAnnexBFile.h \
    source/fileSourceFFmpegFile.h \
    source/frameHandler.h \
    source/labelElided.h \
    source/mainwindow.h \
    source/mainwindow_performanceTestDialog.h \
    source/parserAnnexB.h \
    source/parserAnnexBAVC.h \
    source/parserAnnexBHEVC.h \
    source/parserAnnexBMpeg2.h \
    source/parserAV1OBU.h \
    source/parserAVFormat.h \
    source/parserBase.h \
    source/parserCommon.h \
    source/parserCommonMacros.h \
    source/playbackController.h \
    source/playlistItem.h \
    source/playlistItemCompressedVideo.h \
    source/playlistItemContainer.h \
    source/playlistItemDifference.h \
    source/playlistItemImageFile.h \
    source/playlistItemImageFileSequence.h \
    source/playlistItemOverlay.h \
    source/playlistItemRawFile.h \
    source/playlistItems.h \
    source/playlistItemStatisticsFile.h \
    source/playlistItemStatisticsCSVFile.h \
    source/playlistItemStatisticsVTMBMSFile.h \
    source/playlistItemText.h \
    source/playlistItemWithVideo.h \
    source/playlistTreeWidget.h \
    source/propertiesWidget.h \
    source/separateWindow.h \
    source/settingsDialog.h \
    source/showColorFrame.h \
    source/splitViewWidget.h \
    source/singleInstanceHandler.h \
    source/statisticHandler.h \
    source/statisticsExtensions.h \
    source/statisticsstylecontrol.h \
    source/statisticsStyleControl_ColorMapEditor.h \
    source/typedef.h \
    source/updateHandler.h \
    source/videoCache.h \
    source/videoCacheInfoWidget.h \
    source/videoHandler.h \
    source/videoHandlerDifference.h \
    source/videoHandlerRGB.h \
    source/videoHandlerYUV.h \
    source/viewStateHandler.h \
    source/yuviewapp.h \
    decoder/libde265/de265.h \
    decoder/libde265/de265_internals.h \
    decoder/libde265/de265-version.h \
    decoder/dav1d/common.h \
    decoder/dav1d/data.h \
    decoder/dav1d/dav1d.h \
    decoder/dav1d/headers.h \
    decoder/dav1d/picture.h

FORMS += \
    ui/bitstreamAnalysisWidget.ui \
    ui/frameHandler.ui \
    ui/mainwindow.ui \
    ui/mainwindow_performanceTestDialog.ui \
    ui/playbackController.ui \
    ui/playlistItem.ui \
    ui/playlistItemCompressedFile.ui \
    ui/playlistItemCompressedFile_logDialog.ui \
    ui/playlistItemOverlay.ui \
    ui/playlistItemText.ui \
    ui/playlistItemHEVCFile.ui \
    ui/settingsDialog.ui \
    ui/splitViewWidgetControls.ui \
    ui/statisticHandler.ui \
    ui/updateDialog.ui \
    ui/videoHandlerDifference.ui \
    ui/videoHandlerRGB.ui \
    ui/videoHandlerRGB_CustomFormatDialog.ui \
    ui/videoHandlerYUV.ui \
    ui/videoHandlerYUV_CustomFormatDialog.ui \
    ui/statisticsstylecontrol.ui \
    ui/statisticsStyleControl_ColorMapEditor.ui

RESOURCES += \
    images/images.qrc \
    docs/resources.qrc

INCLUDEPATH += \
    decoder/libde265 \
    decoder/dav1d \
    source/

OTHER_FILES += \
    HACKING.md \
    README.md \
    docs/about.html \
    docs/help.html

unix:!mac {
	isEmpty(PREFIX) {
		PREFIX = /usr/local
	}
	isEmpty(BINDIR) {
		BINDIR = bin
	}

        target.path = $$PREFIX/$$BINDIR/

        metainfo.files = packaging/linux/de.rwth_aachen.ient.YUView.appdata.xml
        metainfo.path = $$PREFIX/share/metainfo
        desktop.files = packaging/linux/de.rwth_aachen.ient.YUView.desktop
        desktop.path = $$PREFIX/share/applications
        mime.files = packaging/linux/de.rwth_aachen.ient.YUView.xml
        mime.path = $$PREFIX/share/mime/packages
        icon32.files = packaging/linux/icons/32x32/de.rwth_aachen.ient.YUView.png
        icon64.files = packaging/linux/icons/64x64/de.rwth_aachen.ient.YUView.png
        icon128.files = packaging/linux/icons/128x128/de.rwth_aachen.ient.YUView.png
        icon256.files = packaging/linux/icons/256x256/de.rwth_aachen.ient.YUView.png
        icon512.files = packaging/linux/icons/512x512/de.rwth_aachen.ient.YUView.png
        icon32.path = $$PREFIX/share/icons/hicolor/32x32/apps
        icon64.path = $$PREFIX/share/icons/hicolor/64x64/apps
        icon128.path = $$PREFIX/share/icons/hicolor/128x128/apps
        icon256.path = $$PREFIX/share/icons/hicolor/256x256/apps
        icon512.path = $$PREFIX/share/icons/hicolor/512x512/apps

        INSTALLS += target metainfo desktop mime icon32 icon64 icon128 icon256 icon512
}


contains(QT_ARCH, x86_32|i386) {
    warning("You are building for a 32 bit system. This is untested!")
}

macx {
    CONFIG(debug, debug|release) {
        DESTDIR = build/debug
    } else {
        DESTDIR = build/release
    }
    OBJECTS_DIR = $$DESTDIR/.obj
    MOC_DIR = $$DESTDIR/.moc
    RCC_DIR = $$DESTDIR/.qrc
    UI_DIR = $$DESTDIR/.ui

    QMAKE_MAC_SDK = macosx

    ICON = images/YUView.icns
    QMAKE_INFO_PLIST = Info.plist
    SVNN   = $$system("git describe --tags")
}

linux {
    CONFIG(debug, debug|release) {
        DESTDIR = build/debug
    } else {
        DESTDIR = build/release
    }
    OBJECTS_DIR = $$DESTDIR/.obj
    MOC_DIR = $$DESTDIR/.moc
    RCC_DIR = $$DESTDIR/.qrc
    UI_DIR = $$DESTDIR/.ui

    SVNN   = $$system("git describe --tags")
}
win32-msvc* {
    message("MSVC Compiler detected.")
}
win32-g++ {
    message("MinGW Compiler detected.")
    QMAKE_FLAGS_RELEASE += -O3 -Ofast -msse4.1 -mssse3 -msse3 -msse2 -msse -mfpmath=sse
    QMAKE_CXXFLAGS_RELEASE += -O3 -Ofast -msse4.1 -mssse3 -msse3 -msse2 -msse -mfpmath=sse
}
win32 {
	CONFIG(debug, debug|release) {
        DESTDIR = build/debug
    } else {
        DESTDIR = build/release
    }
    OBJECTS_DIR = $$DESTDIR/.obj
    MOC_DIR = $$DESTDIR/.moc
    RCC_DIR = $$DESTDIR/.qrc
    UI_DIR = $$DESTDIR/.ui

    #QMAKE_LFLAGS_DEBUG    = /INCREMENTAL:NO
    RC_FILE += images/WindowsAppIcon.rc

    SVNN = $$system("git describe --tags")
    DEFINES += NOMINMAX
}

LASTHASH = $$system("git rev-parse HEAD")
isEmpty(LASTHASH) {
LASTHASH = 0
}

win32-msvc* {
    HASHSTRING = '\\"$${LASTHASH}\\"'
    DEFINES += YUVIEW_HASH=$${HASHSTRING}
}

win32-g++ | linux | macx {
    HASHSTRING = '\\"$${LASTHASH}\\"'
    DEFINES += YUVIEW_HASH=\"$${HASHSTRING}\"
}

isEmpty(SVNN) {
 SVNN = 0
}

win32-msvc* {
    VERSTR = '\\"$${SVNN}\\"'
    DEFINES += YUVIEW_VERSION=$${VERSTR}
}

win32-g++ | linux | macx {
    VERSTR = '\\"$${SVNN}\\"'
    DEFINES += YUVIEW_VERSION=\"$${VERSTR}\"
}
