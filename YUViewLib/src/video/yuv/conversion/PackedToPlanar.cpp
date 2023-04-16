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

#pragma once

#include "PackedToPlanar.h"

namespace video::yuv::conversion
{

namespace
{

PlanarDataAndFormat convertV210PackedToPlanar(ConstDataPointer src, const Size frameSize)
{
  // There are 6 pixels values per 16 bytes in the input.
  // 6 Values (6 Y, 3 U/V) are packed like this (highest to lowest bit, each value is 10 bit):
  // Byte 0-3:   (2 zero bytes), Cr0, Y0, Cb0
  // Byte 4-7:   (2 zero bytes), Y2, Cb1, Y1
  // Byte 8-11:  (2 zero bytes), Cb2, Y3, Cr1
  // Byte 12-15: (2 zero bytes), Y5, Cr2, Y4

  // The output format is 422 10 bit planar
  const auto newFormat        = PixelFormatYUV(Subsampling::YUV_422, 10, PlaneOrder::YUV);
  const auto bytesPerOutFrame = newFormat.bytesPerFrame(frameSize);
  ByteVector data;
  data.resize(bytesPerOutFrame);

  const auto w = frameSize.width;
  const auto h = frameSize.height;

  auto widthRoundUp = (((w + 48 - 1) / 48) * 48);
  auto strideIn     = widthRoundUp / 6 * 16;

  auto *restrict dstY = data.data();
  auto *restrict dstU = dstY + w * h;
  auto *restrict dstV = dstU + w / 2 * h;

  for (unsigned y = 0; y < h; y++)
  {
    for (auto [xIn, xOutY, xOutUV] = std::tuple{0u, 0u, 0u}; xOutY < w;
         xOutY += 6, xOutUV += 3, xIn += 16)
    {
      auto           xw0 = xIn;
      unsigned short Cb0 = src[xw0] + ((src[xw0 + 1] & 0x03) << 8);
      unsigned short Y0  = ((src[xw0 + 1] >> 2) & 0x3f) + ((src[xw0 + 2] & 0x0f) << 6);
      unsigned short Cr0 = (src[xw0 + 2] >> 4) + ((src[xw0 + 3] & 0x3f) << 4);

      auto           xw1 = xIn + 4;
      unsigned short Y1  = src[xw1] + ((src[xw1 + 1] & 0x03) << 8);
      unsigned short Cb1 = ((src[xw1 + 1] >> 2) & 0x3f) + ((src[xw1 + 2] & 0x0f) << 6);
      unsigned short Y2  = (src[xw1 + 2] >> 4) + ((src[xw1 + 3] & 0x3f) << 4);

      auto           xw2 = xIn + 8;
      unsigned short Cr1 = src[xw2] + ((src[xw2 + 1] & 0x03) << 8);
      unsigned short Y3  = ((src[xw2 + 1] >> 2) & 0x3f) + ((src[xw2 + 2] & 0x0f) << 6);
      unsigned short Cb2 = (src[xw2 + 2] >> 4) + ((src[xw2 + 3] & 0x3f) << 4);

      auto           xw3 = xIn + 12;
      unsigned short Y4  = src[xw3] + ((src[xw3 + 1] & 0x03) << 8);
      unsigned short Cr2 = ((src[xw3 + 1] >> 2) & 0x3f) + ((src[xw3 + 2] & 0x0f) << 6);
      unsigned short Y5  = (src[xw3 + 2] >> 4) + ((src[xw3 + 3] & 0x3f) << 4);

      dstY[xOutY]     = Y0;
      dstY[xOutY + 1] = Y1;
      dstU[xOutUV]    = Cb0;
      dstV[xOutUV]    = Cr0;

      if (xOutY + 2 < w)
      {
        dstY[xOutY + 2]  = Y2;
        dstY[xOutY + 3]  = Y3;
        dstU[xOutUV + 1] = Cb1;
        dstV[xOutUV + 1] = Cr1;

        if (xOutY + 4 < w)
        {
          dstY[xOutY + 4]  = Y4;
          dstY[xOutY + 5]  = Y5;
          dstU[xOutUV + 2] = Cb2;
          dstV[xOutUV + 2] = Cr2;
        }
      }
    }
    src += strideIn;
    dstY += w;
    dstU += w / 2;
    dstV += w / 2;
  }

  return {data, newFormat};
}

PlanarDataAndFormat convertYUVPackedToPlanar(ConstDataPointer      srcData,
                                             const PixelFormatYUV &pixelFormat,
                                             const Size            frameSize)
{
  const auto packing = pixelFormat.getPackingOrder();

  ByteVector data;

  const auto w = frameSize.width;
  const auto h = frameSize.height;

  // Bytes per sample
  const auto bps = (pixelFormat.getBitsPerSample() > 8) ? 2u : 1u;

  if (pixelFormat.getSubsampling() == Subsampling::YUV_422)
  {
    // The data is arranged in blocks of 4 samples. How many of these are there?
    const auto nr4Samples = w * h / 2;

    // What are the offsets withing the 4 samples for the components?
    const int oY = (packing == PackingOrder::YUYV || packing == PackingOrder::YVYU) ? 0 : 1;
    const int oU = (packing == PackingOrder::UYVY)   ? 0
                   : (packing == PackingOrder::YUYV) ? 1
                   : (packing == PackingOrder::VYUY) ? 2
                                                     : 3;
    const int oV = (packing == PackingOrder::VYUY)   ? 0
                   : (packing == PackingOrder::YVYU) ? 1
                   : (packing == PackingOrder::UYVY) ? 2
                                                     : 3;

    if (pixelFormat.getBitsPerSample() == 10 && pixelFormat.isBytePacking())
    {
      // Byte packing in 422 with 10 bit. So for each 2 pixels we have 4 10 bit values which
      // are exactly 5 bytes (40 bits).
      const auto newFormat  = PixelFormatYUV(Subsampling::YUV_422, 10, PlaneOrder::YUV);
      auto       outputSize = newFormat.bytesPerFrame(frameSize);
      data.resize(outputSize);

      auto *restrict src  = reinterpret_cast<const unsigned short *>(srcData);
      auto *restrict dstY = reinterpret_cast<unsigned short *>(data.data());
      auto *restrict dstU = dstY + w * h;
      auto *restrict dstV = dstU + w / 2 * h;

      for (unsigned i = 0; i < nr4Samples; i++)
      {
        unsigned short values[4];
        values[0] = (src[0] << 2) + (src[1] >> 6);
        values[1] = ((src[1] & 0x3f) << 4) + (src[2] >> 4);
        values[2] = ((src[2] & 0x0f) << 6) + (src[3] >> 2);
        values[3] = ((src[3] & 0x03) << 8) + src[4];

        *dstY++ = values[oY];
        *dstY++ = values[oY + 2];
        *dstU++ = values[oU];
        *dstV++ = values[oV];

        src += 5;
      }

      return {data, newFormat};
    }
    else
    {
      auto inputFrameSize = pixelFormat.bytesPerFrame(frameSize);
      data.resize(inputFrameSize);

      if (bps == 1)
      {
        auto *restrict src  = srcData;
        auto *restrict dstY = data.data();
        auto *restrict dstU = dstY + w * h;
        auto *restrict dstV = dstU + w / 2 * h;

        for (unsigned i = 0; i < nr4Samples; i++)
        {
          *dstY++ = src[oY];
          *dstY++ = src[oY + 2];
          *dstU++ = src[oU];
          *dstV++ = src[oV];
          src += 4; // Goto the next 4 samples
        }
      }
      else
      {
        auto *restrict           src  = reinterpret_cast<const unsigned short *>(srcData);
        auto *restrict           dstY = reinterpret_cast<unsigned short *>(data.data());
        unsigned short *restrict dstU = dstY + w * h;
        unsigned short *restrict dstV = dstU + w / 2 * h;

        for (unsigned i = 0; i < nr4Samples; i++)
        {
          *dstY++ = src[oY];
          *dstY++ = src[oY + 2];
          *dstU++ = src[oU];
          *dstV++ = src[oV];
          src += 4; // Goto the next 4 samples
        }
      }
    }
  }
  else if (pixelFormat.getSubsampling() == Subsampling::YUV_444)
  {
    auto inputFrameSize = pixelFormat.bytesPerFrame(frameSize);
    data.resize(inputFrameSize);

    // What are the offsets withing the 3 or 4 bytes per sample?
    const int oY = (packing == PackingOrder::AYUV) ? 1 : (packing == PackingOrder::VUYA) ? 2 : 0;
    const int oU = (packing == PackingOrder::YUV || packing == PackingOrder::YUVA ||
                    packing == PackingOrder::VUYA)
                       ? 1
                       : 2;
    const int oV = (packing == PackingOrder::YVU)    ? 1
                   : (packing == PackingOrder::AYUV) ? 3
                   : (packing == PackingOrder::VUYA) ? 0
                                                     : 2;

    const int offsetToNextSample =
        (packing == PackingOrder::YUV || packing == PackingOrder::YVU ? 3 : 4);

    if (bps == 1)
    {
      const auto *restrict src  = srcData;
      auto *restrict       dstY = data.data();
      auto *restrict       dstU = dstY + w * h;
      auto *restrict       dstV = dstU + w * h;

      for (unsigned i = 0; i < w * h; i++)
      {
        *dstY++ = src[oY];
        *dstU++ = src[oU];
        *dstV++ = src[oV];
        src += offsetToNextSample;
      }
    }
    else
    {
      auto *restrict src  = reinterpret_cast<const unsigned short *>(srcData);
      auto *restrict dstY = reinterpret_cast<unsigned short *>(data.data());
      auto *restrict dstU = dstY + w * h;
      auto *restrict dstV = dstU + w * h;

      for (unsigned i = 0; i < w * h; i++)
      {
        *dstY++ = src[oY];
        *dstU++ = src[oU];
        *dstV++ = src[oV];
        src += offsetToNextSample;
      }
    }
  }
  else
    throw std::logic_error("Invalid parameters for conversion");

  // The output buffer is planar with the same subsampling as before
  const auto newFormat = PixelFormatYUV(pixelFormat.getSubsampling(),
                                        pixelFormat.getBitsPerSample(),
                                        PlaneOrder::YUV,
                                        pixelFormat.isBigEndian(),
                                        pixelFormat.getChromaOffset(),
                                        pixelFormat.getChromaPacking());

  return {data, newFormat};
}

} // namespace

PlanarDataAndFormat convertPackedToPlanar(ConstDataPointer      source,
                                          const PixelFormatYUV &pixelFormat,
                                          const Size            frameSize)
{
  if (const auto predefinedFormat = pixelFormat.getPredefinedFormat())
  {
    if (*predefinedFormat == PredefinedPixelFormat::V210)
      return convertV210PackedToPlanar(source, frameSize);
  }
  else
    return convertYUVPackedToPlanar(source, pixelFormat, frameSize);
}

} // namespace video::yuv::conversion
