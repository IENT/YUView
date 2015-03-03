#-------------------------------------------------
#
# Project created by QtCreator 2010-10-28T14:12:12
#
#-------------------------------------------------

QT       += core gui opengl xml

TARGET = YUView
TEMPLATE = app

SOURCES += main.cpp\
        mainwindow.cpp \
    yuvfile.cpp \
    yuviewapp.cpp \
    statslistmodel.cpp \
    sliderdelegate.cpp \
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
    plistserializer.cpp

HEADERS  += mainwindow.h \
    yuvfile.h \
    yuviewapp.h \
    statslistmodel.h \
    sliderdelegate.h \
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
    plistserializer.h
FORMS    += mainwindow.ui \
    settingswindow.ui \
    edittextdialog.ui

OTHER_FILES += \
    docs/YUView ToDo.txt

RESOURCES += \
    images.qrc \
    resources.qrc

contains(QT_ARCH, x86_32):{
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
    SVNN   = $$system("svn info | grep \"Revision\" | awk '{print $2}'")

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
    SVNN   = $$system("svn info | grep \"Revision\" | awk '{print $2}'")
}

win32 {

    #QMAKE_CXXFLAGS += -openmp # that works for the msvc2012 compiler
    #QMAKE_LFLAGS +=  -openmp
    QMAKE_CXXFLAGS += -fopenmp # that should work for a MinGW build?
    QMAKE_LFLAGS +=  -fopenmp
    #QMAKE_LFLAGS_DEBUG    = /INCREMENTAL:NO
    RC_FILE += WindowsAppIcon.rc

    SVNN = $$system("svnversion -n")
    SVNN = $$replace(SVNN,"M","")
    SVNN = $$replace(SVNN,"S","")
    SVNN = $$replace(SVNN,"P","")
    SVNN = $$section(SVNN, :, 0, 0)

}

isEmpty(SVNN) {
 SVNN = 0
}
VERSTR = '\\"$${SVNN}\\"'
DEFINES += YUVIEW_VERSION=\"$${VERSTR}\"
