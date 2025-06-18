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

template <typename T> struct DataPointers
{
  T *r;
  T *g;
  T *b;
};

template <typename T>
DataPointers<T> calculatePointersToStartOfComponents(const InputFrameParameters frameParameters,
                                                     const PixelFormatRGB      &pixelFormat)
{
  const auto posR = pixelFormat.getChannelPosition(Channel::Red);
  const auto posG = pixelFormat.getChannelPosition(Channel::Green);
  const auto posB = pixelFormat.getChannelPosition(Channel::Blue);

  const auto castDataPointer = reinterpret_cast<T *>(frameParameters.rawDataItem->data());

  if (pixelFormat.getDataLayout() == DataLayout::Planar)
  {
    const auto offsetToNextPlane =
      frameParameters.frameSize.width * frameParameters.frameSize.height;

    return DataPointers<T>({.r = castDataPointer + (posR * offsetToNextPlane),
                            .g = castDataPointer + (posG * offsetToNextPlane),
                            .b = castDataPointer + (posB * offsetToNextPlane)});
  }

  return DataPointers<T>(
    {.r = castDataPointer + posR, .g = castDataPointer + posG, .b = castDataPointer + posB});
}

std::pair<QImage, MSE>
calculateDifferencePredefinedPixelFormat(const InputFrameParameters &frame1,
                                         const InputFrameParameters &frame2,
                                         const PredefinedPixelFormat predefinedPixelFormat,
                                         const int                   amplificationFactor,
                                         const bool                  markDifference)
{
  if (predefinedPixelFormat != PredefinedPixelFormat::RGB565 &&
      predefinedPixelFormat != PredefinedPixelFormat::RGB565BE)
    return {};

  const auto frameSize = Size(std::min(frame1.frameSize.width, frame2.frameSize.width),
                              std::min(frame1.frameSize.height, frame2.frameSize.height));

  auto outputImage =
    QImage(QSize(frameSize.width, frameSize.height), functionsGui::platformImageFormat(false));

  MSE mse;

  // Todo: Add code here!

  return {outputImage, mse};
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

  const auto components1 = calculatePointersToStartOfComponents<T>(frame1, pixelFormat);
  const auto components2 = calculatePointersToStartOfComponents<T>(frame2, pixelFormat);

  const auto frameSize   = Size(std::min(frame1.frameSize.width, frame2.frameSize.width),
                              std::min(frame1.frameSize.height, frame2.frameSize.height));
  auto       outputImage = QImage(QSize(frameSize.width, frameSize.height),
                            functionsGui::platformImageFormat(pixelFormat.hasAlpha()));
  MSE        mse;

  unsigned char *restrict dst = outputImage.bits();
  const auto offsetToNextValue =
    (pixelFormat.getDataLayout() == DataLayout::Planar ? 1 : pixelFormat.getNrChannels());

  for (int y = 0; y < frameSize.height; ++y)
  {
    for (int x = 0; x < frameSize.width; ++x)
    {
      const auto offsetCoordinate = frameSize.width * y + x;

      const auto r1 = static_cast<int>(*(components1.r + offsetToNextValue * offsetCoordinate));
      const auto g1 = static_cast<int>(*(components1.g + offsetToNextValue * offsetCoordinate));
      const auto b1 = static_cast<int>(*(components1.b + offsetToNextValue * offsetCoordinate));

      const auto r2 = static_cast<int>(*(components2.r + offsetToNextValue * offsetCoordinate));
      const auto g2 = static_cast<int>(*(components2.g + offsetToNextValue * offsetCoordinate));
      const auto b2 = static_cast<int>(*(components2.b + offsetToNextValue * offsetCoordinate));

      const auto deltaR = r1 - r2;
      const auto deltaG = g1 - g2;
      const auto deltaB = b1 - b2;

      mse.r += deltaR * deltaR;
      mse.g += deltaG * deltaG;
      mse.b += deltaB * deltaB;

      if (markDifference)
      {
        // Just mark if there is a difference
        dst[0] = (deltaB == 0) ? 0 : 255;
        dst[1] = (deltaG == 0) ? 0 : 255;
        dst[2] = (deltaR == 0) ? 0 : 255;
      }
      else
      {
        // We want to see the difference
        dst[0] = functions::clip(128 + deltaB * amplificationFactor, 0, 255);
        dst[1] = functions::clip(128 + deltaG * amplificationFactor, 0, 255);
        dst[2] = functions::clip(128 + deltaR * amplificationFactor, 0, 255);
      }

      dst[3] = 255;
      dst += 4;
    }
  }

  return {outputImage, mse};
}

} // namespace

std::pair<QImage, MSE> calculateDifferenceAndMSE(const InputFrameParameters &frame1,
                                                 const InputFrameParameters &frame2,
                                                 const PixelFormatRGB       &pixelFormat,
                                                 const int                   amplificationFactor,
                                                 const bool                  markDifference)
{

  if (pixelFormat.getPredefinedPixelFormat())
    return calculateDifferencePredefinedPixelFormat(
      frame1, frame2, *pixelFormat.getPredefinedPixelFormat(), amplificationFactor, markDifference);

  const auto bitDepth = pixelFormat.getBitsPerComponent();

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
