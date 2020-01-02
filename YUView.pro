TEMPLATE = subdirs
SUBDIRS = YUViewLib YUViewApp YUViewUnitTest

YUViewApp.subdir = YUViewApp
YUViewLib.subdir = YUViewLib
YUViewUnitTest.subdir = YUViewUnitTest

YUViewApp.depends = YUViewLib
YUViewUnitTest.depends = YUViewLib
