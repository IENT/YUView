TEMPLATE = lib
CONFIG += staticlib
CONFIG += c++1z
CONFIG -= debug_and_release

SOURCES += $$files(*.cpp, true)
HEADERS += $$files(*.h, true)

warning("Sources list " + $${SOURCES})
