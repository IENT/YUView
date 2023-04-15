/*  This file is part of YUView - The YUV player with advanced analytics toolset
 *   <https://github.com/IENT/YUView>
 *   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   In addition, as a special exception, the copyright holders give
 *   permission to link the code of portions of this program with the
 *   OpenSSL library under certain conditions as described in each
 *   individual source file, and distribute linked combinations including
 *   the two.
 *
 *   You must obey the GNU General Public License in all respects for all
 *   of the code used other than OpenSSL. If you modify file(s) with this
 *   exception, you may extend this exception to your version of the
 *   file(s), but you are not obligated to do so. If you do not wish to do
 *   so, delete this exception statement from your version. If you delete
 *   this exception statement from all source files in the program, then
 *   also delete it here.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "SpecializedConversions.h"

namespace video::yuv::conversion
{

namespace
{

// This is a specialized function that can convert 8 - bit YUV 4 : 2 : 0 to RGB888 using
// NearestNeighborInterpolation. The chroma must be 0 in x direction and 1 in y direction. No
// yuvMath is supported.
// TODO: Correct the chroma subsampling offset.
template <int bitDepth>
bool convertYUV420ToRGB(DataPointerPerChannel       inputPlanes,
                        const ConversionParameters &parameters,
                        DataPointer                 output)
{
  typedef typename std::conditional<bitDepth == 8, uint8_t *, uint16_t *>::type InValueType;
  static_assert(bitDepth == 8 || bitDepth == 10);
  constexpr auto rightShift = (bitDepth == 8) ? 0 : 2;

  const auto frameWidth  = size.width;
  const auto frameHeight = size.height;

  // For 4:2:0, w and h must be dividible by 2
  assert(frameWidth % 2 == 0 && frameHeight % 2 == 0);

  int componentLenghtY  = frameWidth * frameHeight;
  int componentLengthUV = componentLenghtY >> 2;
  Q_ASSERT(sourceBuffer.size() >= componentLenghtY + componentLengthUV +
                                      componentLengthUV); // YUV 420 must be (at least) 1.5*Y-area

#if SSE_CONVERSION_420_ALT
  quint8 *srcYRaw = (quint8 *)sourceBuffer.data();
  quint8 *srcURaw = srcYRaw + componentLenghtY;
  quint8 *srcVRaw = srcURaw + componentLengthUV;

  quint8 *dstBuffer       = (quint8 *)targetBuffer.data();
  quint32 dstBufferStride = frameWidth * 4;

  yuv420_to_argb8888(srcYRaw,
                     srcURaw,
                     srcVRaw,
                     frameWidth,
                     frameWidth >> 1,
                     frameWidth,
                     frameHeight,
                     dstBuffer,
                     dstBufferStride);
  return false;
#endif

#if SSE_CONVERSION
  // Try to use SSE. If this fails use conventional algorithm

  if (frameWidth % 32 == 0 && frameHeight % 2 == 0)
  {
    // We can use 16byte aligned read/write operations

    quint8 *srcY = (quint8 *)sourceBuffer.data();
    quint8 *srcU = srcY + componentLenghtY;
    quint8 *srcV = srcU + componentLengthUV;

    __m128i yMult  = _mm_set_epi16(75, 75, 75, 75, 75, 75, 75, 75);
    __m128i ySub   = _mm_set_epi16(16, 16, 16, 16, 16, 16, 16, 16);
    __m128i ugMult = _mm_set_epi16(25, 25, 25, 25, 25, 25, 25, 25);
    //__m128i sub16  = _mm_set_epi8(16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16);
    __m128i sub128 = _mm_set_epi8(
        128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128);

    //__m128i test = _mm_set_epi8(128, 0, 1, 2, 3, 245, 254, 255, 128, 128, 128, 128, 128, 128, 128,
    // 128);

    __m128i y, u, v, uMult, vMult;
    __m128i RGBOut0, RGBOut1, RGBOut2;
    __m128i tmp;

    for (int yh = 0; yh < frameHeight / 2; yh++)
    {
      for (int x = 0; x < frameWidth / 32; x += 32)
      {
        // Load 16 bytes U/V
        u = _mm_load_si128((__m128i *)&srcU[x / 2]);
        v = _mm_load_si128((__m128i *)&srcV[x / 2]);
        // Subtract 128 from each U/V value (16 values)
        u = _mm_sub_epi8(u, sub128);
        v = _mm_sub_epi8(v, sub128);

        // Load 16 bytes Y from this line and the next one
        y = _mm_load_si128((__m128i *)&srcY[x]);

        // Get the lower 8 (8bit signed) Y values and put them into a 16bit register
        tmp = _mm_srai_epi16(_mm_unpacklo_epi8(y, y), 8);
        // Subtract 16 and multiply by 75
        tmp = _mm_sub_epi16(tmp, ySub);
        tmp = _mm_mullo_epi16(tmp, yMult);

        // Now to add them to the 16 bit RGB output values
        RGBOut0 = _mm_shuffle_epi32(tmp, _MM_SHUFFLE(1, 0, 1, 0));
        RGBOut0 = _mm_shufflelo_epi16(RGBOut0, _MM_SHUFFLE(1, 0, 0, 0));
        RGBOut0 = _mm_shufflehi_epi16(RGBOut0, _MM_SHUFFLE(2, 2, 1, 1));

        RGBOut1 = _mm_shuffle_epi32(tmp, _MM_SHUFFLE(2, 1, 2, 1));
        RGBOut1 = _mm_shufflelo_epi16(RGBOut1, _MM_SHUFFLE(1, 1, 1, 0));
        RGBOut1 = _mm_shufflehi_epi16(RGBOut1, _MM_SHUFFLE(3, 2, 2, 2));

        RGBOut2 = _mm_shuffle_epi32(tmp, _MM_SHUFFLE(3, 2, 3, 2));
        RGBOut2 = _mm_shufflelo_epi16(RGBOut2, _MM_SHUFFLE(2, 2, 1, 1));
        RGBOut2 = _mm_shufflehi_epi16(RGBOut2, _MM_SHUFFLE(3, 3, 3, 2));

        // y2 = _mm_load_si128((__m128i *) &srcY[x + 16]);

        // --- Start with the left 8 values from U/V

        // Get the lower 8 (8bit signed) U/V values and put them into a 16bit register
        uMult = _mm_srai_epi16(_mm_unpacklo_epi8(u, u), 8);
        vMult = _mm_srai_epi16(_mm_unpacklo_epi8(v, v), 8);

        // Multiply

        /*y3 = _mm_load_si128((__m128i *) &srcY[x + frameWidth]);
        y4 = _mm_load_si128((__m128i *) &srcY[x + frameWidth + 16]);*/
      }
    }

    return true;
  }
#endif

  static unsigned char *clip_buf = clp_buf + 384;
  if (!clp_buf_initialized)
    initClippingTable();

  unsigned char *restrict dst = targetBuffer;

  // Get/set the parameters used for YUV -> RGB conversion
  const bool fullRange = isFullRange(conversionSettings.colorConversion);
  const int  yOffset   = (fullRange ? 0 : 16);
  const int  cZero     = 128;
  int        RGBConv[5];
  getColorConversionCoefficients(conversionSettings.colorConversion, RGBConv);

  // Get pointers to the source and the output array
  const bool uPplaneFirst =
      (format.getPlaneOrder() == PlaneOrder::YUV ||
       format.getPlaneOrder() == PlaneOrder::YUVA); // Is the U plane the first or the second?
  const auto *restrict srcY = InValueType(sourceBuffer.data());
  const auto *restrict srcU =
      uPplaneFirst ? srcY + componentLenghtY : srcY + componentLenghtY + componentLengthUV;
  const auto *restrict srcV =
      uPplaneFirst ? srcY + componentLenghtY + componentLengthUV : srcY + componentLenghtY;

  for (unsigned yh = 0; yh < frameHeight / 2; yh++)
  {
    // Process two lines at once, always 4 RGB values at a time (they have the same U/V components)

    int dstAddr1  = yh * 2 * frameWidth * 4;       // The RGB output address of line yh*2
    int dstAddr2  = (yh * 2 + 1) * frameWidth * 4; // The RGB output address of line yh*2+1
    int srcAddrY1 = yh * 2 * frameWidth;           // The Y source address of line yh*2
    int srcAddrY2 = (yh * 2 + 1) * frameWidth;     // The Y source address of line yh*2+1
    int srcAddrUV = yh * frameWidth / 2; // The UV source address of both lines (UV are identical)

    for (unsigned xh = 0, x = 0; xh < frameWidth / 2; xh++, x += 2)
    {
      // Process four pixels (the ones for which U/V are valid

      // Load UV and pre-multiply
      const int U_tmp_G = (((int)srcU[srcAddrUV + xh] >> rightShift) - cZero) * RGBConv[2];
      const int U_tmp_B = (((int)srcU[srcAddrUV + xh] >> rightShift) - cZero) * RGBConv[4];
      const int V_tmp_R = (((int)srcV[srcAddrUV + xh] >> rightShift) - cZero) * RGBConv[1];
      const int V_tmp_G = (((int)srcV[srcAddrUV + xh] >> rightShift) - cZero) * RGBConv[3];

      // Pixel top left
      {
        const int Y_tmp = (((int)srcY[srcAddrY1 + x] >> rightShift) - yOffset) * RGBConv[0];

        const int R_tmp = (Y_tmp + V_tmp_R) >> 16;
        const int G_tmp = (Y_tmp + U_tmp_G + V_tmp_G) >> 16;
        const int B_tmp = (Y_tmp + U_tmp_B) >> 16;

        dst[dstAddr1]     = clip_buf[B_tmp];
        dst[dstAddr1 + 1] = clip_buf[G_tmp];
        dst[dstAddr1 + 2] = clip_buf[R_tmp];
        dst[dstAddr1 + 3] = 255;
        dstAddr1 += 4;
      }
      // Pixel top right
      {
        const int Y_tmp = (((int)srcY[srcAddrY1 + x + 1] >> rightShift) - yOffset) * RGBConv[0];

        const int R_tmp = (Y_tmp + V_tmp_R) >> 16;
        const int G_tmp = (Y_tmp + U_tmp_G + V_tmp_G) >> 16;
        const int B_tmp = (Y_tmp + U_tmp_B) >> 16;

        dst[dstAddr1]     = clip_buf[B_tmp];
        dst[dstAddr1 + 1] = clip_buf[G_tmp];
        dst[dstAddr1 + 2] = clip_buf[R_tmp];
        dst[dstAddr1 + 3] = 255;
        dstAddr1 += 4;
      }
      // Pixel bottom left
      {
        const int Y_tmp = (((int)srcY[srcAddrY2 + x] >> rightShift) - yOffset) * RGBConv[0];

        const int R_tmp = (Y_tmp + V_tmp_R) >> 16;
        const int G_tmp = (Y_tmp + U_tmp_G + V_tmp_G) >> 16;
        const int B_tmp = (Y_tmp + U_tmp_B) >> 16;

        dst[dstAddr2]     = clip_buf[B_tmp];
        dst[dstAddr2 + 1] = clip_buf[G_tmp];
        dst[dstAddr2 + 2] = clip_buf[R_tmp];
        dst[dstAddr2 + 3] = 255;
        dstAddr2 += 4;
      }
      // Pixel bottom right
      {
        const int Y_tmp = (((int)srcY[srcAddrY2 + x + 1] >> rightShift) - yOffset) * RGBConv[0];

        const int R_tmp = (Y_tmp + V_tmp_R) >> 16;
        const int G_tmp = (Y_tmp + U_tmp_G + V_tmp_G) >> 16;
        const int B_tmp = (Y_tmp + U_tmp_B) >> 16;

        dst[dstAddr2]     = clip_buf[B_tmp];
        dst[dstAddr2 + 1] = clip_buf[G_tmp];
        dst[dstAddr2 + 2] = clip_buf[R_tmp];
        dst[dstAddr2 + 3] = 255;
        dstAddr2 += 4;
      }
    }
  }

  return true;
}

} // namespace

void yuvToRGB4208Or10BitNearestNeighbourNoMath(const DataPointerPerChannel &inputPlanes,
                                               const ConversionParameters & parameters,
                                               DataPointer                  output)
{
  if (parameters.bitsPerSample == 8)
    convertYUV420ToRGB<8>(inputPlanes, parameters, output);
  else if (parameters.bitsPerSample == 10)
    convertYUV420ToRGB<10>(inputPlanes, parameters, output);
}

} // namespace video::yuv::conversion
