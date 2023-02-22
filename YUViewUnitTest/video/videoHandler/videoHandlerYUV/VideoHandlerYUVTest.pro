TEMPLATE = app

CONFIG += qt console warn_on no_testcase_installs depend_includepath testcase
CONFIG += c++1z
CONFIG -= debug_and_release
CONFIG -= app_bundled

TARGET = VideoHandlerYUVTest

QT += testlib
QT += gui widgets xml

INCLUDEPATH += $$top_srcdir/YUViewLib/src \
               $$top_srcdir/YUViewUnitTest
LIBS += -L$$top_builddir/YUViewLib -lYUViewLib -L$$top_builddir/YUViewUnitTest/helper -lYUViewUnitTestHelperLib

SOURCES += VideoHandlerYUVTest.cpp
