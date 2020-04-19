TEMPLATE = app

CONFIG += qt console warn_on no_testcase_installs depend_includepath testcase
CONFIG -= debug_and_release
CONFIG -= app_bundled

TARGET = videoHandlerYUVFormatFromToString

QT += testlib gui widgets

INCLUDEPATH += $$top_srcdir/YUViewLib/src $$top_builddir/YUViewLib
LIBS += -L$$top_builddir/YUViewLib -lYUViewLib

SOURCES += videoHandlerYUVFormatFromToString.cpp
