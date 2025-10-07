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

#include "ConversionDifferenceRGB.h"

#include <common/FunctionsGui.h>
#include <cstdint>

#include "ConversionFunctions.h"
#include "video/PixelFormat.h"
#include "video/rgb/PixelFormatRGB.h"

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
RenderValue convertDeltaToRenderValue(const rgba_t &delta,
                                      const bool    markDifference,
                                      const int     amplificationFactor)
{
  if (markDifference)
  {
    const auto r = (delta.r == 0) ? 0 : 255;
    const auto g = (delta.g == 0) ? 0 : 255;
    const auto b = (delta.b == 0) ? 0 : 255;
    return {r, g, b};
  }
  else
  {
    const auto r = functions::clip(128 + delta.r * amplificationFactor, 0, 255);
    const auto g = functions::clip(128 + delta.g * amplificationFactor, 0, 255);
    const auto b = functions::clip(128 + delta.b * amplificationFactor, 0, 255);
    return {r, g, b};
  }
}

std::pair<QImage, MSE>
calculateDifferencePredefinedPixelFormat(const InputFrameParameters &frame1,
                                         const InputFrameParameters &frame2,
                                         const PredefinedPixelFormat predefinedPixelFormat,
                                         const Endianness            endianess,
                                         const int                   amplificationFactor,
                                         const bool                  markDifference)
{
  if (predefinedPixelFormat != PredefinedPixelFormat::RGB565)
    return {};

  const auto frameSize = Size(std::min(frame1.frameSize.width, frame2.frameSize.width),
                              std::min(frame1.frameSize.height, frame2.frameSize.height));

  auto outputImage =
    QImage(QSize(frameSize.width, frameSize.height), functionsGui::platformImageFormat(false));
  unsigned char *restrict dst = outputImage.bits();

  SSE sse;

  auto rawData1 = reinterpret_cast<const unsigned char *>(frame1.rawDataItem.data());
  auto rawData2 = reinterpret_cast<const unsigned char *>(frame2.rawDataItem.data());

  for (unsigned i = 0; i < frameSize.width * frameSize.height; ++i)
  {
    const auto rgb1 = extractRGB565Value(rawData1, endianess);
    const auto rgb2 = extractRGB565Value(rawData2, endianess);

    const auto delta = rgb1 - rgb2;

    sse.addSample(delta);

    std::tie(dst[2], dst[1], dst[0]) =
      convertDeltaToRenderValue(delta, markDifference, amplificationFactor);
    dst[3] = 255;

    rawData1 += 2;
    rawData2 += 2;
    dst += 4;
  }

  return {outputImage, sse.getMSE()};
}

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

  const auto frameSize   = Size(std::min(frame1.frameSize.width, frame2.frameSize.width),
                              std::min(frame1.frameSize.height, frame2.frameSize.height));
  auto       outputImage = QImage(QSize(frameSize.width, frameSize.height),
                            functionsGui::platformImageFormat(pixelFormat.hasAlpha()));
  SSE        sse;

  unsigned char *restrict dst = outputImage.bits();
  const auto offsetToNextValue =
    (pixelFormat.getDataLayout() == DataLayout::Planar ? 1 : pixelFormat.getNrChannels());

  for (unsigned i = 0; i < frameSize.width * frameSize.height; ++i)
  {
    const rgba_t rgb1 = {.r = static_cast<int>(*dataPointers1.r),
                         .g = static_cast<int>(*dataPointers1.g),
                         .b = static_cast<int>(*dataPointers1.b)};

    const rgba_t rgb2 = {.r = static_cast<int>(*dataPointers2.r),
                         .g = static_cast<int>(*dataPointers2.g),
                         .b = static_cast<int>(*dataPointers2.b)};

    const auto delta = rgb1 - rgb2;

    sse.addSample(delta);

    std::tie(dst[2], dst[1], dst[0]) =
      convertDeltaToRenderValue(delta, markDifference, amplificationFactor);
    dst[3] = 255;

    dataPointers1 += offsetToNextValue;
    dataPointers2 += offsetToNextValue;
    dst += 4;
  }

  return {outputImage, sse.getMSE()};
}

} // namespace

std::pair<QImage, MSE> calculateDifferenceAndMSE(const InputFrameParameters &frame1,
                                                 const InputFrameParameters &frame2,
                                                 const PixelFormatRGB       &pixelFormat,
                                                 const int                   amplificationFactor,
                                                 const bool                  markDifference)
{
  if (pixelFormat.getPredefinedPixelFormat())
    return calculateDifferencePredefinedPixelFormat(frame1,
                                                    frame2,
                                                    *pixelFormat.getPredefinedPixelFormat(),
                                                    pixelFormat.getEndianess(),
                                                    amplificationFactor,
                                                    markDifference);

  if (pixelFormat.getBitsPerComponent() == 8)
    return calculateDifferenceAndMSE<uint8_t>(
      frame1, frame2, pixelFormat, amplificationFactor, markDifference);

  if (pixelFormat.getBitsPerComponent() <= 16)
    return calculateDifferenceAndMSE<uint16_t>(
      frame1, frame2, pixelFormat, amplificationFactor, markDifference);

  return calculateDifferenceAndMSE<uint32_t>(
    frame1, frame2, pixelFormat, amplificationFactor, markDifference);
}

} // namespace video::rgb
