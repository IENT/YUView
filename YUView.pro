#-------------------------------------------------
#
# Project created by QtCreator 2010-10-28T14:12:12
#
#-------------------------------------------------

QT       += core gui opengl xml concurrent

TARGET = YUView
TEMPLATE = app

SOURCES += main.cpp\
        mainwindow.cpp \
    yuvfile.cpp \
    yuviewapp.cpp \
    statslistmodel.cpp \
    playlisttreewidget.cpp \
    statslistview.cpp \
    settingswindow.cpp \
    displaywidget.cpp \
    displaysplitwidget.cpp \
    playlistitem.cpp \
    playlistitemvid.cpp \
    playlistitemstats.cpp \
    playlistitemtext.cpp \
    frameobject.cpp \
    displayobject.cpp \
    statisticsobject.cpp\
    textobject.cpp \
    edittextdialog.cpp \
    plistparser.cpp \
    plistserializer.cpp \
    playlistitemdifference.cpp \
    differenceobject.cpp

HEADERS  += mainwindow.h \
    yuvfile.h \
    yuviewapp.h \
    statslistmodel.h \
    playlisttreewidget.h \
    statslistview.h \
    settingswindow.h \
    displaywidget.h \
    displaysplitwidget.h \
    playlistitem.h \
    playlistitemvid.h \
    playlistitemstats.h \
    playlistitemtext.h \
    frameobject.h \
    displayobject.h \
    typedef.h \
    statisticsobject.h\
    textobject.h \
    edittextdialog.h \
    plistparser.h \
    plistserializer.h \
    playlistitemdifference.h \
    differenceobject.h \
    statisticsextensions.h
FORMS    += mainwindow.ui \
    settingswindow.ui \
    edittextdialog.ui

RESOURCES += \
    images.qrc \
    resources.qrc

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
    QMAKE_LFLAGS +=  -openmp, --large-address-aware
}
win32-g++ {
    message("MinGW Compiler detected.")
    QMAKE_CXXFLAGS += -fopenmp # that should work for a MinGW build?
    QMAKE_LFLAGS +=  -fopenmp
}
win32 {
    #QMAKE_LFLAGS_DEBUG    = /INCREMENTAL:NO
    RC_FILE += WindowsAppIcon.rc

    SVNN = $$system("git describe")
    #SVNN = $$replace(SVNN,"M","")
    #SVNN = $$replace(SVNN,"S","")
    #SVNN = $$replace(SVNN,"P","")
    #SVNN = $$section(SVNN, :, 0, 0)

}

isEmpty(SVNN) {
 SVNN = 0
}
VERSTR = '\\"$${SVNN}\\"'
DEFINES += YUVIEW_VERSION=\"$${VERSTR}\"
