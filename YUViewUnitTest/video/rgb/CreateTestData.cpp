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
#include "video/PixelFormat.h"
#include "video/rgb/PixelFormatRGB.h"
#include <stdexcept>

namespace video::rgb::test
{

namespace
{

void scaleValueToBitDepthAndPushIntoArray(QByteArray      &data,
                                          const unsigned   value,
                                          const int        valueBitDepth,
                                          const int        outputBitDepth,
                                          const Endianness endianness)
{
  const auto scaledValue = convertBitness(value, valueBitDepth, outputBitDepth);

  if (outputBitDepth == 8)
    data.push_back(scaledValue);
  else if (outputBitDepth <= 16)
  {
    const auto upperByte = ((scaledValue & 0xff00) >> 8);
    const auto lowerByte = (scaledValue & 0xff);
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
  else
  {
    if (endianness == Endianness::Little)
    {
      data.push_back((scaledValue >> 0) & 0xFF);
      data.push_back((scaledValue >> 8) & 0xFF);
      data.push_back((scaledValue >> 16) & 0xFF);
      data.push_back((scaledValue >> 24) & 0xFF);
    }
    else
    {
      data.push_back((scaledValue >> 24) & 0xFF);
      data.push_back((scaledValue >> 16) & 0xFF);
      data.push_back((scaledValue >> 8) & 0xFF);
      data.push_back((scaledValue >> 0) & 0xFF);
    }
  }
}

void scaleValueToRGB565AndPushIntoArray(QByteArray      &data,
                                        const rgba_t    &value,
                                        const Endianness endianess)
{
  int dataBytes = (value.r & 0b00000000'00011111) + ((value.g << 5) & 0b00000111'11100000) +
                  ((value.b << 11) & 0b11111000'00000000);

  const int byte1 = (dataBytes >> 8);
  const int byte2 = (dataBytes & 0b1111'1111);

  if (endianess == Endianness::Big)
  {
    data.push_back(byte1);
    data.push_back(byte2);
  }
  else
  {
    data.push_back(byte2);
    data.push_back(byte1);
  }
}

} // namespace

QByteArray createRawRGBData(const PixelFormatRGB      &format,
                            const std::vector<rgba_t> &values,
                            const int                  valuesBitDepth)
{
  QByteArray data;

  const auto bitDepth  = format.getBitsPerComponent();
  const auto endianess = format.getEndianess();

  if (format.getPredefinedPixelFormat() == PredefinedPixelFormat::RGB565)
  {
    for (const auto value : values)
      scaleValueToRGB565AndPushIntoArray(data, value, endianess);
  }
  else if (format.getPredefinedPixelFormat())
  {
    throw std::logic_error("Support for pixel format not implemented");
  }
  else
  {
    if (format.getDataLayout() == DataLayout::Packed)
    {
      for (const auto value : values)
      {
        for (int channelPosition = 0; channelPosition < static_cast<int>(format.getNrChannels());
             channelPosition++)
        {
          const auto channel = format.getChannelAtPosition(channelPosition);
          scaleValueToBitDepthAndPushIntoArray(
            data, value.at(channel), valuesBitDepth, bitDepth, endianess);
        }
      }
    }
    else
    {
      for (int channelPosition = 0; channelPosition < static_cast<int>(format.getNrChannels());
           channelPosition++)
      {
        const auto channel = format.getChannelAtPosition(channelPosition);
        for (const auto value : values)
          scaleValueToBitDepthAndPushIntoArray(
            data, value.at(channel), valuesBitDepth, bitDepth, endianess);
      }
    }
  }

  data.squeeze();
  return data;
}

} // namespace video::rgb::test
