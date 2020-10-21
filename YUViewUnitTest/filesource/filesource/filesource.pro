TEMPLATE = app

CONFIG += qt console warn_on no_testcase_installs depend_includepath testcase
CONFIG -= debug_and_release
CONFIG -= app_bundled

TARGET = tst_filesource

QT += testlib
QT -= gui

INCLUDEPATH += ../../../YUViewLib/src
LIBS += -L../../../YUViewLib -lYUViewLib

SOURCES += tst_filesource.cpp
