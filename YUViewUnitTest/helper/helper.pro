QT += core gui widgets opengl xml concurrent network

TEMPLATE = app
CONFIG += staticlib
CONFIG += c++1z
CONFIG -= debug_and_release

SOURCES += $$files(*.cpp, true)
HEADERS += $$files(*.h, true)

