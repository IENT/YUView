TEMPLATE = lib

QT += core widgets opengl xml concurrent network

CONFIG += staticlib
CONFIG += c++1z
CONFIG -= debug_and_release
CONFIG += object_parallel_to_source

TARGET = YUViewUnitTestHelperLib

QT += testlib
QT -= gui

INCLUDEPATH += $$top_srcdir/YUViewLib/src
LIBS += -L$$top_builddir/YUViewLib -lYUViewLib

SOURCES += YUVGenerator.cpp
