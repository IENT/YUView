TEMPLATE = subdirs
SUBDIRS = YUViewLib YUViewApp

YUViewApp.subdir = YUViewApp
YUViewLib.subdir = YUViewLib

YUViewApp.depends = YUViewLib

UNITTESTS {
  SUBDIRS += Googletest
  Googletest.subdir = submodules/googletest-qmake
  
  SUBDIRS += YUViewUnitTest
  YUViewUnitTest.subdir = YUViewUnitTest
  YUViewUnitTest.depends = Googletest
  YUViewUnitTest.depends = YUViewLib
}
