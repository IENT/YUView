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
#include <array>
#include <vector>
#include <video/rgb/PixelFormatRGB.h>

namespace video::rgb::test
{

constexpr auto createTestSetOfPixelFormatRGB()
{
  constexpr std::array bitDepths{8, 9, 10, 12, 16, 32};

  const auto nrFormats = bitDepths.size() * DataLayoutMapper.size() * ChannelOrderMapper.size() +
                         PredefinedPixelFormatMapper.size();

  std::array<PixelFormatRGB, nrFormats> pixelFormats;

  size_t i = 0;
  for (const auto bitDepth : bitDepths)
    for (const auto dataLayout : DataLayoutMapper.getValues())
      for (const auto channelOrder : ChannelOrderMapper.getValues())
        pixelFormats[i++] = PixelFormatRGB(bitDepth, dataLayout, channelOrder);

  for (const auto pixelFormat : PredefinedPixelFormatMapper.getValues())
    pixelFormats[i++] = pixelFormat;

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
// in a different bit depth then the pixel format, the values will be scaled.
// The only exception is RGB565 where the values will not be scaled.
QByteArray createRawRGBData(const PixelFormatRGB      &format,
                            const std::vector<rgba_t> &value,
                            const int                  valuesBitDepth);

} // namespace video::rgb::test
