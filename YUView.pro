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
    renderwidget.cpp \
    yuvfile.cpp \
    yuvobject.cpp \
    yuvlistitem.cpp \
    yuvlistitemvid.cpp \
    yuviewapp.cpp \
    videofile.cpp \
    yuvlistitemmulti.cpp \
    cameraparameter.cpp \
    renderer.cpp \
    viewinterpolator.cpp \
    nointerpolator.cpp \
    fullinterpolator.cpp \
    defaultrenderer.cpp \
    shutterrenderer.cpp \
    anaglyphrenderer.cpp \
    leftrightlinerenderer.cpp \
    statisticsparser.cpp \
    statslistmodel.cpp \
    sliderdelegate.cpp \
    playlisttreewidget.cpp \
    statslistview.cpp \
    settingswindow.cpp \
    glew.c

HEADERS  += mainwindow.h \
    renderwidget.h \
    yuvfile.h \
    yuvobject.h \
    yuvlistitem.h \
    yuvlistitemvid.h \
    yuviewapp.h \
    videofile.h \
    yuvlistitemmulti.h \
    cameraparameter.h \
    renderer.h \
    viewinterpolator.h \
    nointerpolator.h \
    fullinterpolator.h \
    defaultrenderer.h \
    shutterrenderer.h \
    anaglyphrenderer.h \
    leftrightlinerenderer.h \
    statisticsparser.h \
    statslistmodel.h \
    sliderdelegate.h \
    playlisttreewidget.h \
    statslistview.h \
    settingswindow.h \
    glew.h \
    glxew.h \
    wglew.h

FORMS    += mainwindow.ui \
    settingswindow.ui

OTHER_FILES += \
    docs/YUView ToDo.txt

RESOURCES += \
    images.qrc \
    resources.qrc

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

    LIBS += -lGLU
}

win32 {
    LIBS += -lGLU32
    DEFINES += GLEW_STATIC

    QMAKE_CXXFLAGS += -openmp
#QMAKE_LFLAGS_DEBUG    = /INCREMENTAL:NO
    RC_FILE += WindowsAppIcon.rc
}
