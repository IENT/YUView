#-------------------------------------------------
#
# Project created by QtCreator 2010-10-28T14:12:12
#
#-------------------------------------------------

QT       += core gui opengl xml concurrent network

TARGET = YUView
TEMPLATE = app

SOURCES += source/yuviewapp.cpp \
    source/videoHandlerYUV.cpp \
    source/videoHandlerRGB.cpp \
    source/videoHandlerDifference.cpp \
    source/videoHandler.cpp \
    source/statisticHandler.cpp \
    source/fileInfoWidget.cpp \
    source/fileSource.cpp \
    source/mainwindow.cpp \
    source/playbackController.cpp \
    source/playlistItemDifference.cpp \
    source/playlistItemText.cpp \
    source/playlistItemRawFile.cpp \
    source/playlistTreeWidget.cpp \
    source/propertiesWidget.cpp \
    source/settingswindow.cpp \
    source/splitViewWidget.cpp \
    source/playlistItemStatisticsFile.cpp \
    source/videoCache.cpp \
    source/fileSourceHEVCAnnexBFile.cpp \
    source/playlistItemHEVCFile.cpp \
    source/playlistItemOverlay.cpp \
    source/playlistitemIndexed.cpp \
    source/playlistitemStatic.cpp

HEADERS += source/videoHandlerYUV.h \
    source/videoHandlerRGB.h \
    source/videoHandlerDifference.h \
    source/videoHandler.h \
    source/statisticHandler.h \
    source/yuviewapp.h \
    source/videoCache.h \
    source/typedef.h \
    source/statisticsExtensions.h \
    source/splitViewWidget.h \
    source/settingswindow.h \
    source/propertiesWidget.h \
    source/playlistTreeWidget.h \
    source/playlistItemRawFile.h \
    source/playlistItemText.h \
    source/playlistItemStatisticsFile.h \
    source/playlistItemDifference.h \
    source/playlistItem.h \
    source/playbackController.h \
    source/mainwindow.h \
    source/fileSource.h \
    source/fileInfoWidget.h \
    source/fileSourceHEVCAnnexBFile.h \
    source/playlistItemHEVCFile.h \
    source/playlistItemOverlay.h \
    source/playlistitemIndexed.h \
    source/playlistitemStatic.h

FORMS    += ui/mainwindow.ui \
    ui/statisticHandler.ui \
    ui/videoHandler.ui \
    ui/videoHandlerYUV.ui \
	ui/videoHandlerRGB.ui \
	ui/videoHandlerRGB_CustomFormatDialog.ui \
    ui/playbackController.ui \
    ui/playlistItemText.ui \
    ui/settingswindow.ui \
    ui/splitViewWidgetControls.ui \
    ui/videoHandlerDifference.ui \
    ui/playlistItemOverlay.ui \
    ui/playlistItemIndexed.ui \
    ui/playlistItemStatic.ui

RESOURCES += \
    images/images.qrc \
    docs/resources.qrc

INCLUDEPATH += "libde265" \
                source



contains(QT_ARCH, x86_32||i386):{
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
    SVNN   = $$system("git describe")

    # GCC only :-(
    #QMAKE_CXXFLAGS += -fopenmp
    #QMAKE_LFLAGS *= -fopenmp
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

    QMAKE_CXXFLAGS += -fopenmp
    QMAKE_LFLAGS *= -fopenmp

    SVNN   = $$system("git describe")
}
win32-msvc* {
    message("MSVC Compiler detected.")
    QMAKE_CXXFLAGS += -openmp # that works for the msvc2012 compiler
    QMAKE_LFLAGS +=
}
win32-g++ {
    message("MinGW Compiler detected.")
    QMAKE_CXXFLAGS += -fopenmp # that should work for a MinGW build?
    QMAKE_LFLAGS +=  -fopenmp
    QMAKE_FLAGS_RELEASE += -O3 -Ofast -msse4.1 -mssse3 -msse3 -msse2 -msse -mfpmath=sse
    QMAKE_CXXFLAGS_RELEASE += -O3 -Ofast -msse4.1 -mssse3 -msse3 -msse2 -msse -mfpmath=sse
}
win32 {
    #QMAKE_LFLAGS_DEBUG    = /INCREMENTAL:NO
    RC_FILE += images/WindowsAppIcon.rc

    SVNN = $$system("git describe")

}

LASTHASH = $$system("git rev-parse HEAD")
isEmpty(LASTHASH) {
LASTHASH = 0
}

win32-msvc* {
    HASHSTRING = '\\"$${LASTHASH}\\"'
    DEFINES += YUVIEW_HASH=$${HASHSTRING}
}

win32-g++ || linux {
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

win32-g++ || linux {
    VERSTR = '\\"$${SVNN}\\"'
    DEFINES += YUVIEW_VERSION=\"$${VERSTR}\"
}
