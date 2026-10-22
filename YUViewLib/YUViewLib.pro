QT += core gui widgets opengl xml concurrent network

TEMPLATE = lib
CONFIG += staticlib
CONFIG += c++20
CONFIG -= debug_and_release
CONFIG += object_parallel_to_source

gcc {
  # For gcc 9, setting 20 does not work. Must set c++2a.
  equals(QMAKE_GCC_MAJOR_VERSION, 9): QMAKE_CXXFLAGS += -std=c++2a
}

SOURCES += $$files(src/*.cpp, true)
HEADERS += $$files(src/*.h, true)

FORMS += $$files(ui/*.ui, false)

INCLUDEPATH += src/

RESOURCES += \
    images/images.qrc \
    docs/docs.qrc

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
