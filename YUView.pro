TEMPLATE = subdirs
SUBDIRS = YUViewLib YUViewApp

YUViewApp.subdir = YUViewApp
YUViewLib.subdir = YUViewLib

YUViewApp.depends = YUViewLib
