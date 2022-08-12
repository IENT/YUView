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

#include "YUVGenerator.h"

#include <common/Error.h>

namespace helper
{

namespace
{

template <int bitDepth> QByteArray generatePlane(Size planeSize)
{
  typedef typename std::conditional<bitDepth == 8, uint8_t *, uint16_t *>::type InValueType;
  static_assert(bitDepth > 0 && bitDepth <= 16);
  constexpr auto nrBytesPerSample = (bitDepth + 7) / 8;
  constexpr auto maxValue         = (1 << bitDepth) - 1;

  const auto nrSamples = planeSize.width * planeSize.height * nrBytesPerSample;
  const auto xPlusYMax = planeSize.width + planeSize.height;

  QByteArray data;
  data.resize(nrSamples);
  const auto *rawPtr = InValueType(data.data());

  for (unsigned x = 0; x < planeSize.width; x++)
  {
    for (unsigned y = 0; y < planeSize.height; y++)
    {
      const auto val = (x + y) * maxValue / xPlusYMax;
      const auto idx = y * planeSize.width + x;
      rawPtr[idx]    = InValueType(val);
    }
  }
  return data;
}

} // namespace

QByteArray generateYUVVideo(Size frameSize, int nrFrames, video::yuv::PixelFormatYUV pixelFormat)
{
  if (pixelFormat.isValid())
    return {};
  if (!pixelFormat.isPlanar() || pixelFormat.isUVInterleaved())
    throw NotImplemented();

  QByteArray data;
  for (int frameNr = 0; frameNr < nrFrames; frameNr++)
  {
    data += generatePlane(frameSize, pixelFormat.getBitsPerSample());

    auto chromaSize = Size(frameSize.width / pixelFormat.getSubsamplingHor(),
                           frameSize.height / pixelFormat.getSubsamplingVer());
    for (int planeIdx = 1; planeIdx < pixelFormat.getNrPlanes(); planeIdx++)
      data += generatePlane(chromaSize, pixelFormat.getBitsPerSample());
  }
  return data;
}

} // namespace helper
