QT += core xml 

TARGET = YUViewUnitTest
TEMPLATE = app

CONFIG += console
CONFIG -= app_bundle
CONFIG -= debug_and_release

CONFIG += c++20
gcc {
  # For gcc 9, setting 20 does not work. Must set c++2a.
  equals(QMAKE_GCC_MAJOR_VERSION, 9): QMAKE_CXXFLAGS += -std=c++2a
}

SOURCES += $$files(*.cpp, true)
HEADERS += $$files(*.h, true)

INCLUDEPATH += $$top_srcdir/submodules/googletest/googletest/include \
               $$top_srcdir/submodules/googletest/googlemock/include \
               $$top_srcdir/YUViewLib/src \
               $$top_srcdir/YUViewUnitTest/common
LIBS += -L$$top_builddir/submodules/googletest-qmake/gtest -lgtest
LIBS += -L$$top_builddir/submodules/googletest-qmake/gtest_main -lgtest_main
LIBS += -L$$top_builddir/YUViewLib -lYUViewLib

#win32-msvc* {
#    PRE_TARGETDEPS += $$top_builddir/YUViewLib/YUViewLib.lib
#} else {
#    PRE_TARGETDEPS += $$top_builddir/YUViewLib/libYUViewLib.a
#}

