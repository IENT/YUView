#ifndef TYPEDEF_H
#define TYPEDEF_H

typedef enum {
   YUVC_UnknownPixelFormat                        =  0,
   YUVC_32RGBAPixelFormat                         =  1, // k32RGBAPixelFormat='RGBA', FourCC 'RGBA'
   YUVC_24RGBPixelFormat                          =  2, // k24RGBPixelFormat=0x00000018
   YUVC_24BGRPixelFormat                          =  3, // k24BGRPixelFormat='BG24'
   YUVC_411YpCbCr8PlanarPixelFormat               =  4,
   YUVC_420YpCbCr8PlanarPixelFormat               =  5, // FourCC 'i420'
   YUVC_422YpCbCr8PlanarPixelFormat               =  6,
   YUVC_UYVY422PixelFormat                        =  7, // kUYVY422PixelFormat='UYVY', k422YpCbCr8CodecType='2vuy', FourCC 'uyvy'
   YUVC_422YpCbCr10PixelFormat                    =  8, // k422YpCbCr10CodecType='v210', FourCC 'v210'
   YUVC_444YpCbCr8PlanarPixelFormat               =  9,
   YUVC_444YpCbCr12LEPlanarPixelFormat            = 10,
   YUVC_444YpCbCr12BEPlanarPixelFormat            = 11,
   YUVC_444YpCbCr16LEPlanarPixelFormat            = 12,
   YUVC_444YpCbCr16BEPlanarPixelFormat            = 13,
#if __LITTLE_ENDIAN__
   YUVC_444YpCbCr12NativePlanarPixelFormat        = YUVC_444YpCbCr12LEPlanarPixelFormat,
   YUVC_444YpCbCr12SwappedPlanarPixelFormat       = YUVC_444YpCbCr12BEPlanarPixelFormat,
   YUVC_444YpCbCr16NativePlanarPixelFormat        = YUVC_444YpCbCr16LEPlanarPixelFormat,
   YUVC_444YpCbCr16SwappedPlanarPixelFormat       = YUVC_444YpCbCr16BEPlanarPixelFormat,
#elif __BIG_ENDIAN__
   YUVC_444YpCbCr12NativePlanarPixelFormat        = YUVC_444YpCbCr12BEPlanarPixelFormat,
   YUVC_444YpCbCr12SwappedPlanarPixelFormat       = YUVC_444YpCbCr12LEPlanarPixelFormat,
   YUVC_444YpCbCr16NativePlanarPixelFormat        = YUVC_444YpCbCr16BEPlanarPixelFormat,
   YUVC_444YpCbCr16SwappedPlanarPixelFormat       = YUVC_444YpCbCr16LEPlanarPixelFormat,
#endif
   YUVC_8GrayPixelFormat                          = 14, // FourCC 'y800'
   YUVC_GBR12in16LEPlanarPixelFormat              = 15,
   YUVC_420YpCbCr10LEPlanarPixelFormat            = 16,
   YUVC_422YpCrCb8PlanarPixelFormat               = 17, // 'YV16',
   YUVC_444YpCrCb8PlanarPixelFormat               = 18,
   YUVC_UYVY422YpCbCr10PixelFormat                = 19  // found in VQEG files
} YUVCPixelFormatType;

typedef enum {
   YUVC601ColorConversionType = 601,
   YUVC709ColorConversionType = 709
} YUVCColorConversionType;

typedef enum
{
    NearestNeighborInterpolation,
    BiLinearInterpolation,
    InterstitialInterpolation
} InterpolationMode;

#endif // TYPEDEF_H

