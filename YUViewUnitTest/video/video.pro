TEMPLATE = subdirs

requires(qtHaveModule(testlib))

SUBDIRS = YUVPixelFormatTest.pro \
          PixelFormatRGBTest.pro \
          YUVPixelFormatGuessTest.pro
