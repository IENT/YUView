TEMPLATE = subdirs

requires(qtHaveModule(testlib))

SUBDIRS = PixelFormatYUVTest.pro \
          PixelFormatRGBTest.pro \
          PixelFormatYUVGuessTest.pro
