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

#include <QByteArray>

#include "PixelFormatRGB.h"

namespace video::rgb
{

template <typename T> struct DataPointers
{
  const T *r;
  const T *g;
  const T *b;

  DataPointers operator+=(const int offset)
  {
    this->r += offset;
    this->g += offset;
    this->b += offset;
    return *this;
  }
};

template <typename T>
DataPointers<T> calculatePointersToStartOfComponents(const QByteArray     &rawFrameData,
                                                     const Size           &frameSize,
                                                     const PixelFormatRGB &pixelFormat)
{
  if (!pixelFormat.isValid())
    throw std::invalid_argument("Pixel format must be valid");
  if (!frameSize)
    throw std::invalid_argument("Frame size must be valid");
  if (rawFrameData.size() < pixelFormat.getBytesPerFrame(frameSize))
    throw std::invalid_argument("Raw frame data too small");

  const auto posR = pixelFormat.getChannelPosition(Channel::Red);
  const auto posG = pixelFormat.getChannelPosition(Channel::Green);
  const auto posB = pixelFormat.getChannelPosition(Channel::Blue);

  const auto castDataPointer = reinterpret_cast<T const *>(rawFrameData.data());

  if (pixelFormat.getDataLayout() == DataLayout::Planar)
  {
    const auto offsetToNextPlane = frameSize.width * frameSize.height;

    return {.r = castDataPointer + (posR * offsetToNextPlane),
            .g = castDataPointer + (posG * offsetToNextPlane),
            .b = castDataPointer + (posB * offsetToNextPlane)};
  }

  return {.r = castDataPointer + posR, .g = castDataPointer + posG, .b = castDataPointer + posB};
}

inline rgba_t extractRGB565Value(const unsigned char *data, const Endianness endianess)
{
  int byte1 = *data;
  int byte2 = *(data + 1);

  if (endianess == Endianness::Big)
    std::swap(byte1, byte2);

  const auto value = byte1 + (byte2 << 8);

  const int r = (value & 0b00000000'00011111);
  const int g = ((value & 0b00000111'11100000) >> 5);
  const int b = ((value & 0b11111000'00000000) >> 11);

  return {r, g, b, 255};
}

} // namespace video::rgb
