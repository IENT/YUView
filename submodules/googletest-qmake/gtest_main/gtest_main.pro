QT -= core gui

TARGET = gtest_main
TEMPLATE = lib
CONFIG += staticlib

INCLUDEPATH += \
    ../../googletest/googletest/include

SOURCES += \
    ../../googletest/googletest/src/gtest_main.cc
