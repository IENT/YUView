TEMPLATE = subdirs

requires(qtHaveModule(testlib))

SUBDIRS = ConversionRGBTest \
          Issue511RGBConversionInvalidMemoryAccess \
          PixelFormatRGBTest \
          PixelFormatRGBGuessTest
