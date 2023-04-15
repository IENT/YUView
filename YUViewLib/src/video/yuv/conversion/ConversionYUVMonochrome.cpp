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

#include "ConversionYUVMonochrome.h"

#include <video/LimitedRangeToFullRange.h>

namespace video::yuv::conversion
{

// For every input sample in src, apply YUV transformation, (scale to 8 bit if required) and set the
// value as RGB (monochrome). inValSkip: skip this many values in the input for every value. For
// pure planar formats, this 1. If the UV components are interleaved, this is 2 or 3.
void yPlaneToRGBMonochrome(ConstDataPointer            inputPlane,
                           const ConversionParameters &parameters,
                           DataPointer                 output)
{
  const auto math = parameters.mathParameters.at(LUMA);

  const auto shiftTo8Bit = parameters.bitsPerSample - 8;
  const auto inputMax    = (1 << parameters.bitsPerSample) - 1;

  for (int y = 0; y < parameters.frameSize.height; ++y)
  {
    for (int x = 0; x < parameters.frameSize.width; ++x)
    {
      auto yValue =
          getValueFromSource(inputPlane, x, parameters.bitsPerSample, parameters.bigEndian);
      yValue = applyMathToValue(yValue, math, inputMax);

      if (shiftTo8Bit > 0)
        yValue = clip8Bit(yValue >> shiftTo8Bit);
      if (!isFullRange(parameters.colorConversion))
        yValue = LimitedRangeToFullRange.at(yValue);

      // Set the value for R, G and B (BGRA)
      output[0] = (unsigned char)yValue;
      output[1] = (unsigned char)yValue;
      output[2] = (unsigned char)yValue;
      output[3] = (unsigned char)255;
      output += OFFSET_TO_NEXT_RGB_VALUE;
    }

    inputPlane += parameters.stridePerComponent.at(LUMA);
  }
}

void uvPlaneToRGBMonochrome444(ConstDataPointer            inputPlane,
                               const ConversionParameters &parameters,
                               DataPointer                 output)
{
  // For 4:4:4, all planes behave the same
  yPlaneToRGBMonochrome(inputPlane, parameters, output);
}

void uvPlaneToRGBMonochrome422(ConstDataPointer            inputPlane,
                               const ConversionParameters &parameters,
                               DataPointer                 output)
{
  const auto math = parameters.mathParameters.at(CHROMA);

  const auto shiftTo8Bit = parameters.bitsPerSample - 8;
  const auto inputMax    = (1 << parameters.bitsPerSample) - 1;

  for (int y = 0; y < parameters.frameSize.height; ++y)
  {
    for (int x = 0; x < parameters.frameSize.width / 2; ++x)
    {
      auto value =
          getValueFromSource(inputPlane, x, parameters.bitsPerSample, parameters.bigEndian);
      value = applyMathToValue(value, math, inputMax);

      if (shiftTo8Bit > 0)
        value = clip8Bit(value >> shiftTo8Bit);
      if (!isFullRange(parameters.colorConversion))
        value = LimitedRangeToFullRange.at(value);

      // Set the value for R, G and B (BGRA)
      output[0] = static_cast<unsigned char>(value);
      output[1] = static_cast<unsigned char>(value);
      output[2] = static_cast<unsigned char>(value);
      output[3] = 255;
      output += OFFSET_TO_NEXT_RGB_VALUE;
      output[0] = static_cast<unsigned char>(value);
      output[1] = static_cast<unsigned char>(value);
      output[2] = static_cast<unsigned char>(value);
      output[3] = 255;
      output += OFFSET_TO_NEXT_RGB_VALUE;
    }

    inputPlane += parameters.stridePerComponent.at(CHROMA);
  }
}

void uvPlaneToRGBMonochrome420(ConstDataPointer            inputPlane,
                               const ConversionParameters &parameters,
                               DataPointer                 output)
{
  const auto math = parameters.mathParameters.at(CHROMA);

  const auto shiftTo8Bit         = parameters.bitsPerSample - 8;
  const auto inputMax            = (1 << parameters.bitsPerSample) - 1;
  const auto offsetToNextRGBLine = parameters.frameSize.width;

  for (int y = 0; y < parameters.frameSize.height / 2; ++y)
  {
    for (int x = 0; x < parameters.frameSize.width / 2; ++x)
    {
      auto value =
          getValueFromSource(inputPlane, x, parameters.bitsPerSample, parameters.bigEndian);
      value = applyMathToValue(value, math, inputMax);

      if (shiftTo8Bit > 0)
        value = clip8Bit(value >> shiftTo8Bit);
      if (!isFullRange(parameters.colorConversion))
        value = LimitedRangeToFullRange.at(value);

      output[0] = static_cast<unsigned char>(value);
      output[1] = static_cast<unsigned char>(value);
      output[2] = static_cast<unsigned char>(value);
      output[3] = 255;
      output += OFFSET_TO_NEXT_RGB_VALUE;
      output[0] = static_cast<unsigned char>(value);
      output[1] = static_cast<unsigned char>(value);
      output[2] = static_cast<unsigned char>(value);
      output[3] = 255;
      output += OFFSET_TO_NEXT_RGB_VALUE;

      output[0 + offsetToNextRGBLine] = static_cast<unsigned char>(value);
      output[1 + offsetToNextRGBLine] = static_cast<unsigned char>(value);
      output[2 + offsetToNextRGBLine] = static_cast<unsigned char>(value);
      output[3 + offsetToNextRGBLine] = 255;
      output += OFFSET_TO_NEXT_RGB_VALUE;
      output[0 + offsetToNextRGBLine] = static_cast<unsigned char>(value);
      output[1 + offsetToNextRGBLine] = static_cast<unsigned char>(value);
      output[2 + offsetToNextRGBLine] = static_cast<unsigned char>(value);
      output[3 + offsetToNextRGBLine] = 255;
      output += OFFSET_TO_NEXT_RGB_VALUE;
    }

    inputPlane += parameters.stridePerComponent.at(CHROMA);
    output += offsetToNextRGBLine;
  }
}

void uvPlaneToRGBMonochrome440(ConstDataPointer            inputPlane,
                               const ConversionParameters &parameters,
                               DataPointer                 output)
{
  const auto math = parameters.mathParameters.at(CHROMA);

  const auto shiftTo8Bit         = parameters.bitsPerSample - 8;
  const auto inputMax            = (1 << parameters.bitsPerSample) - 1;
  const auto offsetToNextRGBLine = parameters.frameSize.width;

  for (int y = 0; y < parameters.frameSize.height / 2; ++y)
  {
    for (int x = 0; x < parameters.frameSize.width; ++x)
    {
      auto value =
          getValueFromSource(inputPlane, x, parameters.bitsPerSample, parameters.bigEndian);
      value = applyMathToValue(value, math, inputMax);

      if (shiftTo8Bit > 0)
        value = clip8Bit(value >> shiftTo8Bit);
      if (!isFullRange(parameters.colorConversion))
        value = LimitedRangeToFullRange.at(value);

      // Set the value for R, G and B (BGRA)
      output[0]                       = static_cast<unsigned char>(value);
      output[1]                       = static_cast<unsigned char>(value);
      output[2]                       = static_cast<unsigned char>(value);
      output[3]                       = 255;
      output[0 + offsetToNextRGBLine] = static_cast<unsigned char>(value);
      output[1 + offsetToNextRGBLine] = static_cast<unsigned char>(value);
      output[2 + offsetToNextRGBLine] = static_cast<unsigned char>(value);
      output[3 + offsetToNextRGBLine] = 255;
      output += OFFSET_TO_NEXT_RGB_VALUE;
    }

    inputPlane += parameters.stridePerComponent.at(CHROMA);
    output += offsetToNextRGBLine;
  }
}

void uvPlaneToRGBMonochrome410(ConstDataPointer            inputPlane,
                               const ConversionParameters &parameters,
                               DataPointer                 output)
{
  const auto math = parameters.mathParameters.at(CHROMA);

  const auto shiftTo8Bit         = parameters.bitsPerSample - 8;
  const auto inputMax            = (1 << parameters.bitsPerSample) - 1;
  const auto offsetToNextRGBLine = parameters.frameSize.width;

  for (int y = 0; y < parameters.frameSize.height / 4; ++y)
  {
    for (int x = 0; x < parameters.frameSize.width / 4; ++x)
    {
      auto value =
          getValueFromSource(inputPlane, x, parameters.bitsPerSample, parameters.bigEndian);
      value = applyMathToValue(value, math, inputMax);

      if (shiftTo8Bit > 0)
        value = clip8Bit(value >> shiftTo8Bit);
      if (!isFullRange(parameters.colorConversion))
        value = LimitedRangeToFullRange.at(value);

      for (int yo = 0; yo < 4; yo++)
      {
        for (int xo = 0; xo < 4; xo++)
        {
          const auto pos  = yo + xo * offsetToNextRGBLine;
          output[0 + pos] = static_cast<unsigned char>(value);
          output[1 + pos] = static_cast<unsigned char>(value);
          output[2 + pos] = static_cast<unsigned char>(value);
          output[3 + pos] = 255;
        }
      }

      output += OFFSET_TO_NEXT_RGB_VALUE * 4;
    }

    inputPlane += parameters.stridePerComponent.at(CHROMA);
    output += offsetToNextRGBLine * 3;
  }
}

void uvPlaneToRGBMonochrome411(ConstDataPointer            inputPlane,
                               const ConversionParameters &parameters,
                               DataPointer                 output)
{
  const auto math = parameters.mathParameters.at(CHROMA);

  const auto shiftTo8Bit = parameters.bitsPerSample - 8;
  const auto inputMax    = (1 << parameters.bitsPerSample) - 1;

  for (int y = 0; y < parameters.frameSize.height; ++y)
  {
    for (int x = 0; x < parameters.frameSize.width / 4; ++x)
    {
      auto value =
          getValueFromSource(inputPlane, x, parameters.bitsPerSample, parameters.bigEndian);
      value = applyMathToValue(value, math, inputMax);

      if (shiftTo8Bit > 0)
        value = clip8Bit(value >> shiftTo8Bit);
      if (!isFullRange(parameters.colorConversion))
        value = LimitedRangeToFullRange.at(value);

      for (int xo = 0; xo < 4; xo++)
      {
        output[0] = static_cast<unsigned char>(value);
        output[1] = static_cast<unsigned char>(value);
        output[2] = static_cast<unsigned char>(value);
        output[3] = 255;
        output += OFFSET_TO_NEXT_RGB_VALUE;
      }
    }

    inputPlane += parameters.stridePerComponent.at(CHROMA);
  }
}

} // namespace video::yuv::conversion
