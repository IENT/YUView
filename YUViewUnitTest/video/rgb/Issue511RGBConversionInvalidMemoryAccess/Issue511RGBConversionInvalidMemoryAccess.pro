TEMPLATE = app

CONFIG += qt console warn_on no_testcase_installs depend_includepath testcase
CONFIG += c++1z
CONFIG -= debug_and_release
CONFIG -= app_bundled

TARGET = Issue511RGBConversionInvalidMemoryAccess

QT += testlib
QT -= gui

INCLUDEPATH += $$top_srcdir/YUViewLib/src
LIBS += -L$$top_builddir/YUViewLib -lYUViewLib

SOURCES += Issue511RGBConversionInvalidMemoryAccess.cpp
