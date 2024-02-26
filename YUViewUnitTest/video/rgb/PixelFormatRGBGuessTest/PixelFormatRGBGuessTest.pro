TEMPLATE = app

CONFIG += qt console warn_on no_testcase_installs depend_includepath testcase
CONFIG += c++17
CONFIG -= debug_and_release
CONFIG -= app_bundled

TARGET = PixelFormatRGBGuessTest

QT += testlib
QT -= gui

INCLUDEPATH += $$top_srcdir/YUViewLib/src
LIBS += -L$$top_builddir/YUViewLib -lYUViewLib

SOURCES += PixelFormatRGBGuessTest.cpp
