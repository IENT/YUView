/*  YUView - YUV player with advanced analytics toolset
*   Copyright (C) 2015  Institut für Nachrichtentechnik
*                       RWTH Aachen University, GERMANY
*
*   YUView is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   YUView is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with YUView.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef TYPEDEF_H
#define TYPEDEF_H

#define INT_INVALID -1

typedef enum {
   YUVC_UnknownPixelFormat                        =  0,
   YUVC_GBR12in16LEPlanarPixelFormat              =  1,
   YUVC_32RGBAPixelFormat                         =  2, // k32RGBAPixelFormat='RGBA', FourCC 'RGBA'
   YUVC_24RGBPixelFormat                          =  3, // k24RGBPixelFormat=0x00000018
   YUVC_24BGRPixelFormat                          =  4, // k24BGRPixelFormat='BG24'
   YUVC_444YpCbCr16LEPlanarPixelFormat            =  5,
   YUVC_444YpCbCr16BEPlanarPixelFormat            =  6,
   YUVC_444YpCbCr12LEPlanarPixelFormat            =  7,
   YUVC_444YpCbCr12BEPlanarPixelFormat            =  8,
   YUVC_444YpCbCr10LEPlanarPixelFormat            =  9,
   YUVC_444YpCbCr10BEPlanarPixelFormat            =  10,
   YUVC_444YpCbCr8PlanarPixelFormat               =  11,
   YUVC_444YpCrCb8PlanarPixelFormat               = 12,
   YUVC_422YpCbCr8PlanarPixelFormat               = 13,
   YUVC_422YpCrCb8PlanarPixelFormat               = 14, // 'YV16',
   YUVC_UYVY422PixelFormat                        = 15, // kUYVY422PixelFormat='UYVY', k422YpCbCr8CodecType='2vuy', FourCC 'uyvy'
   YUVC_422YpCbCr10PixelFormat                    = 16, // k422YpCbCr10CodecType='v210', FourCC 'v210'
   YUVC_UYVY422YpCbCr10PixelFormat                = 17, // found in VQEG files
   YUVC_420YpCbCr10LEPlanarPixelFormat            = 18,
   YUVC_420YpCbCr8PlanarPixelFormat               = 19, // FourCC 'i420'
   YUVC_411YpCbCr8PlanarPixelFormat               = 20,
   YUVC_8GrayPixelFormat                          = 21, // FourCC 'y800'
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
   YUVC_444YpCbCr12NativePlanarPixelFormat        = YUVC_444YpCbCr12LEPlanarPixelFormat,
   YUVC_444YpCbCr12SwappedPlanarPixelFormat       = YUVC_444YpCbCr12BEPlanarPixelFormat,
   YUVC_444YpCbCr16NativePlanarPixelFormat        = YUVC_444YpCbCr16LEPlanarPixelFormat,
   YUVC_444YpCbCr16SwappedPlanarPixelFormat       = YUVC_444YpCbCr16BEPlanarPixelFormat,
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
   YUVC_444YpCbCr12NativePlanarPixelFormat        = YUVC_444YpCbCr12BEPlanarPixelFormat,
   YUVC_444YpCbCr12SwappedPlanarPixelFormat       = YUVC_444YpCbCr12LEPlanarPixelFormat,
   YUVC_444YpCbCr16NativePlanarPixelFormat        = YUVC_444YpCbCr16BEPlanarPixelFormat,
   YUVC_444YpCbCr16SwappedPlanarPixelFormat       = YUVC_444YpCbCr16LEPlanarPixelFormat,
#endif
} YUVCPixelFormatType;

#ifndef YUVIEW_HASH
#define VERSION_CHECK 0
#define YUVIEW_HASH 0
#else
#define VERSION_CHECK 1
#endif

/*
kr/kg/kb matrix (Rec. ITU-T H.264 03/2010, p. 379):
R = Y                  + V*(1-Kr)
G = Y - U*(1-Kb)*Kb/Kg - V*(1-Kr)*Kr/Kg
B = Y + U*(1-Kb)

To respect value range of Y in [16:235] and U/V in [16:240], the matrix entries need to be scaled by 255/219 for Y and 255/112 for U/V
In this software color conversion is performed with 16bit precision. Thus, further scaling with 2^16 is performed to get all factors as integers.
*/
typedef enum {
   YUVC601ColorConversionType,
   YUVC709ColorConversionType,
   YUVC2020ColorConversionType
} YUVCColorConversionType;

typedef enum
{
    NearestNeighborInterpolation,
    BiLinearInterpolation,
    InterstitialInterpolation
} InterpolationMode;

#define MAX_SCALE_FACTOR 5

template <typename T> inline T clip(const T& n, const T& lower, const T& upper) { return std::max(lower, std::min(n, upper)); }

// A pair of two strings
typedef QPair<QString, QString> ValuePair;
// A list of valuePairs (pairs of two strings)
typedef QList<ValuePair> ValuePairList;

#endif // TYPEDEF_H

