TEMPLATE = subdirs
SUBDIRS = YUViewLib YUViewApp LibFFmpeg

YUViewApp.subdir = YUViewApp
YUViewLib.subdir = YUViewLib
LibFFmpeg.subdir = submodules/LibFFmpeg

YUViewLib.depends = LibFFmpeg
YUViewApp.depends = YUViewLib

UNITTESTS {
  SUBDIRS += Googletest
  Googletest.subdir = submodules/googletest-qmake
  
  SUBDIRS += YUViewUnitTest
  YUViewUnitTest.subdir = YUViewUnitTest
  YUViewUnitTest.depends = Googletest
  YUViewUnitTest.depends = YUViewLib
}
