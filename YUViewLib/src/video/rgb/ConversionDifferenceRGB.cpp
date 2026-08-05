/*  This file is part of YUView - The YUV player with advanced analytics toolset
 *   <https://github.com/IENT/YUView>
 *   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
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

#include "ConversionDifferenceRGB.h"

#include <common/Functions.h>
#include <common/FunctionsGui.h>

#include "ConversionFunctions.h"

#include <type_traits>

// Restrict is basically a promise to the compiler that for the scope of the pointer, the target of
// the pointer will only be accessed through that pointer (and pointers copied from it).
#if __STDC__ != 1
#define restrict __restrict /* use implementation __ format */
#else
#ifndef __STDC_VERSION__
#define restrict __restrict /* use implementation __ format */
#else
#if __STDC_VERSION__ < 199901L
#define restrict __restrict /* use implementation __ format */
#else
#/* all ok */
#endif
#endif
#endif

namespace video::rgb
{

namespace
{

using RenderValue = std::tuple<unsigned char, unsigned char, unsigned char>;

/**
 * @brief Map a signed RGBA difference to an 8-bit display RGB triple.
 *
 * @param delta Signed per-channel difference
 * @param markDifference When true, any non-zero channel becomes 255
 * @param amplificationFactor Linear gain applied around mid-gray (128)
 * @return Display RGB values as (R, G, B)
 */
RenderValue convertDeltaToRenderValue(const rgba_diff_t &delta,
                                      const bool         markDifference,
                                      const int          amplificationFactor)
{
  if (markDifference)
  {
    const auto r = (delta.r == 0) ? 0 : 255;
    const auto g = (delta.g == 0) ? 0 : 255;
    const auto b = (delta.b == 0) ? 0 : 255;
    return {static_cast<unsigned char>(r),
            static_cast<unsigned char>(g),
            static_cast<unsigned char>(b)};
  }

  const auto r = functions::clip(128 + delta.r * amplificationFactor, 0, 255);
  const auto g = functions::clip(128 + delta.g * amplificationFactor, 0, 255);
  const auto b = functions::clip(128 + delta.b * amplificationFactor, 0, 255);
  return {static_cast<unsigned char>(r),
          static_cast<unsigned char>(g),
          static_cast<unsigned char>(b)};
}

/**
 * @brief Read one RGBA sample from component pointers and correct endianness if needed.
 *
 * @tparam T Sample storage type
 * @param dataPointers Pointers to the current R/G/B(/A) samples
 * @param pixelFormat Source pixel format (endianness + alpha presence)
 * @return Host-endian rgba_t sample
 */
template <typename T>
rgba_t getRGBAndConvertEndianness(const DataPointers<T> dataPointers,
                                  const PixelFormatRGB &pixelFormat)
{
  constexpr auto bitDepth =
      (std::is_same_v<T, uint8_t> ? 8 : (std::is_same_v<T, uint16_t> ? 16 : 32));

  auto r = *dataPointers.r;
  auto g = *dataPointers.g;
  auto b = *dataPointers.b;
  T    a = 0;

  if (pixelFormat.getEndianess() == Endianness::Big)
  {
    r = swapBytesEndianness<bitDepth>(r);
    g = swapBytesEndianness<bitDepth>(g);
    b = swapBytesEndianness<bitDepth>(b);
  }

  if (pixelFormat.hasAlpha())
  {
    a = *dataPointers.a;
    if (pixelFormat.getEndianess() == Endianness::Big)
      a = swapBytesEndianness<bitDepth>(a);
  }

  return rgba_t{static_cast<unsigned>(r),
                static_cast<unsigned>(g),
                static_cast<unsigned>(b),
                static_cast<unsigned>(a)};
}

/**
 * @brief Typed implementation of RGB difference + MSE calculation.
 *
 * @tparam T Sample storage type matching the source bit depth
 * @param frame1 First input frame
 * @param frame2 Second input frame
 * @param pixelFormat Shared pixel format
 * @param amplificationFactor Visualization gain
 * @param markDifference Binary difference marking mode
 * @return Difference image and MSE
 */
template <typename T>
std::pair<QImage, MSE> calculateDifferenceAndMSE(const InputFrameParameters &frame1,
                                                 const InputFrameParameters &frame2,
                                                 const PixelFormatRGB       &pixelFormat,
                                                 const int                   amplificationFactor,
                                                 const bool                  markDifference)
{
  static_assert(std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t> ||
                std::is_same_v<T, uint32_t>);

  auto dataPointers1 =
      calculatePointersToStartOfComponents<T>(frame1.rawDataItem, frame1.frameSize, pixelFormat);
  auto dataPointers2 =
      calculatePointersToStartOfComponents<T>(frame2.rawDataItem, frame2.frameSize, pixelFormat);

  const auto frameSize = Size(std::min(frame1.frameSize.width, frame2.frameSize.width),
                              std::min(frame1.frameSize.height, frame2.frameSize.height));
  auto       outputImage =
      QImage(QSize(static_cast<int>(frameSize.width), static_cast<int>(frameSize.height)),
             functionsGui::platformImageFormat(pixelFormat.hasAlpha()));
  SSE sse;

  unsigned char *restrict dst = outputImage.bits();
  const auto offsetToNextValue =
      (pixelFormat.getDataLayout() == DataLayout::Planar ? 1
                                                         : static_cast<int>(pixelFormat.nrChannels()));

  for (unsigned i = 0; i < frameSize.width * frameSize.height; ++i)
  {
    const auto rgb1 = getRGBAndConvertEndianness(dataPointers1, pixelFormat);
    const auto rgb2 = getRGBAndConvertEndianness(dataPointers2, pixelFormat);

    const auto delta = makeDiff(rgb1, rgb2);

    sse.addSample(delta);

    std::tie(dst[2], dst[1], dst[0]) =
        convertDeltaToRenderValue(delta, markDifference, amplificationFactor);
    dst[3] = 255;

    dataPointers1 += offsetToNextValue;
    dataPointers2 += offsetToNextValue;
    dst += 4;
  }

  return {outputImage, sse.getMSE(pixelFormat.hasAlpha())};
}

} // namespace

void PrintTo(const MSE &mse, std::ostream *os)
{
  *os << "MSE(r=" << mse.r << ", g=" << mse.g << ", b=" << mse.b << ", a=" << mse.a << ")";
}

std::pair<QImage, MSE> calculateDifferenceAndMSE(const InputFrameParameters &frame1,
                                                 const InputFrameParameters &frame2,
                                                 const PixelFormatRGB       &pixelFormat,
                                                 const int                   amplificationFactor,
                                                 const bool                  markDifference)
{
  // Local PixelFormatRGB has no PredefinedPixelFormat (e.g. RGB565) path; dispatch by bit depth.
  if (pixelFormat.getBitsPerSample() == 8)
    return calculateDifferenceAndMSE<uint8_t>(
        frame1, frame2, pixelFormat, amplificationFactor, markDifference);

  if (pixelFormat.getBitsPerSample() <= 16)
    return calculateDifferenceAndMSE<uint16_t>(
        frame1, frame2, pixelFormat, amplificationFactor, markDifference);

  return calculateDifferenceAndMSE<uint32_t>(
      frame1, frame2, pixelFormat, amplificationFactor, markDifference);
}

} // namespace video::rgb
