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

#include <common/Testing.h>

#include <video/rgb/PixelFormatRGB.h>

namespace video::rgb::test
{

namespace
{

std::vector<PixelFormatRGB> getAllFormats()
{
  std::vector<PixelFormatRGB> allFormats;

  for (int bitsPerPixel = 8; bitsPerPixel <= 16; bitsPerPixel++)
    for (auto dataLayout : DataLayoutMapper.getItems())
      for (auto channelOrder : ChannelOrderMapper.getItems())
        for (auto alphaMode : AlphaModeMapper.getItems())
          for (auto endianness : EndianessMapper.getItems())
            allFormats.push_back(
                PixelFormatRGB(bitsPerPixel, dataLayout, channelOrder, alphaMode, endianness));

  return allFormats;
}

} // namespace

TEST(PixelFormatRGBTest, testFormatFromToString)
{
  for (auto fmt : getAllFormats())
  {
    const auto name = fmt.getName();
    EXPECT_TRUE(fmt.isValid()) << "Format " << name << " is invalid.";
    EXPECT_FALSE(name.empty()) << "Format " << name << " getName is empty.";

    const auto fmtNew = PixelFormatRGB(name);
    EXPECT_EQ(fmt, fmtNew) << "New format " << fmtNew.getName() << " unequal to initial format "
                           << name;

    EXPECT_EQ(fmt.getChannelPosition(Channel::Red), fmtNew.getChannelPosition(Channel::Red))
        << "Format " << name << " channel position R missmatch";
    EXPECT_EQ(fmt.getChannelPosition(Channel::Green), fmtNew.getChannelPosition(Channel::Green))
        << "Format " << name << " channel position G missmatch";
    EXPECT_EQ(fmt.getChannelPosition(Channel::Blue), fmtNew.getChannelPosition(Channel::Blue))
        << "Format " << name << " channel position B missmatch";
    EXPECT_EQ(fmt.getChannelPosition(Channel::Alpha), fmtNew.getChannelPosition(Channel::Alpha))
        << "Format " << name << " channel position A missmatch";
    EXPECT_EQ(fmt.getBitsPerSample(), fmtNew.getBitsPerSample())
        << "Format " << name << " bits per sample missmatch";
    EXPECT_EQ(fmt.getDataLayout(), fmtNew.getDataLayout())
        << "Format " << name << " data layout missmatch";

    if (fmt.hasAlpha())
    {
      EXPECT_EQ(fmt.nrChannels(), 4u) << "Format " << name << " alpha channel indication wrong. ";
    }
    else
    {
      EXPECT_EQ(fmt.nrChannels(), 3u) << "Format " << name << " alpha channel indication wrong. ";
    }
  }
}

TEST(PixelFormatRGBTest, testInvalidFormats)
{
  std::vector<PixelFormatRGB> invalidFormats;
  invalidFormats.push_back(PixelFormatRGB(0, video::DataLayout::Packed, ChannelOrder::RGB));
  invalidFormats.push_back(PixelFormatRGB(1, video::DataLayout::Packed, ChannelOrder::RGB));
  invalidFormats.push_back(PixelFormatRGB(7, video::DataLayout::Packed, ChannelOrder::RGB));
  invalidFormats.push_back(PixelFormatRGB(17, video::DataLayout::Packed, ChannelOrder::RGB));
  invalidFormats.push_back(PixelFormatRGB(200, video::DataLayout::Packed, ChannelOrder::RGB));

  for (auto fmt : invalidFormats)
    EXPECT_FALSE(fmt.isValid()) << "Format " << fmt.getName() << " should be invalid.";
}

} // namespace video::rgb::test
