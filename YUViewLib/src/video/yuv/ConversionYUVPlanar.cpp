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
#include <video/yuv/ChromaResampling.h>
#include <video/yuv/Common.h>
#include <video/yuv/Restrict.h>

namespace video::yuv
{

using rgba_t = video::rgb::rgba_t;

namespace
{

constexpr auto LUMA                     = 0;
constexpr auto CHROMA                   = 1;
constexpr auto CHROMA_U                 = 1;
constexpr auto CHROMA_V                 = 2;
constexpr auto OFFSET_TO_NEXT_RGB_VALUE = 4;

using MathParametersPerComponent  = std::array<MathParameters, 2>;
using DataPointer                 = unsigned char *restrict;
using DataPointerPerChannel       = std::array<DataPointer, 4>;
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
  bool                        fullRange{};
  ColorConversion             colorConversion{};
};

std::array<int, 2> getNrSamplesPerPlane(const PixelFormatYUV &pixelFormat, const Size frameSize)
{
  auto nrSamplesLuma   = static_cast<int>(frameSize.area());
  auto nrSamplesChroma = static_cast<int>((frameSize.width / pixelFormat.getSubsamplingHor()) *
                                          (frameSize.height / pixelFormat.getSubsamplingVer()));
  return {nrSamplesLuma, nrSamplesChroma};
}

std::array<int, 2> getNrBytesPerPlane(const PixelFormatYUV &pixelFormat, const Size frameSize)
{
  const auto nrSamplesPerPlane = getNrSamplesPerPlane(pixelFormat, frameSize);
  if (pixelFormat.getBitsPerSample() > 8)
    return {nrSamplesPerPlane[LUMA] * 2, nrSamplesPerPlane[CHROMA] * 2};
  return nrSamplesPerPlane;
}

bool isResamplingOfChromaNeeded(const PixelFormatYUV &     srcPixelFormat,
                                const ChromaInterpolation &chromaInterpolation)
{
  const auto hasChroma = (srcPixelFormat.getSubsampling() != Subsampling::YUV_400);
  if (!hasChroma)
    return false;

  const auto useChromaInterpolation = (chromaInterpolation != ChromaInterpolation::NearestNeighbor);
  if (!useChromaInterpolation)
    return false;

  const auto isChromaPositionTopLeft =
      (srcPixelFormat.getChromaOffset().x == 0 && srcPixelFormat.getChromaOffset().y == 0);
  if (!isChromaPositionTopLeft)
    return false;

  return true;
}

using ColorConversionCoefficients = std::array<int, 5>;
ColorConversionCoefficients getColorConversionCoefficients(ColorConversion colorConversion)
{
  // The values are [Y, cRV, cGU, cGV, cBU]
  switch (colorConversion)
  {
  case ColorConversion::BT709_LimitedRange:
    return {76309, 117489, -13975, -34925, 138438};
  case ColorConversion::BT709_FullRange:
    return {65536, 103206, -12276, -30679, 121608};
  case ColorConversion::BT601_LimitedRange:
    return {76309, 104597, -25675, -53279, 132201};
  case ColorConversion::BT601_FullRange:
    return {65536, 91881, -22553, -46802, 116129};
  case ColorConversion::BT2020_LimitedRange:
    return {76309, 110013, -12276, -42626, 140363};
  case ColorConversion::BT2020_FullRange:
    return {65536, 96638, -10783, -37444, 123299};

  default:
    return {};
  }
}

int getOffsetBetweenChromaPlanesInSamples(const ChromaPacking chromaPacking,
                                          const int           chromaStrideInSamples,
                                          const int           nrSamplesPerChromaPlane)
{
  if (chromaPacking == ChromaPacking::PerValue)
    return 1;
  if (chromaPacking == ChromaPacking::PerLine)
    return (chromaStrideInSamples / 2);
  if (chromaPacking == ChromaPacking::Planar)
    return nrSamplesPerChromaPlane;
}

DataPointerPerChannel getPointersToComponents(DataPointer           arrayPointer,
                                              const PixelFormatYUV &pixelFormat,
                                              const Size            frameSize)
{
  const auto nrSamplesPerPlane     = getNrSamplesPerPlane(pixelFormat, frameSize);
  const auto chromaStrideInSamples = getChromaStrideInSamples(pixelFormat, frameSize);

  const auto bytesPerSample = pixelFormat.getBytesPerSample();

  const auto offsetToNextChromaPlane = getOffsetBetweenChromaPlanesInSamples(
      pixelFormat.getChromaPacking(), chromaStrideInSamples, nrSamplesPerPlane[CHROMA]);

  const auto ptrY = arrayPointer;
  const auto ptrU = ptrY + nrSamplesPerPlane[LUMA] * bytesPerSample;
  const auto ptrV = ptrU + offsetToNextChromaPlane * bytesPerSample;

  DataPointer ptrA{};
  if (pixelFormat.hasAlpha())
    ptrA = ptrU + offsetToNextChromaPlane * bytesPerSample;

  const auto planeOrder = pixelFormat.getPlaneOrder();
  if (planeOrder == PlaneOrder::YUV || planeOrder == PlaneOrder::YUVA)
    return {ptrY, ptrU, ptrV, ptrA};
  else
    return {ptrY, ptrV, ptrU, ptrA};
}

/* Apply the given transformation to the YUV sample. If invert is true, the sample is inverted at
 * the value defined by offset. If the scale is greater one, the values will be amplified relative
 * to the offset value. The input can be 8 to 16 bit. The output will be of the same bit depth. The
 * output is clamped to (0...clipMax).
 */
inline int applyMathToValue(const unsigned int value, const MathParameters &math, const int clipMax)
{
  if (!math.mathRequired())
    return;

  auto newValue = value;
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

// For every input sample in src, apply YUV transformation, (scale to 8 bit if required) and set the
// value as RGB (monochrome). inValSkip: skip this many values in the input for every value. For
// pure planar formats, this 1. If the UV components are interleaved, this is 2 or 3.
inline void yPlaneToRGBMonochrome_444(const DataPointerPerChannel &inputPlanes,
                                      const ConversionParameters & parameters,
                                      DataPointer                  output)
{
  const auto math           = parameters.mathParameters.at(LUMA);
  const auto lumaPlane      = inputPlanes.at(LUMA);
  const auto nrSamplesPlane = parameters.frameSize.area();

  const auto shiftTo8Bit = parameters.bitsPerSample - 8;
  const auto inputMax    = (1 << parameters.bitsPerSample) - 1;
  for (int i = 0; i < nrSamplesPlane; ++i)
  {
    auto yValue = getValueFromSource(lumaPlane, i, parameters.bitsPerSample, parameters.bigEndian);
    yValue      = applyMathToValue(yValue, math, inputMax);

    if (shiftTo8Bit > 0)
      yValue = clip8Bit(yValue >> shiftTo8Bit);
    if (!parameters.fullRange)
      yValue = LimitedRangeToFullRange.at(yValue);

    // Set the value for R, G and B (BGRA)
    output[0] = (unsigned char)yValue;
    output[1] = (unsigned char)yValue;
    output[2] = (unsigned char)yValue;
    output[3] = (unsigned char)255;
    output += OFFSET_TO_NEXT_RGB_VALUE;
  }
}

inline void PlanarYUV444ToRGB(const DataPointerPerChannel &inputPlanes,
                              const ConversionParameters & parameters,
                              DataPointer                  output)
{
  const auto inputMax       = (1 << parameters.bitsPerSample) - 1;
  const auto nrSamplesPlane = parameters.frameSize.area();

  for (int i = 0; i < nrSamplesPlane; ++i)
  {
    auto yuvValue = getYUVValuesFromSource(inputPlanes, i, parameters);
    yuvValue      = applyMathToValue(yuvValue, parameters.mathParameters, inputMax);

    const auto rgbValue =
        convertYUVToRGB8Bit(yuvValue,
                            getColorConversionCoefficients(parameters.colorConversion),
                            parameters.fullRange,
                            parameters.bitsPerSample);

    saveRGBValueToOutput(rgbValue, output);
    output += OFFSET_TO_NEXT_RGB_VALUE;
  }
}

StridePerComponent getStridePerComponent(const PixelFormatYUV &pixelFormat, const Size frameSize)
{
  StridePerComponent strides;
  strides[LUMA]   = frameSize.width;
  strides[CHROMA] = getChromaStrideInSamples(pixelFormat, frameSize);
  return strides;
}

NextValueOffsetPerComponent getNextValueOffsetPerComponent(const PixelFormatYUV &pixelFormat)
{
  const auto additionalSkipAlpha = pixelFormat.hasAlpha() ? 1 : 0;

  auto nextValues = NextValueOffsetPerComponent({1, 1});
  if (pixelFormat.isPlanar() && pixelFormat.getChromaPacking() == ChromaPacking::PerValue)
  {
    nextValues = {1, 2 + additionalSkipAlpha};
  }
  if (!pixelFormat.isPlanar())
  {
  }
  return nextValues;
}

const auto inputValSkip =
    format.isUVInterleaved()
        ? ((format.getPlaneOrder() == PlaneOrder::YUV || format.getPlaneOrder() == PlaneOrder::YVU)
               ? 2
               : 3)
        : 1;
}

ConversionParameters getConversionParameters(const PixelFormatYUV &    srcPixelFormat,
                                             const Size                frameSize,
                                             const ConversionSettings &conversionSettings)
{
  ConversionParameters parameters;
  parameters.frameSize          = frameSize;
  parameters.stridePerComponent = getStridePerComponent(srcPixelFormat, frameSize);
  parameters.nextValueOffsets   = return parameters;
}

} // namespace

void convertPlanarYUVToARGB(const QByteArray &        sourceBuffer,
                            PixelFormatYUV            srcPixelFormat,
                            unsigned char *           targetBuffer,
                            const Size                frameSize,
                            const ConversionSettings &conversionSettings)
{
  const auto nrSamplesPerPlane = getNrSamplesPerPlane(srcPixelFormat, frameSize);
  const auto nrBytesPerPlane   = getNrBytesPerPlane(srcPixelFormat, frameSize);

  const bool fullRange = isFullRange(conversionSettings.colorConversion);

  const auto colorConversionCoefficienst =
      getColorConversionCoefficients(conversionSettings.colorConversion);

  auto inputPlanes =
      getPointersToComponents((unsigned char *)sourceBuffer.data(), srcPixelFormat, frameSize);

  if (srcPixelFormat.getSubsampling() == Subsampling::YUV_400)
  {
    yPlaneToRGBMonochrome_444(inputPlanes, conversionParameters, targetBuffer);
    return;
  }

  if (isResamplingOfChromaNeeded(srcPixelFormat, conversionSettings.chromaInterpolation))
  {
    // If there is a chroma offset, we must resample the chroma components before we convert them
    // to RGB. If so, the resampled chroma values are saved in these arrays. We only ignore the
    // chroma offset for other interpolations then nearest neighbor.
    QByteArray uvPlaneChromaResampled[2];
    uvPlaneChromaResampled[0].resize(nrBytesPerPlane[CHROMA]);
    uvPlaneChromaResampled[1].resize(nrBytesPerPlane[CHROMA]);

    auto restrict dstU = (unsigned char *)uvPlaneChromaResampled[0].data();
    auto restrict dstV = (unsigned char *)uvPlaneChromaResampled[1].data();

    resampleChromaComponentToPlanarOutput(srcPixelFormat, frameSize, srcU, srcV, dstU, dstV);

    srcU = dstU;
    srcV = dstV;
    srcPixelFormat.setChromaPacking(ChromaPacking::Planar);
  }

  // Todo
  // Get strides, value offsets and do conversions...

  if (srcPixelFormat.getSubsampling() == Subsampling::YUV_444)
    YUVPlaneToRGB_444(componentSizeLuma,
                      mathY,
                      mathC,
                      srcY,
                      srcU,
                      srcV,
                      dst,
                      RGBConv,
                      fullRange,
                      inputMax,
                      bps,
                      srcpixelformat.isBigEndian(),
                      inputValSkip);
  else if (srcPixelFormat.getSubsampling() == Subsampling::YUV_422)
    YUVPlaneToRGB_422(w,
                      h,
                      mathY,
                      mathC,
                      srcY,
                      srcU,
                      srcV,
                      dst,
                      RGBConv,
                      fullRange,
                      inputMax,
                      interpolation,
                      bps,
                      srcpixelformat.isBigEndian(),
                      inputValSkip);
  else if (srcPixelFormat.getSubsampling() == Subsampling::YUV_420)
    YUVPlaneToRGB_420(w,
                      h,
                      mathY,
                      mathC,
                      srcY,
                      srcU,
                      srcV,
                      dst,
                      RGBConv,
                      fullRange,
                      inputMax,
                      interpolation,
                      bps,
                      srcpixelformat.isBigEndian(),
                      inputValSkip);
  else if (srcPixelFormat.getSubsampling() == Subsampling::YUV_440)
    YUVPlaneToRGB_440(w,
                      h,
                      mathY,
                      mathC,
                      srcY,
                      srcU,
                      srcV,
                      dst,
                      RGBConv,
                      fullRange,
                      inputMax,
                      interpolation,
                      bps,
                      srcpixelformat.isBigEndian(),
                      inputValSkip);
  else if (srcPixelFormat.getSubsampling() == Subsampling::YUV_410)
    YUVPlaneToRGB_410(w,
                      h,
                      mathY,
                      mathC,
                      srcY,
                      srcU,
                      srcV,
                      dst,
                      RGBConv,
                      fullRange,
                      inputMax,
                      interpolation,
                      bps,
                      srcpixelformat.isBigEndian(),
                      inputValSkip);
  else if (srcPixelFormat.getSubsampling() == Subsampling::YUV_411)
    YUVPlaneToRGB_411(w,
                      h,
                      mathY,
                      mathC,
                      srcY,
                      srcU,
                      srcV,
                      dst,
                      RGBConv,
                      fullRange,
                      inputMax,
                      interpolation,
                      bps,
                      srcpixelformat.isBigEndian(),
                      inputValSkip);
  else
    return false;
}

} // namespace video::yuv
