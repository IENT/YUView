QT -= core gui

TARGET = gtest
TEMPLATE = lib

CONFIG += staticlib
CONFIG -= debug_and_release
CONFIG += c++17

INCLUDEPATH += \
    ../../googletest/googletest/include \
    ../../googletest/googletest

SOURCES = ../../googletest/googletest/src/gtest-all.cc
