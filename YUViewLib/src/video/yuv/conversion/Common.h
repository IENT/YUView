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

#include <common/Typedef.h>
#include <video/yuv/PixelFormatYUV.h>
#include <video/yuv/conversion/Restrict.h>

namespace video::yuv::conversion
{

constexpr auto LUMA                     = 0;
constexpr auto CHROMA                   = 1;
constexpr auto CHROMA_U                 = 1;
constexpr auto CHROMA_V                 = 2;
constexpr auto OFFSET_TO_NEXT_RGB_VALUE = 4;

using MathParametersPerComponent  = std::array<MathParameters, 2>;
using DataPointer                 = unsigned char *           restrict;
using ConstDataPointer            = const unsigned char *restrict;
using DataPointerPerChannel       = std::array<ConstDataPointer, 4>;
using StridePerComponent          = std::array<int, 2>;
using NextValueOffsetPerComponent = std::array<int, 2>;

struct ConversionParameters
{
  Size                        frameSize{};
  StridePerComponent          stridePerComponent{};
  NextValueOffsetPerComponent nextValueOffsets{};
  MathParametersPerComponent  mathParameters{};
  int                         bitsPerSample{};
  bool                        bigEndian{};
  ColorConversion             colorConversion{};
  ChromaInterpolation         chromaInterpolation{};
};

inline int getValueFromSource(const unsigned char *restrict src,
                              const int                     idx,
                              const int                     bitsPerSample,
                              const bool                    bigEndian)
{
  if (bitsPerSample > 8)
    // Read two bytes in the right order
    return (bigEndian) ? src[idx * 2] << 8 | src[idx * 2 + 1]
                       : src[idx * 2] | src[idx * 2 + 1] << 8;
  else
    // Just read one byte
    return src[idx];
}

inline void
setValueInBuffer(DataPointer dst, const int val, const int idx, const int bps, const bool bigEndian)
{
  if (bps > 8)
  {
    // Write two bytes
    if (bigEndian)
    {
      dst[idx * 2]     = val >> 8;
      dst[idx * 2 + 1] = val & 0xff;
    }
    else
    {
      dst[idx * 2]     = val & 0xff;
      dst[idx * 2 + 1] = val >> 8;
    }
  }
  else
    // Write one byte
    dst[idx] = val;
}

inline int getChromaStrideInSamples(const PixelFormatYUV &pixelFormat, const Size frameSize)
{
  switch (pixelFormat.getChromaPacking())
  {
  case ChromaPacking::Planar:
    return (frameSize.width / pixelFormat.getSubsamplingHor());
  case ChromaPacking::PerLine:
  case ChromaPacking::PerValue:
    return frameSize.width;
  }
}

/* Apply the given transformation to the YUV sample. If invert is true, the sample is inverted at
 * the value defined by offset. If the scale is greater one, the values will be amplified relative
 * to the offset value. The input can be 8 to 16 bit. The output will be of the same bit depth. The
 * output is clamped to (0...clipMax).
 */
inline int applyMathToValue(const unsigned int value, const MathParameters &math, const int clipMax)
{
  if (!math.mathRequired())
    return value;

  auto newValue = static_cast<int>(value);
  if (math.invert)
    newValue = -(newValue - math.offset) * math.scale + math.offset; // Scale + Offset + Invert
  else
    newValue = (newValue - math.offset) * math.scale + math.offset; // Scale + Offset

  // Clip to 8 bit
  if (newValue < 0)
    newValue = 0;
  if (newValue > clipMax)
    newValue = clipMax;

  return newValue;
}

inline yuva_t
applyMathToValue(const yuva_t &value, const MathParametersPerComponent &math, const int clipMax)
{
  const auto y = applyMathToValue(value.Y, math.at(LUMA), clipMax);
  const auto u = applyMathToValue(value.U, math.at(CHROMA), clipMax);
  const auto v = applyMathToValue(value.V, math.at(CHROMA), clipMax);
  const auto a = value.A;

  return {y, u, v, a};
}

inline int clip8Bit(int val)
{
  if (val < 0)
    return 0;
  if (val > 255)
    return 255;
  return val;
}

} // namespace video::yuv::conversion
