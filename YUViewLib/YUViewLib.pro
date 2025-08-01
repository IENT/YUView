QT += core gui widgets opengl openglwidgets xml concurrent network

TEMPLATE = lib
CONFIG += staticlib
CONFIG += c++17
CONFIG -= debug_and_release
CONFIG += object_parallel_to_source

SOURCES += $$files(src/*.cpp, true)
HEADERS += $$files(src/*.h, true)

# HDR files are automatically included by the recursive $$files() above

FORMS += $$files(ui/*.ui, false)

INCLUDEPATH += src/

RESOURCES += \
    images/images.qrc \
    docs/docs.qrc \
    resources/shaders/shaders.qrc

contains(QT_ARCH, x86_32|i386) {
    warning("You are building for a 32 bit system. This is untested and not supported.")
}

SVNN = $$system("git describe --tags")
LASTHASH = $$system("git rev-parse HEAD")
isEmpty(LASTHASH) {
    LASTHASH = 0
}
isEmpty(SVNN) {
    SVNN = 0
}

win32 {
    DEFINES += NOMINMAX
    # Windows-specific libraries for HDR detection
    LIBS += -ldxgi -luser32 -lole32
    # Note: WinRT support (-lwindowsapp) temporarily disabled for MinGW compatibility
}

win32-msvc* {
    HASHSTRING = '\\"$${LASTHASH}\\"'
    DEFINES += YUVIEW_HASH=$${HASHSTRING}
}
win32-g++ | linux | macx {
    HASHSTRING = '\\"$${LASTHASH}\\"'
    DEFINES += YUVIEW_HASH=\"$${HASHSTRING}\"
}

VERSTR = '\\"$${SVNN}\\"'
DEFINES += YUVIEW_VERSION=$${VERSTR}
