TEMPLATE = subdirs
SUBDIRS = YUViewLib YUViewApp

YUViewApp.subdir = src/YUViewApp
YUViewLib.subdir = src/YUViewLib

YUViewYpp.depends = YUViewLib
