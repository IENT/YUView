TEMPLATE = app

CONFIG += qt console warn_on no_testcase_installs depend_includepath testcase
CONFIG += c++1z
CONFIG -= debug_and_release
CONFIG -= app_bundled

TARGET = StatisticsFileCSVTest

QT += testlib
QT += xml
QT -= gui

INCLUDEPATH += $$top_srcdir/YUViewLib/src
LIBS += -L$$top_builddir/YUViewLib -lYUViewLib

SOURCES += StatisticsFileCSVTest.cpp
