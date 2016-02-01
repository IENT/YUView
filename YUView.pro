#-------------------------------------------------
#
# Project created by QtCreator 2010-10-28T14:12:12
#
#-------------------------------------------------

QT       += core gui opengl xml concurrent network

TARGET = YUView
TEMPLATE = app

SOURCES += source/yuviewapp.cpp \
    source/mainwindow.cpp \
    source/yuvfile.cpp \
    source/statslistmodel.cpp \
    source/playlisttreewidget.cpp \
    source/statslistview.cpp \
    source/settingswindow.cpp \
    source/displaywidget.cpp \
    source/displaysplitwidget.cpp \
    source/playlistitem.cpp \
    source/frameobject.cpp \
    source/displayobject.cpp \
    source/statisticsobject.cpp\
    source/textobject.cpp \
    source/edittextdialog.cpp \
    source/plistparser.cpp \
    source/plistserializer.cpp \
    source/differenceobject.cpp \
    source/yuvsource.cpp \
    source/FileInfoGroupBox.cpp \
    source/statisticSource.cpp \
    source/statisticSourceFile.cpp \
    source/common.cpp \
    source/de265File.cpp

HEADERS  += source/yuviewapp.h \
    source/mainwindow.h \
    source/yuvfile.h \
    source/statslistmodel.h \
    source/playlisttreewidget.h \
    source/statslistview.h \
    source/settingswindow.h \
    source/displaywidget.h \
    source/displaysplitwidget.h \
    source/playlistitem.h \
    source/frameobject.h \
    source/displayobject.h \
    source/typedef.h \
    source/statisticsobject.h\
    source/textobject.h \
    source/edittextdialog.h \
    source/plistparser.h \
    source/plistserializer.h \
    source/differenceobject.h \
    source/statisticsextensions.h \
    source/yuvsource.h \
    source/FileInfoGroupBox.h \
    source/statisticSource.h \
    source/statisticSourceFile.h \
    source/common.h \
    source/de265File.h

FORMS    += ui/mainwindow.ui \
    ui/settingswindow.ui \
    ui/edittextdialog.ui

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

    target.path = /usr/local/bin
    INSTALLS += target
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
