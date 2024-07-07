QT += core

TARGET = YUViewUnitTest
TEMPLATE = app

CONFIG += console
CONFIG -= app_bundle
CONFIG -= debug_and_release
CONFIG += c++17

SOURCES += $$files(*.cpp, true)
HEADERS += $$files(*.h, true)

INCLUDEPATH += $$top_srcdir/submodules/googletest/googletest/include \
               $$top_srcdir/YUViewLib/src
LIBS += -L$$top_builddir/submodules/googletest-qmake/gtest -lgtest
LIBS += -L$$top_builddir/submodules/googletest-qmake/gtest_main -lgtest_main
LIBS += -L$$top_builddir/YUViewLib -lYUViewLib

#win32-msvc* {
#    PRE_TARGETDEPS += $$top_builddir/YUViewLib/YUViewLib.lib
#} else {
#    PRE_TARGETDEPS += $$top_builddir/YUViewLib/libYUViewLib.a
#}

