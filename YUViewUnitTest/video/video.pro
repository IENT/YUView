TEMPLATE = subdirs

requires(qtHaveModule(testlib))

SUBDIRS = yuvPixelFormatTest.pro \
          rgbPixelFormatTest.pro \
          yuvPixelFormatGuessTest.pro
