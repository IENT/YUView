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

#include "ConversionYUVPlanar.h"

#include <video/LimitedRangeToFullRange.h>
#include <video/rgb/PixelFormatRGB.h>
#include <video/yuv/conversion/ColorConversionCoefficients.h>

namespace video::yuv::conversion
{

using rgba_t = video::rgb::rgba_t;

namespace
{

inline int interpolateUVSample(const ChromaInterpolation mode, const int sample1, const int sample2)
{
  if (mode == ChromaInterpolation::Bilinear)
    // Interpolate linearly between sample1 and sample2
    return ((sample1 + sample2) + 1) >> 1;
  return sample1; // Sample and hold
}

inline int interpolateUVSampleQ(const ChromaInterpolation mode,
                                const int                 sample1,
                                const int                 sample2,
                                const int                 quarterPos)
{
  if (mode == ChromaInterpolation::Bilinear)
  {
    // Interpolate linearly between sample1 and sample2
    if (quarterPos == 0)
      return sample1;
    if (quarterPos == 1)
      return ((sample1 * 3 + sample2) + 1) >> 2;
    if (quarterPos == 2)
      return ((sample1 + sample2) + 1) >> 1;
    if (quarterPos == 3)
      return ((sample1 + sample2 * 3) + 1) >> 2;
  }
  return sample1; // Sample and hold
}

// TODO: Consider sample position
inline int interpolateUVSample2D(const ChromaInterpolation mode,
                                 const int                 sample1,
                                 const int                 sample2,
                                 const int                 sample3,
                                 const int                 sample4)
{
  if (mode == ChromaInterpolation::Bilinear)
    // Interpolate linearly between sample1 - sample 4
    return ((sample1 + sample2 + sample3 + sample4) + 2) >> 2;
  return sample1; // Sample and hold
}

yuva_t getYUVValuesFromSource(const DataPointerPerChannel &inputPlanes,
                              const int                    valueIndex,
                              const ConversionParameters & parameters)
{
  const auto valY = getValueFromSource(inputPlanes.at(LUMA),
                                       valueIndex * parameters.nextValueOffsets.at(LUMA),
                                       parameters.bitsPerSample,
                                       parameters.bigEndian);
  const auto valU = getValueFromSource(inputPlanes.at(CHROMA_U),
                                       valueIndex * parameters.nextValueOffsets.at(CHROMA),
                                       parameters.bitsPerSample,
                                       parameters.bigEndian);
  const auto valV = getValueFromSource(inputPlanes.at(CHROMA_U),
                                       valueIndex * parameters.nextValueOffsets.at(CHROMA),
                                       parameters.bitsPerSample,
                                       parameters.bigEndian);

  return {valY, valU, valV};
}

void saveRGBValueToOutput(const rgba_t &rgbValue, DataPointer output)
{
  // The output is always 8 bit ARGB little endian.
  output[0] = rgbValue.B;
  output[1] = rgbValue.G;
  output[2] = rgbValue.R;
  output[3] = 255;
}

inline rgba_t convertYUVToRGB8Bit(const yuva_t                       yuva,
                                  const ColorConversionCoefficients &coefficients,
                                  const bool                         fullRange,
                                  const int                          bitsPerSample)
{
  if (bitsPerSample > 14)
  {
    // The bit depth of an int (32) is not enough to perform a YUV -> RGB conversion for a bit depth
    // > 14 bits. We could use 64 bit values but for what? We are clipping the result to 8 bit
    // anyways so let's just get rid of 2 of the bits for the YUV values.
    const auto yOffset = (fullRange ? 0 : 16 << (bitsPerSample - 10));
    const auto cZero   = 128 << (bitsPerSample - 10);

    const auto Y_tmp = ((yuva.Y >> 2) - yOffset) * coefficients[0];
    const auto U_tmp = (yuva.U >> 2) - cZero;
    const auto V_tmp = (yuva.V >> 2) - cZero;

    const auto rShift = (16 + bitsPerSample - 10);

    const auto R_tmp =
        (Y_tmp + V_tmp * coefficients[1]) >> rShift; // 32 to 16 bit conversion by right shifting
    const auto G_tmp = (Y_tmp + U_tmp * coefficients[2] + V_tmp * coefficients[3]) >> rShift;
    const auto B_tmp = (Y_tmp + U_tmp * coefficients[4]) >> rShift;

    const auto valR = (R_tmp < 0) ? 0 : (R_tmp > 255) ? 255 : R_tmp;
    const auto valG = (G_tmp < 0) ? 0 : (G_tmp > 255) ? 255 : G_tmp;
    const auto valB = (B_tmp < 0) ? 0 : (B_tmp > 255) ? 255 : B_tmp;

    return {static_cast<unsigned>(valR), static_cast<unsigned>(valG), static_cast<unsigned>(valB)};
  }
  else
  {
    const auto yOffset = (fullRange ? 0 : 16 << (bitsPerSample - 8));
    const auto cZero   = 128 << (bitsPerSample - 8);

    const auto Y_tmp = (yuva.Y - yOffset) * coefficients[0];
    const auto U_tmp = yuva.U - cZero;
    const auto V_tmp = yuva.V - cZero;

    const auto rShift = (16 + bitsPerSample - 8);

    const auto R_tmp =
        (Y_tmp + V_tmp * coefficients[1]) >> rShift; // 32 to 16 bit conversion by right shifting
    const auto G_tmp = (Y_tmp + U_tmp * coefficients[2] + V_tmp * coefficients[3]) >> rShift;
    const auto B_tmp = (Y_tmp + U_tmp * coefficients[4]) >> rShift;

    const auto valR = (R_tmp < 0) ? 0 : (R_tmp > 255) ? 255 : R_tmp;
    const auto valG = (G_tmp < 0) ? 0 : (G_tmp > 255) ? 255 : G_tmp;
    const auto valB = (B_tmp < 0) ? 0 : (B_tmp > 255) ? 255 : B_tmp;

    return {static_cast<unsigned>(valR), static_cast<unsigned>(valG), static_cast<unsigned>(valB)};
  }
}

} // namespace

void yuvToRGB444(DataPointerPerChannel       inputPlanes,
                 const ConversionParameters &parameters,
                 DataPointer                 output)
{
  const auto inputMax = (1 << parameters.bitsPerSample) - 1;

  for (unsigned int y = 0; y < parameters.frameSize.height; ++y)
  {
    for (unsigned int x = 0; x < parameters.frameSize.width; ++x)
    {
      auto yuvValue = getYUVValuesFromSource(inputPlanes, x, parameters);
      yuvValue      = applyMathToValue(yuvValue, parameters.mathParameters, inputMax);

      const auto rgbValue =
          convertYUVToRGB8Bit(yuvValue,
                              getColorConversionCoefficients(parameters.colorConversion),
                              isFullRange(parameters.colorConversion),
                              parameters.bitsPerSample);

      saveRGBValueToOutput(rgbValue, output);
      output += OFFSET_TO_NEXT_RGB_VALUE;
    }

    inputPlanes[LUMA] += parameters.stridePerComponent[LUMA];
    inputPlanes[CHROMA_U] += parameters.stridePerComponent[CHROMA];
    inputPlanes[CHROMA_V] += parameters.stridePerComponent[CHROMA];
  }
}

void yuvToRGB422(DataPointerPerChannel       inputPlanes,
                 const ConversionParameters &parameters,
                 DataPointer                 output)
{
  const auto inputMax   = (1 << parameters.bitsPerSample) - 1;
  const auto bps        = parameters.bitsPerSample;
  const auto bigEndian  = parameters.bigEndian;
  const auto mathLuma   = parameters.mathParameters.at(LUMA);
  const auto mathChroma = parameters.mathParameters.at(CHROMA);

  for (unsigned y = 0; y < parameters.frameSize.height; ++y)
  {
    const auto srcY = inputPlanes.at(LUMA);
    const auto srcU = inputPlanes.at(CHROMA_U);
    const auto srcV = inputPlanes.at(CHROMA_V);

    auto uValue = getValueFromSource(srcU, 0, bps, bigEndian);
    auto vValue = getValueFromSource(srcV, 0, bps, bigEndian);
    uValue      = applyMathToValue(uValue, mathChroma, inputMax);
    vValue      = applyMathToValue(vValue, mathChroma, inputMax);

    for (unsigned x = 0; x < (parameters.frameSize.width / 2) - 1; ++x)
    {
      auto uValueNext = getValueFromSource(srcU, x + 1, bps, bigEndian);
      auto vValueNext = getValueFromSource(srcV, x + 1, bps, bigEndian);
      uValueNext      = applyMathToValue(uValueNext, mathChroma, inputMax);
      vValueNext      = applyMathToValue(vValueNext, mathChroma, inputMax);

      auto interpolatedU = interpolateUVSample(parameters.chromaInterpolation, uValue, uValueNext);
      auto interpolatedV = interpolateUVSample(parameters.chromaInterpolation, vValue, vValueNext);

      auto valY1 = getValueFromSource(srcY, x * 2, bps, bigEndian);
      auto valY2 = getValueFromSource(srcY, x * 2 + 1, bps, bigEndian);
      valY1      = applyMathToValue(valY1, mathLuma, inputMax);
      valY2      = applyMathToValue(valY2, mathLuma, inputMax);

      const auto rgb1 =
          convertYUVToRGB8Bit({valY1, uValue, vValue},
                              getColorConversionCoefficients(parameters.colorConversion),
                              isFullRange(parameters.colorConversion),
                              parameters.bitsPerSample);
      const auto rgb2 =
          convertYUVToRGB8Bit({valY2, interpolatedU, interpolatedV},
                              getColorConversionCoefficients(parameters.colorConversion),
                              isFullRange(parameters.colorConversion),
                              parameters.bitsPerSample);

      saveRGBValueToOutput(rgb1, output);
      output += OFFSET_TO_NEXT_RGB_VALUE;
      saveRGBValueToOutput(rgb2, output);
      output += OFFSET_TO_NEXT_RGB_VALUE;

      // The next one is now the current one
      uValue = uValueNext;
      vValue = vValueNext;
    }

    // For the last row, there is no next sample. Just reuse the current one again. No
    // interpolation required either.

    const auto xLastInRow = (parameters.frameSize.width / 2) - 1;
    auto       valY1      = getValueFromSource(srcY, xLastInRow * 2, bps, bigEndian);
    auto       valY2      = getValueFromSource(srcY, xLastInRow * 2 + 1, bps, bigEndian);
    valY1                 = applyMathToValue(valY1, mathLuma, inputMax);
    valY2                 = applyMathToValue(valY2, mathLuma, inputMax);

    const auto rgb1 =
        convertYUVToRGB8Bit({valY1, uValue, vValue},
                            getColorConversionCoefficients(parameters.colorConversion),
                            isFullRange(parameters.colorConversion),
                            parameters.bitsPerSample);
    const auto rgb2 =
        convertYUVToRGB8Bit({valY2, uValue, vValue},
                            getColorConversionCoefficients(parameters.colorConversion),
                            isFullRange(parameters.colorConversion),
                            parameters.bitsPerSample);

    saveRGBValueToOutput(rgb1, output);
    output += OFFSET_TO_NEXT_RGB_VALUE;
    saveRGBValueToOutput(rgb2, output);
    output += OFFSET_TO_NEXT_RGB_VALUE;

    inputPlanes[LUMA] += parameters.stridePerComponent[LUMA];
    inputPlanes[CHROMA_U] += parameters.stridePerComponent[CHROMA];
    inputPlanes[CHROMA_V] += parameters.stridePerComponent[CHROMA];
  }
}

void yuvToRGB420(const DataPointerPerChannel &inputPlanes,
                 const ConversionParameters & parameters,
                 DataPointer                  output)
{
  // TODO: Use some lambdas in here to make this shorter and more readable.
  //       We are doing the same stuff over and over again just in blocks of 4x4.

  const auto inputMax      = (1 << parameters.bitsPerSample) - 1;
  const auto bps           = parameters.bitsPerSample;
  const auto bigEndian     = parameters.bigEndian;
  const auto mathLuma      = parameters.mathParameters.at(LUMA);
  const auto mathChroma    = parameters.mathParameters.at(CHROMA);
  const auto interpolation = parameters.chromaInterpolation;

  const auto outputStride = parameters.frameSize.width * OFFSET_TO_NEXT_RGB_VALUE;
  auto       outputLine1  = output;
  auto       outputLine2  = outputLine1 + outputStride;

  auto srcY = inputPlanes.at(LUMA);
  auto srcU = inputPlanes.at(CHROMA_U);
  auto srcV = inputPlanes.at(CHROMA_V);

  auto srcY_nl = srcY + parameters.stridePerComponent[LUMA];
  auto srcU_nl = srcU + parameters.stridePerComponent[CHROMA];
  auto srcV_nl = srcV + parameters.stridePerComponent[CHROMA];

  // Format is YUV 4:2:0. Horizontal and vertical up-sampling is
  // required. Process 4 Y positions at a time
  const auto hh = parameters.frameSize.height / 2;
  const auto wh = parameters.frameSize.width / 2;
  for (unsigned y = 0; y < hh - 1; y++)
  {
    // Get the current U/V samples for this y line and the one from the next line (_NL)

    auto uValue = getValueFromSource(srcU, 0, bps, bigEndian);
    auto vValue = getValueFromSource(srcV, 0, bps, bigEndian);
    uValue      = applyMathToValue(uValue, mathChroma, inputMax);
    vValue      = applyMathToValue(vValue, mathChroma, inputMax);

    auto uValue_nl = getValueFromSource(srcU_nl, 0, bps, bigEndian);
    auto vValue_nl = getValueFromSource(srcV_nl, 0, bps, bigEndian);
    uValue_nl      = applyMathToValue(uValue_nl, mathChroma, inputMax);
    vValue_nl      = applyMathToValue(vValue_nl, mathChroma, inputMax);

    for (unsigned x = 0; x < wh - 1; x++)
    {
      // Get the next U/V sample for this line and the next one
      auto uValueNext = getValueFromSource(srcU, x + 1, bps, bigEndian);
      auto vValueNext = getValueFromSource(srcV, x + 1, bps, bigEndian);
      uValueNext      = applyMathToValue(uValueNext, mathChroma, inputMax);
      vValueNext      = applyMathToValue(vValueNext, mathChroma, inputMax);

      auto uValueNext_nl = getValueFromSource(srcU_nl, 0, bps, bigEndian);
      auto vValueNext_nl = getValueFromSource(srcV_nl, 0, bps, bigEndian);
      uValueNext_nl      = applyMathToValue(uValueNext_nl, mathChroma, inputMax);
      vValueNext_nl      = applyMathToValue(vValueNext_nl, mathChroma, inputMax);

      // From the current and the next U/V sample, interpolate the 3 UV samples in between
      int interpolatedU_Hor = interpolateUVSample(interpolation, uValue, uValueNext);
      int interpolatedV_Hor = interpolateUVSample(interpolation, vValue, vValueNext);
      int interpolatedU_Ver = interpolateUVSample(interpolation, uValue, uValue_nl);
      int interpolatedV_Ver = interpolateUVSample(interpolation, vValue, vValue_nl);
      int interpolatedU_Bi =
          interpolateUVSample2D(interpolation, uValue, uValueNext, uValue_nl, uValueNext_nl);
      int interpolatedV_Bi =
          interpolateUVSample2D(interpolation, vValue, vValueNext, vValue_nl, vValueNext_nl);

      // Get the 4 Y samples
      auto valY1 = getValueFromSource(srcY, x * 2, bps, bigEndian);
      auto valY2 = getValueFromSource(srcY, x * 2 + 1, bps, bigEndian);
      auto valY3 = getValueFromSource(srcY_nl, x * 2, bps, bigEndian);
      auto valY4 = getValueFromSource(srcY_nl, x * 2 + 1, bps, bigEndian);

      valY1 = applyMathToValue(valY1, mathLuma, inputMax);
      valY2 = applyMathToValue(valY2, mathLuma, inputMax);
      valY3 = applyMathToValue(valY3, mathLuma, inputMax);
      valY4 = applyMathToValue(valY4, mathLuma, inputMax);

      // Convert to 4 RGB values and save them
      const auto rgb1 =
          convertYUVToRGB8Bit({valY1, uValue, vValue},
                              getColorConversionCoefficients(parameters.colorConversion),
                              isFullRange(parameters.colorConversion),
                              parameters.bitsPerSample);
      const auto rgb2 =
          convertYUVToRGB8Bit({valY2, interpolatedU_Hor, interpolatedV_Hor},
                              getColorConversionCoefficients(parameters.colorConversion),
                              isFullRange(parameters.colorConversion),
                              parameters.bitsPerSample);
      const auto rgb3 =
          convertYUVToRGB8Bit({valY3, interpolatedU_Ver, interpolatedV_Ver},
                              getColorConversionCoefficients(parameters.colorConversion),
                              isFullRange(parameters.colorConversion),
                              parameters.bitsPerSample);
      const auto rgb4 =
          convertYUVToRGB8Bit({valY4, interpolatedU_Bi, interpolatedV_Bi},
                              getColorConversionCoefficients(parameters.colorConversion),
                              isFullRange(parameters.colorConversion),
                              parameters.bitsPerSample);

      saveRGBValueToOutput(rgb1, outputLine1);
      outputLine1 += OFFSET_TO_NEXT_RGB_VALUE;
      saveRGBValueToOutput(rgb2, outputLine1);
      outputLine1 += OFFSET_TO_NEXT_RGB_VALUE;

      saveRGBValueToOutput(rgb3, outputLine2);
      outputLine2 += OFFSET_TO_NEXT_RGB_VALUE;
      saveRGBValueToOutput(rgb4, outputLine2);
      outputLine2 += OFFSET_TO_NEXT_RGB_VALUE;

      // The next one is now the current one
      uValue    = uValueNext;
      vValue    = vValueNext;
      uValue_nl = uValueNext_nl;
      vValue_nl = vValueNext_nl;
    }

    // For the last x value (the right border), there is no next value. Just sample and hold. Only
    // vertical interpolation is required.
    int interpolatedU_Ver = interpolateUVSample(interpolation, uValue, uValue_nl);
    int interpolatedV_Ver = interpolateUVSample(interpolation, vValue, vValue_nl);

    // Get the 4 Y samples
    const auto x     = parameters.frameSize.width - 2;
    auto       valY1 = getValueFromSource(srcY, x * 2, bps, bigEndian);
    auto       valY2 = getValueFromSource(srcY, x * 2 + 1, bps, bigEndian);
    auto       valY3 = getValueFromSource(srcY_nl, x * 2, bps, bigEndian);
    auto       valY4 = getValueFromSource(srcY_nl, x * 2 + 1, bps, bigEndian);

    valY1 = applyMathToValue(valY1, mathLuma, inputMax);
    valY2 = applyMathToValue(valY2, mathLuma, inputMax);
    valY3 = applyMathToValue(valY3, mathLuma, inputMax);
    valY4 = applyMathToValue(valY4, mathLuma, inputMax);

    // Convert to 4 RGB values and save them
    const auto rgb1 =
        convertYUVToRGB8Bit({valY1, uValue, vValue},
                            getColorConversionCoefficients(parameters.colorConversion),
                            isFullRange(parameters.colorConversion),
                            parameters.bitsPerSample);
    const auto rgb2 =
        convertYUVToRGB8Bit({valY2, uValue, vValue},
                            getColorConversionCoefficients(parameters.colorConversion),
                            isFullRange(parameters.colorConversion),
                            parameters.bitsPerSample);
    const auto rgb3 =
        convertYUVToRGB8Bit({valY3, interpolatedU_Ver, interpolatedV_Ver},
                            getColorConversionCoefficients(parameters.colorConversion),
                            isFullRange(parameters.colorConversion),
                            parameters.bitsPerSample);
    const auto rgb4 =
        convertYUVToRGB8Bit({valY4, interpolatedU_Ver, interpolatedV_Ver},
                            getColorConversionCoefficients(parameters.colorConversion),
                            isFullRange(parameters.colorConversion),
                            parameters.bitsPerSample);

    saveRGBValueToOutput(rgb1, outputLine1);
    outputLine1 += OFFSET_TO_NEXT_RGB_VALUE;
    saveRGBValueToOutput(rgb2, outputLine1);
    outputLine1 += OFFSET_TO_NEXT_RGB_VALUE;

    saveRGBValueToOutput(rgb3, outputLine2);
    outputLine2 += OFFSET_TO_NEXT_RGB_VALUE;
    saveRGBValueToOutput(rgb4, outputLine2);
    outputLine2 += OFFSET_TO_NEXT_RGB_VALUE;

    srcY += parameters.stridePerComponent[LUMA] * 2;
    srcU += parameters.stridePerComponent[CHROMA] * 2;
    srcV += parameters.stridePerComponent[CHROMA] * 2;
    srcY_nl += parameters.stridePerComponent[LUMA] * 2;
    srcU_nl += parameters.stridePerComponent[CHROMA] * 2;
    srcV_nl += parameters.stridePerComponent[CHROMA] * 2;
  }

  // At the last Y line (the bottom line) a similar scenario occurs. There is no next Y line. Just
  // sample and hold. Only horizontal interpolation is required.

  // Get 2 chroma samples from this line
  auto uValue = getValueFromSource(srcU, 0, bps, bigEndian);
  auto vValue = getValueFromSource(srcV, 0, bps, bigEndian);
  uValue      = applyMathToValue(uValue, mathChroma, inputMax);
  vValue      = applyMathToValue(vValue, mathChroma, inputMax);

  for (unsigned x = 0; x < wh - 1; x++)
  {
    // Get the next U/V sample for this line and the next one
    auto uValueNext = getValueFromSource(srcU, x + 1, bps, bigEndian);
    auto vValueNext = getValueFromSource(srcV, x + 1, bps, bigEndian);
    uValueNext      = applyMathToValue(uValueNext, mathChroma, inputMax);
    vValueNext      = applyMathToValue(vValueNext, mathChroma, inputMax);

    // From the current and the next U/V sample, interpolate the 3 UV samples in between
    int interpolatedU_Hor = interpolateUVSample(interpolation, uValue, uValueNext);
    int interpolatedV_Hor = interpolateUVSample(interpolation, vValue, vValueNext);

    // Get the 4 Y samples
    auto valY1 = getValueFromSource(srcY, x * 2, bps, bigEndian);
    auto valY2 = getValueFromSource(srcY, x * 2 + 1, bps, bigEndian);
    auto valY3 = getValueFromSource(srcY_nl, x * 2, bps, bigEndian);
    auto valY4 = getValueFromSource(srcY_nl, x * 2 + 1, bps, bigEndian);

    valY1 = applyMathToValue(valY1, mathLuma, inputMax);
    valY2 = applyMathToValue(valY2, mathLuma, inputMax);
    valY3 = applyMathToValue(valY3, mathLuma, inputMax);
    valY4 = applyMathToValue(valY4, mathLuma, inputMax);

    // Convert to 4 RGB values and save them
    const auto rgb1 =
        convertYUVToRGB8Bit({valY1, uValue, vValue},
                            getColorConversionCoefficients(parameters.colorConversion),
                            isFullRange(parameters.colorConversion),
                            parameters.bitsPerSample);
    const auto rgb2 =
        convertYUVToRGB8Bit({valY2, interpolatedU_Hor, interpolatedV_Hor},
                            getColorConversionCoefficients(parameters.colorConversion),
                            isFullRange(parameters.colorConversion),
                            parameters.bitsPerSample);
    const auto rgb3 =
        convertYUVToRGB8Bit({valY3, uValue, vValue},
                            getColorConversionCoefficients(parameters.colorConversion),
                            isFullRange(parameters.colorConversion),
                            parameters.bitsPerSample);
    const auto rgb4 =
        convertYUVToRGB8Bit({valY4, interpolatedU_Hor, interpolatedV_Hor},
                            getColorConversionCoefficients(parameters.colorConversion),
                            isFullRange(parameters.colorConversion),
                            parameters.bitsPerSample);

    saveRGBValueToOutput(rgb1, outputLine1);
    outputLine1 += OFFSET_TO_NEXT_RGB_VALUE;
    saveRGBValueToOutput(rgb2, outputLine1);
    outputLine1 += OFFSET_TO_NEXT_RGB_VALUE;

    saveRGBValueToOutput(rgb3, outputLine2);
    outputLine2 += OFFSET_TO_NEXT_RGB_VALUE;
    saveRGBValueToOutput(rgb4, outputLine2);
    outputLine2 += OFFSET_TO_NEXT_RGB_VALUE;

    // The next one is now the current one
    uValue = uValueNext;
    vValue = vValueNext;
  }

  // For the last x value in the last y row (the right bottom), there is no next value in neither
  // direction. Just sample and hold. No interpolation is required.

  // Get the 4 Y samples
  const auto x     = parameters.frameSize.width - 2;
  auto       valY1 = getValueFromSource(srcY, x * 2, bps, bigEndian);
  auto       valY2 = getValueFromSource(srcY, x * 2 + 1, bps, bigEndian);
  auto       valY3 = getValueFromSource(srcY_nl, x * 2, bps, bigEndian);
  auto       valY4 = getValueFromSource(srcY_nl, x * 2 + 1, bps, bigEndian);

  valY1 = applyMathToValue(valY1, mathLuma, inputMax);
  valY2 = applyMathToValue(valY2, mathLuma, inputMax);
  valY3 = applyMathToValue(valY3, mathLuma, inputMax);
  valY4 = applyMathToValue(valY4, mathLuma, inputMax);

  // Convert to 4 RGB values and save them
  const auto rgb1 = convertYUVToRGB8Bit({valY1, uValue, vValue},
                                        getColorConversionCoefficients(parameters.colorConversion),
                                        isFullRange(parameters.colorConversion),
                                        parameters.bitsPerSample);
  const auto rgb2 = convertYUVToRGB8Bit({valY2, uValue, vValue},
                                        getColorConversionCoefficients(parameters.colorConversion),
                                        isFullRange(parameters.colorConversion),
                                        parameters.bitsPerSample);
  const auto rgb3 = convertYUVToRGB8Bit({valY3, uValue, vValue},
                                        getColorConversionCoefficients(parameters.colorConversion),
                                        isFullRange(parameters.colorConversion),
                                        parameters.bitsPerSample);
  const auto rgb4 = convertYUVToRGB8Bit({valY4, uValue, vValue},
                                        getColorConversionCoefficients(parameters.colorConversion),
                                        isFullRange(parameters.colorConversion),
                                        parameters.bitsPerSample);

  saveRGBValueToOutput(rgb1, outputLine1);
  outputLine1 += OFFSET_TO_NEXT_RGB_VALUE;
  saveRGBValueToOutput(rgb2, outputLine1);

  saveRGBValueToOutput(rgb3, outputLine2);
  outputLine2 += OFFSET_TO_NEXT_RGB_VALUE;
  saveRGBValueToOutput(rgb4, outputLine2);
}

void yuvToRGB440(const DataPointerPerChannel &inputPlanes,
                 const ConversionParameters & parameters,
                 DataPointer                  output)
{
  const auto inputMax      = (1 << parameters.bitsPerSample) - 1;
  const auto bps           = parameters.bitsPerSample;
  const auto bigEndian     = parameters.bigEndian;
  const auto mathLuma      = parameters.mathParameters.at(LUMA);
  const auto mathChroma    = parameters.mathParameters.at(CHROMA);
  const auto interpolation = parameters.chromaInterpolation;

  const auto outputStride = parameters.frameSize.width * OFFSET_TO_NEXT_RGB_VALUE;

  // Vertical interpolation is required. Process two Y values at a time
  const auto hh = parameters.frameSize.height / 2;
  for (unsigned x = 0; x < parameters.frameSize.width; ++x)
  {
    auto srcY = inputPlanes.at(LUMA);
    auto srcU = inputPlanes.at(CHROMA_U);
    auto srcV = inputPlanes.at(CHROMA_V);

    auto outputColumn = output + x * OFFSET_TO_NEXT_RGB_VALUE;

    auto uValue = getValueFromSource(srcU, x, bps, bigEndian);
    auto vValue = getValueFromSource(srcV, x, bps, bigEndian);
    uValue      = applyMathToValue(uValue, mathChroma, inputMax);
    vValue      = applyMathToValue(vValue, mathChroma, inputMax);

    for (unsigned y = 0; y < hh - 1; y++)
    {
      // Get the next U/V sample
      srcU += parameters.stridePerComponent[CHROMA];
      srcV += parameters.stridePerComponent[CHROMA];

      auto uValueNext = getValueFromSource(srcU, x, bps, bigEndian);
      auto vValueNext = getValueFromSource(srcV, x, bps, bigEndian);
      uValueNext      = applyMathToValue(uValueNext, mathChroma, inputMax);
      vValueNext      = applyMathToValue(vValueNext, mathChroma, inputMax);

      // From the current and the next U/V sample, interpolate the UV sample in between
      auto interpolatedU = interpolateUVSample(interpolation, uValue, uValueNext);
      auto interpolatedV = interpolateUVSample(interpolation, vValue, vValueNext);

      // Get the 2 Y samples
      auto valY1 = getValueFromSource(srcY, x, bps, bigEndian);
      srcY += parameters.stridePerComponent[LUMA];
      auto valY2 = getValueFromSource(srcY, x, bps, bigEndian);
      srcY += parameters.stridePerComponent[LUMA];

      valY1 = applyMathToValue(valY1, mathLuma, inputMax);
      valY2 = applyMathToValue(valY2, mathLuma, inputMax);

      const auto rgb1 =
          convertYUVToRGB8Bit({valY1, uValue, vValue},
                              getColorConversionCoefficients(parameters.colorConversion),
                              isFullRange(parameters.colorConversion),
                              parameters.bitsPerSample);
      const auto rgb2 =
          convertYUVToRGB8Bit({valY2, interpolatedU, interpolatedV},
                              getColorConversionCoefficients(parameters.colorConversion),
                              isFullRange(parameters.colorConversion),
                              parameters.bitsPerSample);

      saveRGBValueToOutput(rgb1, outputColumn);
      outputColumn += outputStride;
      saveRGBValueToOutput(rgb2, outputColumn);
      outputColumn += outputStride;

      // The next one is now the current one
      uValue = uValueNext;
      vValue = vValueNext;
    }

    // For the last column, there is no next sample. Just reuse the current one again. No
    // interpolation required either.

    // Get the 2 Y samples
    auto valY1 = getValueFromSource(srcY, x, bps, bigEndian);
    srcY += parameters.stridePerComponent[LUMA];
    auto valY2 = getValueFromSource(srcY, x, bps, bigEndian);
    srcY += parameters.stridePerComponent[LUMA];

    valY1 = applyMathToValue(valY1, mathLuma, inputMax);
    valY2 = applyMathToValue(valY2, mathLuma, inputMax);

    const auto rgb1 =
        convertYUVToRGB8Bit({valY1, uValue, vValue},
                            getColorConversionCoefficients(parameters.colorConversion),
                            isFullRange(parameters.colorConversion),
                            parameters.bitsPerSample);
    const auto rgb2 =
        convertYUVToRGB8Bit({valY2, uValue, vValue},
                            getColorConversionCoefficients(parameters.colorConversion),
                            isFullRange(parameters.colorConversion),
                            parameters.bitsPerSample);

    saveRGBValueToOutput(rgb1, outputColumn);
    outputColumn += outputStride;
    saveRGBValueToOutput(rgb2, outputColumn);
  }
}

void yuvToRGB410(const DataPointerPerChannel &inputPlanes,
                 const ConversionParameters & parameters,
                 DataPointer                  output)
{
  const auto inputMax      = (1 << parameters.bitsPerSample) - 1;
  const auto bps           = parameters.bitsPerSample;
  const auto bigEndian     = parameters.bigEndian;
  const auto mathLuma      = parameters.mathParameters.at(LUMA);
  const auto mathChroma    = parameters.mathParameters.at(CHROMA);
  const auto interpolation = parameters.chromaInterpolation;

  const auto outputStride = parameters.frameSize.width * OFFSET_TO_NEXT_RGB_VALUE;

  // Format is YUV 4:1:0. Horizontal and vertical up-sampling is required. Process 4 Y positions of
  // 2 lines at a time. Horizontal subsampling by 4, vertical subsampling by 2
  const int hh = parameters.frameSize.height / 2;
  const int wq = parameters.frameSize.width / 4;

  auto srcY    = inputPlanes.at(LUMA);
  auto srcY_nl = srcY + parameters.stridePerComponent[LUMA];
  auto srcU    = inputPlanes.at(CHROMA_U);
  auto srcV    = inputPlanes.at(CHROMA_V);

  // We fill 2 lines at once
  auto output1 = output;
  auto output2 = output + outputStride;

  for (int y = 0; y < hh; y++)
  {
    // Get the current U/V samples for this y line and the next line (_nl)
    auto srcU_nl = srcU + parameters.stridePerComponent[CHROMA];
    auto srcV_nl = srcV + parameters.stridePerComponent[CHROMA];

    const auto x      = 0;
    auto       uValue = getValueFromSource(srcU, x, bps, bigEndian);
    auto       vValue = getValueFromSource(srcV, x, bps, bigEndian);
    uValue            = applyMathToValue(uValue, mathChroma, inputMax);
    vValue            = applyMathToValue(vValue, mathChroma, inputMax);

    auto uValue_nl = getValueFromSource(srcU_nl, x, bps, bigEndian);
    auto vValue_nl = getValueFromSource(srcV_nl, x, bps, bigEndian);
    uValue_nl      = applyMathToValue(uValue_nl, mathChroma, inputMax);
    vValue_nl      = applyMathToValue(vValue_nl, mathChroma, inputMax);

    for (int x = 0; x < wq; ++x)
    {
      // Get the next U/V sample for this line and the next one
      auto uValueNext = getValueFromSource(srcU, x + 1, bps, bigEndian);
      auto vValueNext = getValueFromSource(srcV, x + 1, bps, bigEndian);
      uValueNext      = applyMathToValue(uValueNext, mathChroma, inputMax);
      vValueNext      = applyMathToValue(vValueNext, mathChroma, inputMax);

      auto uValueNext_nl = getValueFromSource(srcU_nl, x + 1, bps, bigEndian);
      auto vValueNext_nl = getValueFromSource(srcV_nl, x + 1, bps, bigEndian);
      uValueNext_nl      = applyMathToValue(uValueNext_nl, mathChroma, inputMax);
      vValueNext_nl      = applyMathToValue(vValueNext_nl, mathChroma, inputMax);

      auto interpolatedU = interpolateUVSample(interpolation, uValue, uValue_nl);
      auto interpolatedV = interpolateUVSample(interpolation, vValue, vValue_nl);

      auto interpolatedUNext = interpolateUVSample(interpolation, uValueNext, uValueNext_nl);
      auto interpolatedVNext = interpolateUVSample(interpolation, vValueNext, vValueNext_nl);

      for (int subX = 0; subX < 4; ++subX)
      {
        // We process two Y lines
        const auto yIndex = x * 4 + subX;
        const auto valY1  = getValueFromSource(srcY, yIndex, bps, bigEndian);
        const auto valY2  = getValueFromSource(srcY_nl, yIndex, bps, bigEndian);

        auto u1 = interpolateUVSampleQ(interpolation, uValue, uValueNext, subX);
        auto v1 = interpolateUVSampleQ(interpolation, uValue_nl, uValueNext_nl, subX);
        auto u2 = interpolateUVSampleQ(interpolation, uValue, uValueNext, subX);
        auto v2 = interpolateUVSampleQ(interpolation, uValue_nl, uValueNext_nl, subX);

        u1 = applyMathToValue(u1, mathChroma, inputMax);
        v1 = applyMathToValue(v1, mathChroma, inputMax);
        u2 = applyMathToValue(u2, mathChroma, inputMax);
        v2 = applyMathToValue(v2, mathChroma, inputMax);

        const auto rgb1 =
            convertYUVToRGB8Bit({valY1, u1, v1},
                                getColorConversionCoefficients(parameters.colorConversion),
                                isFullRange(parameters.colorConversion),
                                parameters.bitsPerSample);
        const auto rgb2 =
            convertYUVToRGB8Bit({valY2, u2, v2},
                                getColorConversionCoefficients(parameters.colorConversion),
                                isFullRange(parameters.colorConversion),
                                parameters.bitsPerSample);

        saveRGBValueToOutput(rgb1, output1);
        output1 += OFFSET_TO_NEXT_RGB_VALUE;
        saveRGBValueToOutput(rgb2, output2);
        output2 += OFFSET_TO_NEXT_RGB_VALUE;
      }

      uValue    = uValueNext;
      vValue    = vValueNext;
      uValue_nl = uValueNext_nl;
      vValue_nl = vValueNext_nl;
    }

    srcY += 2 * parameters.stridePerComponent[LUMA];
    srcU += 2 * parameters.stridePerComponent[CHROMA];
    srcV += 2 * parameters.stridePerComponent[CHROMA];
    output1 += outputStride;
    output2 += outputStride;
  }
}

void yuvToRGB411(const DataPointerPerChannel &inputPlanes,
                 const ConversionParameters & parameters,
                 DataPointer                  output)
{
  const auto inputMax      = (1 << parameters.bitsPerSample) - 1;
  const auto bps           = parameters.bitsPerSample;
  const auto bigEndian     = parameters.bigEndian;
  const auto mathLuma      = parameters.mathParameters.at(LUMA);
  const auto mathChroma    = parameters.mathParameters.at(CHROMA);
  const auto interpolation = parameters.chromaInterpolation;

  auto srcY = inputPlanes.at(LUMA);
  auto srcU = inputPlanes.at(CHROMA_U);
  auto srcV = inputPlanes.at(CHROMA_V);

  const auto wq = parameters.frameSize.width / 4;

  // Horizontal up-sampling is required. Process four Y values at a time.
  for (int y = 0; y < parameters.frameSize.height; ++y)
  {
    const auto x      = 0;
    auto       uValue = getValueFromSource(srcU, x, bps, bigEndian);
    auto       vValue = getValueFromSource(srcV, x, bps, bigEndian);
    uValue            = applyMathToValue(uValue, mathChroma, inputMax);
    vValue            = applyMathToValue(vValue, mathChroma, inputMax);

    for (int x = 0; x < wq - 1; ++x)
    {
      auto uValueNext = getValueFromSource(srcU, x + 1, bps, bigEndian);
      auto vValueNext = getValueFromSource(srcV, x + 1, bps, bigEndian);
      uValueNext      = applyMathToValue(uValueNext, mathChroma, inputMax);
      vValueNext      = applyMathToValue(vValueNext, mathChroma, inputMax);

      for (int subX = 0; subX < 4; ++subX)
      {
        const auto yIndex = x * 4 + subX;
        const auto valY   = getValueFromSource(srcY, yIndex, bps, bigEndian);

        const auto interpolatedU = interpolateUVSampleQ(interpolation, uValue, uValueNext, subX);
        const auto interpolatedV = interpolateUVSampleQ(interpolation, vValue, vValueNext, subX);

        const auto rgb =
            convertYUVToRGB8Bit({valY, interpolatedU, interpolatedV},
                                getColorConversionCoefficients(parameters.colorConversion),
                                isFullRange(parameters.colorConversion),
                                parameters.bitsPerSample);

        saveRGBValueToOutput(rgb, output);
        output += OFFSET_TO_NEXT_RGB_VALUE;
      }

      uValue = uValueNext;
      vValue = vValueNext;
    }

    // For the last row, there is no next sample. Just reuse the current one again.
    // No interpolation required either.
    for (int subX = 0; subX < 4; ++subX)
    {
      const auto yIndex = x * 4 + subX;
      const auto valY   = getValueFromSource(srcY, yIndex, bps, bigEndian);

      const auto rgb =
          convertYUVToRGB8Bit({valY, uValue, vValue},
                              getColorConversionCoefficients(parameters.colorConversion),
                              isFullRange(parameters.colorConversion),
                              parameters.bitsPerSample);

      saveRGBValueToOutput(rgb, output);
      output += OFFSET_TO_NEXT_RGB_VALUE;
    }

    srcY += parameters.stridePerComponent[LUMA];
    srcU += parameters.stridePerComponent[CHROMA];
    srcV += parameters.stridePerComponent[CHROMA];
  }
}

} // namespace video::yuv::conversion
