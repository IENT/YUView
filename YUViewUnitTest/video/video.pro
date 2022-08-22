TEMPLATE = subdirs

requires(qtHaveModule(testlib))

SUBDIRS = pixelFormatYUV/PixelFormatYUVTest.pro \
          pixelFormatRGBTest/PixelFormatRGBTest.pro \
          pixelFormatYUVGuessTest/PixelFormatYUVGuessTest.pro \
          pixelFormatRGBGuessTest/PixelFormatRGBGuessTest.pro
