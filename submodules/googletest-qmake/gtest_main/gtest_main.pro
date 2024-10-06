QT -= core gui

TARGET = gtest_main
TEMPLATE = lib

CONFIG += staticlib
CONFIG -= debug_and_release
CONFIG += c++17

INCLUDEPATH += \
    ../../googletest/googletest/include

SOURCES += \
    ../../googletest/googletest/src/gtest_main.cc
