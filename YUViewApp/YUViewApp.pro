QT += gui opengl xml concurrent network charts

TARGET = YUView
TEMPLATE = app
CONFIG += c++11
CONFIG -= debug_and_release

SOURCES += $$files(src/*.cpp, false)
HEADERS += $$files(src/*.h, false)

INCLUDEPATH += $$top_srcdir/YUViewLib/src
LIBS += -L$$top_builddir/YUViewLib -lYUViewLib

win32 {
    PRE_TARGETDEPS += $$top_builddir/YUViewLib/YUViewLib.lib
} else {
    PRE_TARGETDEPS += $$top_builddir/YUViewLib/libYUViewLib.a
}

unix:!mac {
    isEmpty(PREFIX) {
        PREFIX = /usr/local
    }
    isEmpty(BINDIR) {
        BINDIR = bin
    }

    target.path = $$PREFIX/$$BINDIR/

    metainfo.files = $$top_srcdir/packaging/linux/de.rwth_aachen.ient.YUView.appdata.xml
    metainfo.path = $$PREFIX/share/metainfo
    desktop.files = $$top_srcdir/packaging/linux/de.rwth_aachen.ient.YUView.desktop
    desktop.path = $$PREFIX/share/applications
    mime.files = $$top_srcdir/packaging/linux/de.rwth_aachen.ient.YUView.xml
    mime.path = $$PREFIX/share/mime/packages
    icon32.files = $$top_srcdir/packaging/linux/icons/32x32/de.rwth_aachen.ient.YUView.png
    icon64.files = $$top_srcdir/packaging/linux/icons/64x64/de.rwth_aachen.ient.YUView.png
    icon128.files = $$top_srcdir/packaging/linux/icons/128x128/de.rwth_aachen.ient.YUView.png
    icon256.files = $$top_srcdir/packaging/linux/icons/256x256/de.rwth_aachen.ient.YUView.png
    icon512.files = $$top_srcdir/packaging/linux/icons/512x512/de.rwth_aachen.ient.YUView.png
    icon32.path = $$PREFIX/share/icons/hicolor/32x32/apps
    icon64.path = $$PREFIX/share/icons/hicolor/64x64/apps
    icon128.path = $$PREFIX/share/icons/hicolor/128x128/apps
    icon256.path = $$PREFIX/share/icons/hicolor/256x256/apps
    icon512.path = $$PREFIX/share/icons/hicolor/512x512/apps
    icon1024.path = $$PREFIX/share/icons/hicolor/1024x1024/apps

    INSTALLS += target metainfo desktop mime icon32 icon64 icon128 icon256 icon512 icon1024
}

contains(QT_ARCH, x86_32|i386) {
    warning("You are building for a 32 bit system. This is untested and not supported.")
}

macx {
    QMAKE_MAC_SDK = macosx

    ICON = images/YUView.icns
    QMAKE_INFO_PLIST = Info.plist
    SVNN = $$system("git describe --tags")
}

linux {
    SVNN = $$system("git describe --tags")
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
    RC_FILE += images/WindowsAppIcon.rc
    SVNN = $$system("git describe --tags")
    DEFINES += NOMINMAX
}

LASTHASH = $$system("git rev-parse HEAD")
isEmpty(LASTHASH) {
    LASTHASH = 0
}

isEmpty(SVNN) {
    SVNN = 0
}
VERSTR = '\\"$${SVNN}\\"'
DEFINES += YUVIEW_VERSION=$${VERSTR}
