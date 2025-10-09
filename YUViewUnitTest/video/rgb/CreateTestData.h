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

#include "video/PixelFormat.h"
#include <QByteArray>
#include <array>
#include <optional>
#include <vector>
#include <video/rgb/PixelFormatRGB.h>

namespace video::rgb::test
{

constexpr auto createTestSetOfPixelFormatRGB()
{
  constexpr std::array bitDepthsGreater8{9, 10, 12, 16, 32};

  const auto nrFormats8Bit =
    DataLayoutMapper.size() * ChannelOrderMapper.size() * AlphaModeMapper.size();
  const auto nrFormatsGreater8Bit = bitDepthsGreater8.size() * DataLayoutMapper.size() *
                                    ChannelOrderMapper.size() * AlphaModeMapper.size() *
                                    EndianessMapper.size();
  const auto nrFormatsPredefined = PredefinedPixelFormatMapper.size() * 2;

  const auto nrFormats = nrFormats8Bit + nrFormatsGreater8Bit + nrFormatsPredefined;

  std::array<PixelFormatRGB, nrFormats> pixelFormats;

  size_t i = 0;
  for (const auto dataLayout : DataLayoutMapper.getValues())
    for (const auto channelOrder : ChannelOrderMapper.getValues())
      for (const auto alphaMode : AlphaModeMapper.getValues())
        pixelFormats[i++] =
          PixelFormatRGB(8, dataLayout, channelOrder, alphaMode, Endianness::Little);

  for (const auto bitDepth : bitDepthsGreater8)
    for (const auto dataLayout : DataLayoutMapper.getValues())
      for (const auto channelOrder : ChannelOrderMapper.getValues())
        for (const auto alphaMode : AlphaModeMapper.getValues())
          for (const auto endianess : EndianessMapper.getValues())
            pixelFormats[i++] =
              PixelFormatRGB(bitDepth, dataLayout, channelOrder, alphaMode, endianess);

  for (const auto predefinedPixelFormat : PredefinedPixelFormatMapper.getValues())
  {
    pixelFormats[i++] = PixelFormatRGB(predefinedPixelFormat);
    pixelFormats[i++] = PixelFormatRGB(predefinedPixelFormat, Endianness::Big);
  }

  assert(i == nrFormats);

  return pixelFormats;
}

const std::vector<rgba_t> TEST_VALUES_12BIT{rgba_t({0, 0, 0, 0}),
                                            rgba_t({156, 0, 0, 0}),
                                            rgba_t({560, 0, 0, 98}),
                                            rgba_t({1023, 0, 0, 700}),
                                            rgba_t({0, 156, 0, 852}),
                                            rgba_t({0, 760, 0, 0}),
                                            rgba_t({0, 1023, 0, 230}),
                                            rgba_t({0, 0, 156, 0}),
                                            rgba_t({0, 0, 576, 0}),
                                            rgba_t({0, 0, 1023, 0}),
                                            rgba_t({213, 214, 265, 1023}),
                                            rgba_t({1023, 78, 234, 1023}),
                                            rgba_t({1023, 1023, 3, 0}),
                                            rgba_t({16, 1023, 22, 0}),
                                            rgba_t({1023, 1023, 1023, 0}),
                                            rgba_t({1023, 1023, 1023, 1023})};
constexpr Size            TEST_FRAME_SIZE      = {4, 4};
constexpr int             TEST_FRAME_NR_VALUES = TEST_FRAME_SIZE.width * TEST_FRAME_SIZE.height;

// This function reorders the rgb values into the QByteArray in the right order. If the values are
// in a different bit depth then the pixel format, the values will be scaled. Only supports the
// non-predefined pixel formats.
QByteArray createRawRGBData(const PixelFormatRGB      &format,
                            const std::vector<rgba_t> &values,
                            const int                  valuesBitDepth);

// Same conversion function for predefined pixel formats. If valuesBitDepth is given, a bit depth
// conversion will be performed. If not, the values are assumed to be in the right bit depth.
QByteArray createRawRGBData(const PredefinedPixelFormat predefinedPixelFormat,
                            const Endianness            endianess,
                            const std::vector<rgba_t>  &values,
                            std::optional<int>          valuesBitDepth = std::nullopt);

} // namespace video::rgb::test
