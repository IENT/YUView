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

#include "Conversion.h"

#include <video/yuv/conversion/ChromaResampling.h>
#include <video/yuv/conversion/ConversionYUVMonochrome.h>
#include <video/yuv/conversion/ConversionYUVPlanar.h>
#include <video/yuv/conversion/PackedToPlanar.h>
#include <video/yuv/conversion/SpecializedConversions.h>

namespace video::yuv
{

using namespace video::yuv::conversion;

namespace
{

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

DataPointerPerChannel getPointersToComponents(ConstDataPointer      arrayPointer,
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

  ConstDataPointer ptrA{};
  if (pixelFormat.hasAlpha())
    ptrA = ptrU + offsetToNextChromaPlane * bytesPerSample;

  const auto planeOrder = pixelFormat.getPlaneOrder();
  if (planeOrder == PlaneOrder::YUV || planeOrder == PlaneOrder::YUVA)
    return {ptrY, ptrU, ptrV, ptrA};
  else
    return {ptrY, ptrV, ptrU, ptrA};
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
    // Later. But this should also work
    assert(false);
  }
  return nextValues;
}

ConversionParameters getConversionParameters(const ConversionSettings &settings)
{
  ConversionParameters parameters;
  parameters.frameSize          = settings.frameSize;
  parameters.stridePerComponent = getStridePerComponent(settings.pixelFormat, settings.frameSize);
  parameters.nextValueOffsets   = getNextValueOffsetPerComponent(settings.pixelFormat);
  parameters.mathParameters     = settings.mathParametersPerComponent;
  parameters.bitsPerSample      = settings.pixelFormat.getBitsPerSample();
  parameters.bigEndian          = settings.pixelFormat.isBigEndian();
  parameters.colorConversion    = settings.colorConversion;
  return parameters;
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

void convertPlanarYUVToARGB(DataPointerPerChannel inputPlanes,
                            ConversionSettings    settings,
                            DataPointer           targetBuffer)
{
  auto       pixelFormat = settings.pixelFormat;
  const auto frameSize   = settings.frameSize;

  if (pixelFormat.getSubsampling() == Subsampling::YUV_400)
  {
    const auto conversionParameters = getConversionParameters(settings);

    yPlaneToRGBMonochrome(inputPlanes.at(LUMA), conversionParameters, targetBuffer);
    return;
  }

  ByteVector tempDataU;
  ByteVector tempDataV;

  if (isResamplingOfChromaNeeded(pixelFormat, settings.chromaInterpolation))
  {
    // If there is a chroma offset, we must resample the chroma components before we convert them
    // to RGB. If so, the resampled chroma values are saved in these arrays. We only ignore the
    // chroma offset for other interpolations then nearest neighbor.

    const auto nrBytesPerPlane = getNrBytesPerPlane(pixelFormat, frameSize);

    tempDataU.resize(nrBytesPerPlane[CHROMA]);
    tempDataV.resize(nrBytesPerPlane[CHROMA]);

    auto restrict dstU = (unsigned char *)tempDataU.data();
    auto restrict dstV = (unsigned char *)tempDataV.data();

    resampleChromaComponentToPlanarOutput(
        pixelFormat, frameSize, inputPlanes.at(CHROMA_U), inputPlanes.at(CHROMA_V), dstU, dstV);

    inputPlanes[CHROMA_U] = dstU;
    inputPlanes[CHROMA_V] = dstV;

    settings.pixelFormat.setChromaPacking(ChromaPacking::Planar);
  }

  const auto conversionParameters = getConversionParameters(settings);

  const auto subsampling = pixelFormat.getSubsampling();
  if (subsampling == Subsampling::YUV_444)
    yuvToRGB444(inputPlanes, conversionParameters, targetBuffer);
  else if (subsampling == Subsampling::YUV_422)
    yuvToRGB422(inputPlanes, conversionParameters, targetBuffer);
  else if (subsampling == Subsampling::YUV_420)
    yuvToRGB420(inputPlanes, conversionParameters, targetBuffer);
  else if (subsampling == Subsampling::YUV_440)
    yuvToRGB440(inputPlanes, conversionParameters, targetBuffer);
  else if (subsampling == Subsampling::YUV_410)
    yuvToRGB410(inputPlanes, conversionParameters, targetBuffer);
  else if (subsampling == Subsampling::YUV_411)
    yuvToRGB411(inputPlanes, conversionParameters, targetBuffer);
}

} // namespace

void convertYUVPlanarToARGB(const ConstDataPointer    sourceBuffer,
                            const ConversionSettings &settings,
                            DataPointer               targetBuffer)
{
  const auto parameters = getConversionParameters(settings);
  auto       inputPlanes =
      getPointersToComponents(sourceBuffer, settings.pixelFormat, settings.frameSize);

  const auto pixelFormat = settings.pixelFormat;
  if ((pixelFormat.getBitsPerSample() == 8 || pixelFormat.getBitsPerSample() == 10) &&
      pixelFormat.getSubsampling() == Subsampling::YUV_420 &&
      settings.chromaInterpolation == ChromaInterpolation::NearestNeighbor &&
      pixelFormat.getChromaOffset().x == 0 && pixelFormat.getChromaOffset().y == 1 &&
      settings.componentDisplayMode == ComponentDisplayMode::DisplayAll &&
      pixelFormat.getChromaPacking() == ChromaPacking::Planar &&
      !settings.mathParametersPerComponent.at(LUMA).mathRequired() &&
      !settings.mathParametersPerComponent.at(CHROMA).mathRequired())
  // 8/10 bit 4:2:0, nearest neighbor, chroma offset (0,1) (the default for 4:2:0), all components
  // displayed and no yuv math. We can use a specialized function for this.
  {
    yuvToRGB4208Or10BitNearestNeighbourNoMath(inputPlanes, parameters, targetBuffer);
    return;
  }

  const auto component = settings.componentDisplayMode;
  if (component == ComponentDisplayMode::DisplayY)
  {
    yPlaneToRGBMonochrome(inputPlanes.at(LUMA), parameters, targetBuffer);
  }
  else if (component == ComponentDisplayMode::DisplayCb ||
           component == ComponentDisplayMode::DisplayCr)
  {
    const auto inputPlane = component == ComponentDisplayMode::DisplayCb ? inputPlanes.at(CHROMA_U)
                                                                         : inputPlanes.at(CHROMA_V);

    if (settings.pixelFormat.getSubsampling() == Subsampling::YUV_444)
      uvPlaneToRGBMonochrome444(inputPlane, parameters, targetBuffer);
    else if (settings.pixelFormat.getSubsampling() == Subsampling::YUV_422)
      uvPlaneToRGBMonochrome422(inputPlane, parameters, targetBuffer);
    else if (settings.pixelFormat.getSubsampling() == Subsampling::YUV_420)
      uvPlaneToRGBMonochrome420(inputPlane, parameters, targetBuffer);
    else if (settings.pixelFormat.getSubsampling() == Subsampling::YUV_440)
      uvPlaneToRGBMonochrome440(inputPlane, parameters, targetBuffer);
    else if (settings.pixelFormat.getSubsampling() == Subsampling::YUV_410)
      uvPlaneToRGBMonochrome410(inputPlane, parameters, targetBuffer);
    else if (settings.pixelFormat.getSubsampling() == Subsampling::YUV_411)
      uvPlaneToRGBMonochrome411(inputPlane, parameters, targetBuffer);
  }
  else
  {
    convertPlanarYUVToARGB(inputPlanes, settings, targetBuffer);
  }
}

void convertYUVToARGB(ConstDataPointer          sourceBuffer,
                      const ConversionSettings &settings,
                      DataPointer               targetBuffer)
{
  if (settings.pixelFormat.isPlanar())
    convertYUVPlanarToARGB(sourceBuffer, settings, targetBuffer);
  else
  {
    const auto [planarData, planarPixelFormat] =
        convertPackedToPlanar(sourceBuffer, settings.pixelFormat, settings.frameSize);
    auto newDataPointer = planarData.data();
    convertYUVPlanarToARGB(newDataPointer, settings, targetBuffer);
  }
}

} // namespace video::yuv
