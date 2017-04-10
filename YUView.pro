QT += gui opengl xml concurrent network

TARGET = YUView
TEMPLATE = app
CONFIG += c++11

# Please keep the project file lists sorted by name.

SOURCES += \
    source/de265Decoder.cpp \
    source/FFmpegDecoder.cpp \
    source/FFMpegDecoderLibHandling.cpp \
    source/fileInfoWidget.cpp \
    source/fileSource.cpp \
    source/fileSourceHEVCAnnexBFile.cpp \
    source/frameHandler.cpp \
    source/mainwindow.cpp \
    source/playbackController.cpp \
    source/playlistItem.cpp \
    source/playlistItemContainer.cpp \
    source/playlistItemDifference.cpp \
    source/playlistItemHEVCFile.cpp \
    source/playlistItemFFmpegFile.cpp \
    source/playlistItemImageFile.cpp \
    source/playlistItemImageFileSequence.cpp \
    source/playlistItemOverlay.cpp \
    source/playlistItemRawFile.cpp \
    source/playlistItems.cpp \
    source/playlistItemStatisticsFile.cpp \
    source/playlistItemText.cpp \
    source/playlistItemWithVideo.cpp \
    source/playlistTreeWidget.cpp \
    source/propertiesWidget.cpp \
    source/separateWindow.cpp \
    source/settingsDialog.cpp \
    source/showColorFrame.cpp \
    source/splitViewWidget.cpp \
    source/statisticHandler.cpp \
    source/statisticsExtensions.cpp \
    source/statisticsstylecontrol.cpp \
    source/statisticsStyleControl_ColorMapEditor.cpp \
    source/typedef.cpp \
    source/updateHandler.cpp \
    source/videoCache.cpp \
    source/videoHandler.cpp \
    source/videoHandlerDifference.cpp \
    source/videoHandlerRGB.cpp \
    source/videoHandlerYUV.cpp \
    source/viewStateHandler.cpp \
    source/yuviewapp.cpp

HEADERS += \
    source/de265Decoder.h \
    source/FFmpegDecoder.h \
    source/FFMpegDecoderLibHandling.h \
    source/FFMpegDecoderCommonDefs.h \
    source/fileInfoWidget.h \
    source/fileSource.h \
    source/fileSourceHEVCAnnexBFile.h \
    source/frameHandler.h \
    source/labelElided.h \
    source/mainwindow.h \
    source/playbackController.h \
    source/playlistItem.h \
    source/playlistItemContainer.h \
    source/playlistItemDifference.h \
    source/playlistItemHEVCFile.h \
    source/playlistItemFFmpegFile.h \
    source/playlistItemImageFile.h \
    source/playlistItemImageFileSequence.h \
    source/playlistItemOverlay.h \
    source/playlistItemRawFile.h \
    source/playlistItems.h \
    source/playlistItemStatisticsFile.h \
    source/playlistItemText.h \
    source/playlistItemWithVideo.h \
    source/playlistTreeWidget.h \
    source/propertiesWidget.h \
    source/separateWindow.h \
    source/settingsDialog.h \
    source/showColorFrame.h \
    source/signalsSlots.h \
    source/splitViewWidget.h \
    source/statisticHandler.h \
    source/statisticsExtensions.h \
    source/statisticsstylecontrol.h \
    source/statisticsStyleControl_ColorMapEditor.h \
    source/typedef.h \
    source/updateHandler.h \
    source/videoCache.h \
    source/videoHandler.h \
    source/videoHandlerDifference.h \
    source/videoHandlerRGB.h \
    source/videoHandlerYUV.h \
    source/viewStateHandler.h \
    source/yuviewapp.h

FORMS += \
    ui/frameHandler.ui \
    ui/mainwindow.ui \
    ui/playbackController.ui \
    ui/playlistItem.ui \
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
    libde265 \
    source

OTHER_FILES += \
    HACKING.md \
    README.md \
    docs/about.html \
    docs/help.html

target.path = /usr/bin/

desktop.path = /usr/share/applications
desktop.files = YUView.desktop

icon64.path = /usr/share/pixmaps
icon64.files += images/IENT-YUView-64.png

INSTALLS += target desktop icon64

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
