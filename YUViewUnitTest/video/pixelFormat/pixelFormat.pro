TEMPLATE = subdirs

requires(qtHaveModule(testlib))

SUBDIRS = pixelFormatYUV \
          pixelFormatRGB \
          pixelFormatYUVGuess \
          pixelFormatRGBGuess
