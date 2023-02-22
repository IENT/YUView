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

#include "YUVFramesProvider.h"

#include <common/Error.h>

namespace helper
{

namespace
{

QByteArray generatePlane(Size planeSize, int bitDepth)
{
  assert(bitDepth > 0 && bitDepth <= 16);
  auto nrBytesPerSample = (bitDepth + 7) / 8;
  auto maxValue         = (1 << bitDepth) - 1;

  const auto nrSamples = planeSize.width * planeSize.height * nrBytesPerSample;
  const auto xPlusYMax = planeSize.width + planeSize.height;

  QByteArray data;
  data.resize(nrSamples);

  if (nrBytesPerSample == 1)
  {
    auto rawPtr = (uint8_t *)(data.data());
    for (unsigned y = 0; y < planeSize.height; y++)
    {
      for (unsigned x = 0; x < planeSize.width; x++)
      {
        const auto val = (x + y) * maxValue / xPlusYMax;
        rawPtr[x]      = uint8_t(val);
      }
      rawPtr += planeSize.width;
    }
  }
  else if (nrBytesPerSample == 2)
  {
    auto rawPtr = (uint16_t *)(data.data());
    for (unsigned y = 0; y < planeSize.height; y++)
    {
      for (unsigned x = 0; x < planeSize.width; x++)
      {
        const auto val = (x + y) * maxValue / xPlusYMax;
        rawPtr[x]      = uint16_t(val);
      }
      rawPtr += planeSize.width;
    }
  }
  else
    assert(false);

  return data;
}

} // namespace

YUVFramesProvider::YUVFramesProvider(video::videoHandler *videoHandler)
{
  this->videoHandler = videoHandler;
  connect(videoHandler,
          &video::videoHandler::signalRequestFrame,
          this,
          &YUVFramesProvider::onSignalRequestFrame);
  connect(videoHandler,
          &video::videoHandler::signalRequestRawData,
          this,
          &YUVFramesProvider::onSignalRequestRawData);
}

void YUVFramesProvider::onSignalRequestFrame(int frameIndex, bool caching)
{
  // Provide a frame ...
  int debugStop = 234;
}

void YUVFramesProvider::onSignalRequestRawData(int frameIndex, bool caching)
{
  // Provide raw data ...
  int debugStop = 234;
}

QByteArray generateYUVPlane(const Size &planeSize, const video::yuv::PixelFormatYUV &pixelFormat)
{
  QByteArray data;
  data += generatePlane(planeSize, pixelFormat.getBitsPerSample());

  auto chromaSize = Size(planeSize.width / pixelFormat.getSubsamplingHor(),
                         planeSize.height / pixelFormat.getSubsamplingVer());
  for (unsigned planeIdx = 1; planeIdx < pixelFormat.getNrPlanes(); planeIdx++)
    data += generatePlane(chromaSize, pixelFormat.getBitsPerSample());
  return data;
}

QByteArray
generateYUVVideo(const Size &frameSize, const video::yuv::PixelFormatYUV &pixelFormat, int nrFrames)
{
  if (pixelFormat.isValid())
    return {};
  if (!pixelFormat.isPlanar() || pixelFormat.isUVInterleaved())
    throw NotImplemented();

  QByteArray data;
  for (int frameNr = 0; frameNr < nrFrames; frameNr++)
    data += generateYUVPlane(frameSize, pixelFormat);
  return data;
}

} // namespace helper
