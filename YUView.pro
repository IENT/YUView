#-------------------------------------------------
#
# Project created by QtCreator 2010-10-28T14:12:12
#
#-------------------------------------------------

QT       += core gui opengl xml concurrent network webkitwidgets

TARGET = YUView
TEMPLATE = app

SOURCES += source/yuviewapp.cpp \
    source/de265File.cpp \
    source/de265File_BitstreamHandler.cpp \
    source/fileInfoWidget.cpp \
    source/fileSource.cpp \
    source/mainwindow.cpp \
    source/playbackController.cpp \
    source/playlistItem.cpp \
    source/playlistItemDifference.cpp \
    source/playlistItemText.cpp \
    source/playlistItemYUVFile.cpp \
    source/playlistItemYuvSource.cpp \
    source/playlistTreeWidget.cpp \
    source/propertiesWidget.cpp \
    source/settingswindow.cpp \
    source/splitViewWidget.cpp \
    source/statisticSource.cpp \
    source/playlistItemStatisticsFile.cpp \
    source/statsListModel.cpp \
    source/statsListView.cpp \
    source/playlistItemVideo.cpp \
    source/videoCache.cpp



HEADERS  += source/yuviewapp.h \
    source/de265File.h \
    source/de265File_BitstreamHandler.h \
    source/fileInfoWidget.h \
    source/fileSource.h \
    source/mainwindow.h \
    source/playbackController.h \
    source/playlistItem.h \
    source/playlistItemDifference.h \
    source/playlistItemText.h \
    source/playlistItemYUVFile.h \
    source/playlistItemYuvSource.h \
    source/playlistTreeWidget.h \
    source/propertiesWidget.h \
    source/settingswindow.h \
    source/splitViewWidget.h \
    source/statisticsExtensions.h \
    source/statisticSource.h \
    source/playlistItemStatisticsFile.h \
    source/statsListModel.h \
    source/statsListView.h \
    source/typedef.h \
    source/playlistItemVideo.h \
    source/videoCache.h

FORMS    += \
    ui/mainwindow.ui \
    ui/settingswindow.ui \
    ui/frameobjectdialog.ui \
    ui/playbackController.ui \
    ui/playlistItemVideo.ui \
    ui/playlistItemText.ui \
    ui/playlistItemYUVSource.ui \
    ui/splitViewWidgetControls.ui

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

HASHSTRING = '\\"$${LASTHASH}\\"'
DEFINES += YUVIEW_HASH=\"$${HASHSTRING}\"

isEmpty(SVNN) {
 SVNN = 0
}
VERSTR = '\\"$${SVNN}\\"'
DEFINES += YUVIEW_VERSION=\"$${VERSTR}\"
