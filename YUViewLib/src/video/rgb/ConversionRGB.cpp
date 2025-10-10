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

#include "ConversionRGB.h"

#include <video/LimitedRangeToFullRange.h>

#include "ConversionFunctions.h"

namespace video::rgb
{

namespace
{

template <int bitDepth>
using UintValueType =
  typename std::conditional_t<bitDepth == 8,
                              uint8_t *,
                              std::conditional_t<bitDepth == 16, uint16_t *, uint32_t *>>;

int getOffsetToFirstByteOfComponent(const Channel         channel,
                                    const PixelFormatRGB &pixelFormat,
                                    const Size            frameSize)
{
  auto offset = pixelFormat.getChannelPosition(channel);
  if (pixelFormat.getDataLayout() == DataLayout::Planar)
    offset *= frameSize.width * frameSize.height;
  return offset;
}

void convertRGB565ToARGB(const QByteArray     &sourceBuffer,
                         const PixelFormatRGB &srcPixelFormat,
                         unsigned char        *targetBuffer,
                         const Size            frameSize,
                         const bool            componentInvert[4],
                         const int             componentScale[4],
                         const bool            limitedRange)
{
  auto       rawData    = reinterpret_cast<const unsigned char *>(sourceBuffer.data());
  const auto endianness = srcPixelFormat.getEndianness();

  for (unsigned i = 0; i < frameSize.width * frameSize.height; i++)
  {
    auto [r, g, b, a] = extractRGB565Value(rawData, endianness);

    // Scale from 565 to 8 bit
    r = r << 3;
    g = g << 2;
    b = b << 3;

    r = functions::clip(r * componentScale[0], 0, 255);
    g = functions::clip(g * componentScale[1], 0, 255);
    b = functions::clip(b * componentScale[2], 0, 255);

    if (componentInvert[0])
      r = (255 - r);
    if (componentInvert[1])
      g = (255 - g);
    if (componentInvert[2])
      b = (255 - b);

    if (limitedRange)
    {
      r = LimitedRangeToFullRange.at(r);
      g = LimitedRangeToFullRange.at(g);
      b = LimitedRangeToFullRange.at(b);
    }

    targetBuffer[0] = b;
    targetBuffer[1] = g;
    targetBuffer[2] = r;
    targetBuffer[3] = a;

    rawData += 2;
    targetBuffer += 4;
  }
}

// Convert the input format to the output RGBA format. Apply inversion, scaling,
// limited range conversion and alpha multiplication. The input can be any supported
// format. The output is always 8 bit ARGB little endian.
template <int bitDepth>
void convertRGBToARGB(const QByteArray     &sourceBuffer,
                      const PixelFormatRGB &srcPixelFormat,
                      unsigned char        *targetBuffer,
                      const Size            frameSize,
                      const bool            componentInvert[4],
                      const int             componentScale[4],
                      const bool            limitedRange,
                      const bool            outputHasAlpha,
                      const bool            premultiplyAlpha)
{
  const int  rightShift = bitDepth == 8 ? 0 : (srcPixelFormat.getBitsPerComponent() - 8);
  const auto offsetToNextValue =
    srcPixelFormat.getDataLayout() == DataLayout::Planar ? 1 : srcPixelFormat.getNrChannels();

  using InValueType   = UintValueType<bitDepth>;
  const auto setAlpha = outputHasAlpha && srcPixelFormat.hasAlpha();

  const auto rawData = (InValueType)sourceBuffer.data();

  auto srcR = rawData + getOffsetToFirstByteOfComponent(Channel::Red, srcPixelFormat, frameSize);
  auto srcG = rawData + getOffsetToFirstByteOfComponent(Channel::Green, srcPixelFormat, frameSize);
  auto srcB = rawData + getOffsetToFirstByteOfComponent(Channel::Blue, srcPixelFormat, frameSize);

  InValueType srcA = nullptr;
  if (setAlpha)
  {
    auto offsetA = srcPixelFormat.getChannelPosition(Channel::Alpha);
    if (srcPixelFormat.getDataLayout() == DataLayout::Planar)
      offsetA *= frameSize.width * frameSize.height;
    srcA = ((InValueType)sourceBuffer.data()) + offsetA;
  }

  const auto isBigEndian = bitDepth > 8 && srcPixelFormat.getEndianness() == Endianness::Big;
  for (unsigned i = 0; i < frameSize.width * frameSize.height; i++)
  {
    auto convertValue =
      [&isBigEndian, &rightShift](const InValueType sourceData, const int scale, const bool invert)
    {
      auto value = static_cast<int64_t>(sourceData[0]);
      if (isBigEndian)
        value = swapBytesEndianness<bitDepth>(value);
      value = ((value * scale) >> rightShift);
      value = functions::clip(value, 0, 255);
      if (invert)
        value = 255 - value;
      return value;
    };

    auto valR = convertValue(srcR, componentScale[0], componentInvert[0]);
    auto valG = convertValue(srcG, componentScale[1], componentInvert[1]);
    auto valB = convertValue(srcB, componentScale[2], componentInvert[2]);

    if (limitedRange)
    {
      valR = LimitedRangeToFullRange.at(valR);
      valG = LimitedRangeToFullRange.at(valG);
      valB = LimitedRangeToFullRange.at(valB);
      // No limited range for alpha
    }

    int valA = 255;
    if (setAlpha)
    {
      valA = convertValue(srcA, componentScale[3], componentInvert[3]);
      srcA += offsetToNextValue;

      if (premultiplyAlpha)
      {
        valR = ((valR * 255) * valA) / (255 * 255);
        valG = ((valG * 255) * valA) / (255 * 255);
        valB = ((valB * 255) * valA) / (255 * 255);
      }
    }

    srcR += offsetToNextValue;
    srcG += offsetToNextValue;
    srcB += offsetToNextValue;

    targetBuffer[0] = valB;
    targetBuffer[1] = valG;
    targetBuffer[2] = valR;
    targetBuffer[3] = valA;

    targetBuffer += 4;
  }
}

void convertPredefinedPixelFormatRGBPlaneToARGB(const QByteArray     &sourceBuffer,
                                                const PixelFormatRGB &srcPixelFormat,
                                                unsigned char        *targetBuffer,
                                                const Size            frameSize,
                                                const Channel         displayChannel,
                                                const int             scale,
                                                const bool            invert,
                                                const bool            limitedRange)
{
  auto rawData = reinterpret_cast<const unsigned char *>(sourceBuffer.data());

  for (unsigned i = 0; i < frameSize.width * frameSize.height; i++)
  {
    int byte1 = *rawData;
    int byte2 = *(rawData + 1);

    if (srcPixelFormat.getEndianness() == Endianness::Big)
      std::swap(byte1, byte2);

    const auto value = byte1 + (byte2 << 8);

    int greyscaleValue = 0;
    if (displayChannel == Channel::Red)
      greyscaleValue = ((value & 0b00000000'00011111) << 3);
    else if (displayChannel == Channel::Green)
      greyscaleValue = ((value & 0b00000111'11100000) >> 3);
    else if (displayChannel == Channel::Blue)
      greyscaleValue = ((value & 0b11111000'00000000) >> 8);

    greyscaleValue = functions::clip(greyscaleValue * scale, 0, 255);
    if (invert)
      greyscaleValue = (255 - greyscaleValue);
    if (limitedRange)
      greyscaleValue = LimitedRangeToFullRange.at(greyscaleValue);

    targetBuffer[0] = greyscaleValue;
    targetBuffer[1] = greyscaleValue;
    targetBuffer[2] = greyscaleValue;
    targetBuffer[3] = 255;

    rawData += 2;
    targetBuffer += 4;
  }
}

// Convert one single plane of the input format to RGBA. This is used to visualize the individual
// components.
template <int bitDepth>
void convertRGBPlaneToARGB(const QByteArray     &sourceBuffer,
                           const PixelFormatRGB &srcPixelFormat,
                           unsigned char        *targetBuffer,
                           const Size            frameSize,
                           const Channel         displayChannel,
                           const int             scale,
                           const bool            invert,
                           const bool            limitedRange)
{
  const auto shiftTo8Bit = srcPixelFormat.getBitsPerComponent() - 8;
  const auto offsetToNextValue =
    srcPixelFormat.getDataLayout() == DataLayout::Planar ? 1 : srcPixelFormat.getNrChannels();

  using InValueType = UintValueType<bitDepth>;

  auto       src                    = (InValueType)sourceBuffer.data();
  const auto displayComponentOffset = srcPixelFormat.getChannelPosition(displayChannel);
  if (srcPixelFormat.getDataLayout() == DataLayout::Planar)
    src += displayComponentOffset * frameSize.width * frameSize.height;
  else
    src += displayComponentOffset;

  for (size_t i = 0; i < frameSize.width * frameSize.height; i++)
  {
    auto val = static_cast<int64_t>(src[0]);
    if (bitDepth > 8 && srcPixelFormat.getEndianness() == Endianness::Big)
      val = swapBytesEndianness<bitDepth>(val);
    val = (val * scale) >> shiftTo8Bit;
    val = functions::clip(val, 0, 255);
    if (invert)
      val = 255 - val;
    if (limitedRange)
      val = LimitedRangeToFullRange.at(val);

    targetBuffer[0] = val;
    targetBuffer[1] = val;
    targetBuffer[2] = val;
    targetBuffer[3] = 255;

    src += offsetToNextValue;
    targetBuffer += 4;
  }
}

rgba_t getPixelValueForPredefiendFormat(const QByteArray     &sourceBuffer,
                                        const PixelFormatRGB &srcPixelFormat,
                                        const Size            frameSize,
                                        const QPoint         &pixelPos)
{
  const auto offsetPixelPos = frameSize.width * pixelPos.y() + pixelPos.x();
  const auto rawData =
    reinterpret_cast<const unsigned char *>(sourceBuffer.data() + offsetPixelPos * 2);

  int byte1 = *rawData;
  int byte2 = *(rawData + 1);

  if (srcPixelFormat.getEndianness() == Endianness::Big)
    std::swap(byte1, byte2);

  const auto value = byte1 + (byte2 << 8);

  int r = ((value & 0b00000000'00011111));
  int g = ((value & 0b00000111'11100000) >> 5);
  int b = ((value & 0b11111000'00000000) >> 11);

  return {r, g, b, 255};
}

template <int bitDepth>
rgba_t getPixelValue(const QByteArray     &sourceBuffer,
                     const PixelFormatRGB &srcPixelFormat,
                     const Size            frameSize,
                     const QPoint         &pixelPos)
{
  const auto offsetToNextValue =
    srcPixelFormat.getDataLayout() == DataLayout::Planar ? 1 : srcPixelFormat.getNrChannels();
  const auto offsetPixelPos = frameSize.width * pixelPos.y() + pixelPos.x();

  using InValueType = UintValueType<bitDepth>;

  const auto rawData  = (InValueType)sourceBuffer.data();
  auto       srcPixel = rawData + offsetPixelPos * offsetToNextValue;

  rgba_t value{};
  for (auto channel : {Channel::Red, Channel::Green, Channel::Blue, Channel::Alpha})
  {
    if (channel == Channel::Alpha && !srcPixelFormat.hasAlpha())
      continue;

    const auto offset = getOffsetToFirstByteOfComponent(channel, srcPixelFormat, frameSize);

    auto src = srcPixel + offset;
    auto val = (unsigned)src[0];
    if (bitDepth > 8 && srcPixelFormat.getEndianness() == Endianness::Big)
      val = swapBytesEndianness<bitDepth>(val);
    value[channel] = val;
  }

  return value;
}

} // namespace

void convertInputRGBToARGB(const QByteArray     &sourceBuffer,
                           const PixelFormatRGB &srcPixelFormat,
                           unsigned char        *targetBuffer,
                           const Size            frameSize,
                           const bool            componentInvert[4],
                           const int             componentScale[4],
                           const bool            limitedRange,
                           const bool            outputHasAlpha,
                           const bool            premultiplyAlpha)
{
  const auto bitsPerComponent = srcPixelFormat.getBitsPerComponent();

  if (srcPixelFormat.getPredefinedPixelFormat())
    convertRGB565ToARGB(sourceBuffer,
                        srcPixelFormat,
                        targetBuffer,
                        frameSize,
                        componentInvert,
                        componentScale,
                        limitedRange);
  else if (bitsPerComponent == 8)
    convertRGBToARGB<8>(sourceBuffer,
                        srcPixelFormat,
                        targetBuffer,
                        frameSize,
                        componentInvert,
                        componentScale,
                        limitedRange,
                        outputHasAlpha,
                        premultiplyAlpha);
  else if (bitsPerComponent <= 16)
    convertRGBToARGB<16>(sourceBuffer,
                         srcPixelFormat,
                         targetBuffer,
                         frameSize,
                         componentInvert,
                         componentScale,
                         limitedRange,
                         outputHasAlpha,
                         premultiplyAlpha);
  else if (bitsPerComponent <= 32)
    convertRGBToARGB<32>(sourceBuffer,
                         srcPixelFormat,
                         targetBuffer,
                         frameSize,
                         componentInvert,
                         componentScale,
                         limitedRange,
                         outputHasAlpha,
                         premultiplyAlpha);
  else
    throw std::invalid_argument("Unable to perform conversion");
}

void convertSinglePlaneOfRGBToGreyscaleARGB(const QByteArray     &sourceBuffer,
                                            const PixelFormatRGB &srcPixelFormat,
                                            unsigned char        *targetBuffer,
                                            const Size            frameSize,
                                            const Channel         displayChannel,
                                            const int             scale,
                                            const bool            invert,
                                            const bool            limitedRange)
{
  const auto bitsPerComponent = srcPixelFormat.getBitsPerComponent();

  if (srcPixelFormat.getPredefinedPixelFormat())
  {
    convertPredefinedPixelFormatRGBPlaneToARGB(sourceBuffer,
                                               srcPixelFormat,
                                               targetBuffer,
                                               frameSize,
                                               displayChannel,
                                               scale,
                                               invert,
                                               limitedRange);
  }
  else if (bitsPerComponent == 8)
    convertRGBPlaneToARGB<8>(sourceBuffer,
                             srcPixelFormat,
                             targetBuffer,
                             frameSize,
                             displayChannel,
                             scale,
                             invert,
                             limitedRange);
  else if (bitsPerComponent <= 16)
    convertRGBPlaneToARGB<16>(sourceBuffer,
                              srcPixelFormat,
                              targetBuffer,
                              frameSize,
                              displayChannel,
                              scale,
                              invert,
                              limitedRange);
  else if (bitsPerComponent <= 32)
    convertRGBPlaneToARGB<32>(sourceBuffer,
                              srcPixelFormat,
                              targetBuffer,
                              frameSize,
                              displayChannel,
                              scale,
                              invert,
                              limitedRange);
  else
    throw std::invalid_argument("Invalid bit depth in pixel format for conversion");
}

rgba_t getPixelValueFromBuffer(const QByteArray     &sourceBuffer,
                               const PixelFormatRGB &srcPixelFormat,
                               const Size            frameSize,
                               const QPoint         &pixelPos)
{
  const auto bitsPerComponent = srcPixelFormat.getBitsPerComponent();

  if (srcPixelFormat.getPredefinedPixelFormat())
    return getPixelValueForPredefiendFormat(sourceBuffer, srcPixelFormat, frameSize, pixelPos);
  else if (bitsPerComponent == 8)
    return getPixelValue<8>(sourceBuffer, srcPixelFormat, frameSize, pixelPos);
  else if (bitsPerComponent <= 16)
    return getPixelValue<16>(sourceBuffer, srcPixelFormat, frameSize, pixelPos);
  else if (bitsPerComponent <= 32)
    return getPixelValue<32>(sourceBuffer, srcPixelFormat, frameSize, pixelPos);

  throw std::invalid_argument("Unable to perform conversion");
}

} // namespace video::rgb
