QT -= core gui

TARGET = gtest
TEMPLATE = lib
CONFIG += staticlib

INCLUDEPATH += \
    ../../googletest/googletest/include \
    ../../googletest/googletest

SOURCES = ../../googletest/googletest/src/gtest-all.cc
