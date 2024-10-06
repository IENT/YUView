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

#include "CreateTestData.h"

namespace video::rgb::test
{

namespace
{

void scaleValueToBitDepthAndPushIntoArra(QByteArray      &data,
                                         const unsigned   value,
                                         const int        bitDepth,
                                         const Endianness endianness)
{
  const auto shift = 12 - bitDepth;
  if (bitDepth == 8)
    data.push_back(value >> shift);
  else
  {
    const auto scaledValue = (value >> shift);
    const auto upperByte   = ((scaledValue & 0xff00) >> 8);
    const auto lowerByte   = (scaledValue & 0xff);
    if (endianness == Endianness::Little)
    {
      data.push_back(lowerByte);
      data.push_back(upperByte);
    }
    else
    {
      data.push_back(upperByte);
      data.push_back(lowerByte);
    }
  }
}

} // namespace

auto createRawRGBData(const PixelFormatRGB &format) -> QByteArray
{
  QByteArray data;
  const auto bitDepth   = format.getBitsPerSample();
  const auto endianness = format.getEndianess();

  if (format.getDataLayout() == DataLayout::Packed)
  {
    for (auto value : TEST_VALUES_12BIT)
    {
      for (int channelPosition = 0; channelPosition < static_cast<int>(format.nrChannels());
           channelPosition++)
      {
        const auto channel = format.getChannelAtPosition(channelPosition);
        scaleValueToBitDepthAndPushIntoArra(data, value[channel], bitDepth, endianness);
      }
    }
  }
  else
  {
    for (int channelPosition = 0; channelPosition < static_cast<int>(format.nrChannels());
         channelPosition++)
    {
      const auto channel = format.getChannelAtPosition(channelPosition);
      for (auto value : TEST_VALUES_12BIT)
        scaleValueToBitDepthAndPushIntoArra(data, value[channel], bitDepth, endianness);
    }
  }

  data.squeeze();
  return data;
}

} // namespace video::rgb::test
