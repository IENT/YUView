QT -= core gui

TARGET = gmock
TEMPLATE = lib

CONFIG += staticlib
CONFIG -= debug_and_release
CONFIG += c++17

INCLUDEPATH += \
    ../../googletest/googlemock/include \
    ../../googletest/googletest/include \
    ../../googletest/googlemock

SOURCES = ../../googletest/googlemock/src/gmock-all.cc
