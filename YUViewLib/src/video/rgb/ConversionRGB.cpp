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

namespace video::rgb
{

namespace
{

template <typename T> T swapLowestBytes(const T &val)
{
  return ((val & 0xff) << 8) + ((val & 0xff00) >> 8);
};

int getOffsetToFirstByteOfComponent(const Channel         channel,
                                    const PixelFormatRGB &pixelFormat,
                                    const Size            frameSize)
{
  auto offset = pixelFormat.getChannelPosition(channel);
  if (pixelFormat.getDataLayout() == DataLayout::Planar)
    offset *= frameSize.width * frameSize.height;
  return offset;
}

// Convert the input format to the output RGBA format. Apply inversion, scaling,
// limited range conversion and alpha multiplication. The input can be any supported
// format. The output is always 8 bit ARGB little endian.
template <int bitDepth>
void convertRGBToARGB(const QByteArray &    sourceBuffer,
                      const PixelFormatRGB &srcPixelFormat,
                      unsigned char *       targetBuffer,
                      const Size            frameSize,
                      const bool            componentInvert[4],
                      const int             componentScale[4],
                      const bool            limitedRange,
                      const bool            outputHasAlpha,
                      const bool            premultiplyAlpha)
{
  const int  rightShift = bitDepth == 8 ? 0 : (srcPixelFormat.getBitsPerSample() - 8);
  const auto offsetToNextValue =
      srcPixelFormat.getDataLayout() == DataLayout::Planar ? 1 : srcPixelFormat.nrChannels();

  typedef typename std::conditional<bitDepth == 8, uint8_t *, uint16_t *>::type InValueType;
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

  for (unsigned i = 0; i < frameSize.width * frameSize.height; i++)
  {
    const auto isBigEndian  = bitDepth > 8 && srcPixelFormat.getEndianess() == Endianness::Big;
    auto       convertValue = [&isBigEndian, &rightShift](
                            const InValueType sourceData, const int scale, const bool invert) {
      auto value = static_cast<int>(sourceData[0]);
      if (isBigEndian)
        value = swapLowestBytes(value);
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

// Convert one single plane of the input format to RGBA. This is used to visualize the individual
// components.
template <int bitDepth>
void convertRGBPlaneToARGB(const QByteArray &    sourceBuffer,
                           const PixelFormatRGB &srcPixelFormat,
                           unsigned char *       targetBuffer,
                           const Size            frameSize,
                           const int             scale,
                           const bool            invert,
                           const int             displayComponentOffset,
                           const bool            limitedRange)
{
  const auto shiftTo8Bit = srcPixelFormat.getBitsPerSample() - 8;
  const auto offsetToNextValue =
      srcPixelFormat.getDataLayout() == DataLayout::Planar ? 1 : srcPixelFormat.nrChannels();

  typedef typename std::conditional<bitDepth == 8, uint8_t *, uint16_t *>::type InValueType;

  auto src = (InValueType)sourceBuffer.data();
  if (srcPixelFormat.getDataLayout() == DataLayout::Planar)
    src += displayComponentOffset * frameSize.width * frameSize.height;
  else
    src += displayComponentOffset;

  for (unsigned i = 0; i < frameSize.width * frameSize.height; i++)
  {
    auto val = (int)src[0];
    if (bitDepth > 8 && srcPixelFormat.getEndianess() == Endianness::Big)
      val = swapLowestBytes(val);
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

template <int bitDepth>
rgba_t getPixelValue(const QByteArray &    sourceBuffer,
                     const PixelFormatRGB &srcPixelFormat,
                     const Size            frameSize,
                     const QPoint &        pixelPos)
{
  const auto offsetToNextValue =
      srcPixelFormat.getDataLayout() == DataLayout::Planar ? 1 : srcPixelFormat.nrChannels();
  const auto offsetCoordinate = frameSize.width * pixelPos.y() + pixelPos.x();

  typedef typename std::conditional<bitDepth == 8, uint8_t *, uint16_t *>::type InValueType;

  rgba_t value{};
  for (auto channel : {Channel::Red, Channel::Green, Channel::Blue, Channel::Alpha})
  {
    if (channel == Channel::Alpha && !srcPixelFormat.hasAlpha())
      continue;

    const auto offset = getOffsetToFirstByteOfComponent(channel, srcPixelFormat, frameSize);

    auto src = ((InValueType)sourceBuffer.data()) + offset + offsetCoordinate * offsetToNextValue;
    auto val = (unsigned)src[0];
    if (bitDepth > 8 && srcPixelFormat.getEndianess() == Endianness::Big)
      val = swapLowestBytes(val);
    value[channel] = val;
  }

  return value;
}

} // namespace

void convertInputRGBToARGB(const QByteArray &    sourceBuffer,
                           const PixelFormatRGB &srcPixelFormat,
                           unsigned char *       targetBuffer,
                           const Size            frameSize,
                           const bool            componentInvert[4],
                           const int             componentScale[4],
                           const bool            limitedRange,
                           const bool            outputHasAlpha,
                           const bool            premultiplyAlpha)
{
  const auto bitsPerSample = srcPixelFormat.getBitsPerSample();
  if (bitsPerSample < 8 || bitsPerSample > 16)
    throw std::invalid_argument("Invalid bit depth in pixel format for conversion");

  if (bitsPerSample == 8)
    convertRGBToARGB<8>(sourceBuffer,
                        srcPixelFormat,
                        targetBuffer,
                        frameSize,
                        componentInvert,
                        componentScale,
                        limitedRange,
                        outputHasAlpha,
                        premultiplyAlpha);
  else
    convertRGBToARGB<16>(sourceBuffer,
                         srcPixelFormat,
                         targetBuffer,
                         frameSize,
                         componentInvert,
                         componentScale,
                         limitedRange,
                         outputHasAlpha,
                         premultiplyAlpha);
}

void convertSinglePlaneOfRGBToGreyscaleARGB(const QByteArray &    sourceBuffer,
                                            const PixelFormatRGB &srcPixelFormat,
                                            unsigned char *       targetBuffer,
                                            const Size            frameSize,
                                            const int             scale,
                                            const bool            invert,
                                            const int             displayComponentOffset,
                                            const bool            limitedRange)
{
  const auto bitsPerSample = srcPixelFormat.getBitsPerSample();
  if (bitsPerSample < 8 || bitsPerSample > 16)
    throw std::invalid_argument("Invalid bit depth in pixel format for conversion");

  if (bitsPerSample == 8)
    convertRGBPlaneToARGB<8>(sourceBuffer,
                             srcPixelFormat,
                             targetBuffer,
                             frameSize,
                             scale,
                             invert,
                             displayComponentOffset,
                             limitedRange);
  else
    convertRGBPlaneToARGB<16>(sourceBuffer,
                              srcPixelFormat,
                              targetBuffer,
                              frameSize,
                              scale,
                              invert,
                              displayComponentOffset,
                              limitedRange);
}

rgba_t getPixelValueFromBuffer(const QByteArray &    sourceBuffer,
                               const PixelFormatRGB &srcPixelFormat,
                               const Size            frameSize,
                               const QPoint &        pixelPos)
{
  const auto bitsPerSample = srcPixelFormat.getBitsPerSample();
  if (bitsPerSample < 8 || bitsPerSample > 16)
    throw std::invalid_argument("Invalid bit depth in pixel format for conversion");

  if (bitsPerSample == 8)
    return getPixelValue<8>(sourceBuffer, srcPixelFormat, frameSize, pixelPos);
  else
    return getPixelValue<16>(sourceBuffer, srcPixelFormat, frameSize, pixelPos);
}

} // namespace video::rgb
