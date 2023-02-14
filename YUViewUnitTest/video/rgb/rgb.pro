TEMPLATE = subdirs

requires(qtHaveModule(testlib))

SUBDIRS = ConversionRGBTest \
          Issue511RGBConversionInvalidMemoryAccess \
          pixelFormatRGBTest \
          pixelFormatRGBGuessTest
