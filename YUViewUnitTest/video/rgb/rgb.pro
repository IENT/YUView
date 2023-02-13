TEMPLATE = subdirs

requires(qtHaveModule(testlib))

SUBDIRS = ConversionRGBTest \
          pixelFormatRGBTest \
          pixelFormatRGBGuessTest
