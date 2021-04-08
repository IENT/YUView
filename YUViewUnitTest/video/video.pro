TEMPLATE = subdirs

requires(qtHaveModule(testlib))

SUBDIRS = YUVPixelFormatTest.pro \
          rgbPixelFormatTest.pro \
          YUVPixelFormatGuessTest.pro
