QT += core gui widgets opengl xml concurrent network

TEMPLATE = lib
CONFIG += staticlib
CONFIG += c++1z
CONFIG -= debug_and_release
CONFIG += object_parallel_to_source

INCLUDEPATH += $$top_srcdir/YUViewLib/src

# For the generated ui_ files
INCLUDEPATH += $$top_builddir/YUViewLib/

TARGET = YUViewUnitTestHelperLib

SOURCES += $$files(*.cpp, true)
HEADERS += $$files(*.h, true)
