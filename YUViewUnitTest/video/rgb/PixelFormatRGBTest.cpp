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

#include "video/PixelFormat.h"
#include <common/Testing.h>

#include <video/rgb/PixelFormatRGB.h>

namespace video::rgb::test
{

namespace
{

std::vector<PixelFormatRGB> getAllValidFormats()
{
  std::vector<PixelFormatRGB> allFormats;

  for (int bitsPerPixel = 8; bitsPerPixel <= 32; bitsPerPixel++)
    for (auto dataLayout : DataLayoutMapper.getValues())
      for (auto channelOrder : ChannelOrderMapper.getValues())
        for (auto alphaMode : AlphaModeMapper.getValues())
          for (auto endianness : EndianessMapper.getValues())
          {
            if (endianness == Endianness::Big && bitsPerPixel == 8)
              continue;

            allFormats.push_back(
              PixelFormatRGB(bitsPerPixel, dataLayout, channelOrder, alphaMode, endianness));
          }

  allFormats.push_back(PixelFormatRGB(PredefinedPixelFormat::RGB565));
  allFormats.push_back(PixelFormatRGB(PredefinedPixelFormat::RGB565, Endianness::Big));

  return allFormats;
}

std::vector<PixelFormatRGB> getInvalidFormats()
{
  std::vector<PixelFormatRGB> invalidFormats;

  invalidFormats.push_back(PixelFormatRGB());

  // If the bitrate is < 8 or > 32, the format is invalid. We can not add all cases to the list
  // though.
  invalidFormats.push_back(PixelFormatRGB(0, video::DataLayout::Packed, ChannelOrder::RGB));
  invalidFormats.push_back(PixelFormatRGB(1, video::DataLayout::Packed, ChannelOrder::RGB));
  invalidFormats.push_back(PixelFormatRGB(7, video::DataLayout::Packed, ChannelOrder::RGB));
  invalidFormats.push_back(PixelFormatRGB(33, video::DataLayout::Packed, ChannelOrder::RGB));
  invalidFormats.push_back(PixelFormatRGB(200, video::DataLayout::Packed, ChannelOrder::RGB));

  for (auto dataLayout : DataLayoutMapper.getValues())
    for (auto channelOrder : ChannelOrderMapper.getValues())
      for (auto alphaMode : AlphaModeMapper.getValues())
        invalidFormats.push_back(
          PixelFormatRGB(8, dataLayout, channelOrder, alphaMode, Endianness::Big));

  return invalidFormats;
}

} // namespace

TEST(PixelFormatRGBTest, testFormatFromToString)
{
  for (auto fmt : getAllValidFormats())
  {
    const auto name = fmt.getName().value();
    EXPECT_TRUE(fmt.isValid()) << "Format " << name << " is invalid.";
    EXPECT_FALSE(name.empty()) << "Format " << name << " getName is empty.";

    const auto fmtNew = PixelFormatRGB(name);
    EXPECT_EQ(fmt, fmtNew) << "New format " << *fmtNew.getName() << " unequal to initial format "
                           << name;

    EXPECT_EQ(fmt.getChannelPosition(Channel::Red), fmtNew.getChannelPosition(Channel::Red))
      << "Format " << name << " channel position R missmatch";
    EXPECT_EQ(fmt.getChannelPosition(Channel::Green), fmtNew.getChannelPosition(Channel::Green))
      << "Format " << name << " channel position G missmatch";
    EXPECT_EQ(fmt.getChannelPosition(Channel::Blue), fmtNew.getChannelPosition(Channel::Blue))
      << "Format " << name << " channel position B missmatch";
    EXPECT_EQ(fmt.getChannelPosition(Channel::Alpha), fmtNew.getChannelPosition(Channel::Alpha))
      << "Format " << name << " channel position A missmatch";
    EXPECT_EQ(fmt.getBitsPerComponent(), fmtNew.getBitsPerComponent())
      << "Format " << name << " bits per sample missmatch";
    EXPECT_EQ(fmt.getDataLayout(), fmtNew.getDataLayout())
      << "Format " << name << " data layout missmatch";

    if (fmt.hasAlpha())
    {
      EXPECT_EQ(fmt.getNrChannels(), 4) << "Format " << name << " alpha channel indication wrong. ";
    }
    else
    {
      EXPECT_EQ(fmt.getNrChannels(), 3) << "Format " << name << " alpha channel indication wrong. ";
    }
  }
}

TEST(PixelFormatRGBTest, testInvalidFormats)
{
  for (const auto &format : getInvalidFormats())
    EXPECT_FALSE(format.isValid()) << "Format " << *format.getName() << " should be invalid.";
}

TEST(PixelFormatRGBTest, testComparisonOperatorsForValidFormat)
{
  const auto allValidFormats = getAllValidFormats();

  for (size_t i = 0; i < allValidFormats.size(); ++i)
    for (size_t j = 0; j < allValidFormats.size(); ++j)
    {
      const auto shouldBeEqual = (i == j);
      if (shouldBeEqual)
      {
        EXPECT_TRUE(allValidFormats.at(i) == allValidFormats.at(j));
        EXPECT_FALSE(allValidFormats.at(i) != allValidFormats.at(j));
        EXPECT_TRUE(allValidFormats.at(i) == allValidFormats.at(j).getName());
        EXPECT_FALSE(allValidFormats.at(i) != allValidFormats.at(j).getName());
      }
      else
      {
        EXPECT_FALSE(allValidFormats.at(i) == allValidFormats.at(j));
        EXPECT_TRUE(allValidFormats.at(i) != allValidFormats.at(j));
        EXPECT_FALSE(allValidFormats.at(i) == allValidFormats.at(j).getName());
        EXPECT_TRUE(allValidFormats.at(i) != allValidFormats.at(j).getName());
      }
    }
}

TEST(PixelFormatRGBTest, testComparisonOperators_ComparingToInvalidFormat_shouldAlwaysBeUnequal)
{
  const PixelFormatRGB invalidFormat;

  for (const auto &format : getAllValidFormats())
  {
    EXPECT_FALSE(format == invalidFormat);
    EXPECT_TRUE(format != invalidFormat);
    EXPECT_FALSE(format == invalidFormat.getName());
    EXPECT_TRUE(format != invalidFormat.getName());
  }
}

TEST(PixelFormatRGBTest, testComparisonOperators_ComparingTwoInvalidFormats_shouldAlwaysBeUnequal)
{
  const auto invalidFormats = getInvalidFormats();

  for (size_t i = 0; i < invalidFormats.size(); ++i)
    for (size_t j = 0; j < invalidFormats.size(); ++j)
    {
      EXPECT_FALSE(invalidFormats.at(i) == invalidFormats.at(j));
      EXPECT_TRUE(invalidFormats.at(i) != invalidFormats.at(j));
      EXPECT_FALSE(invalidFormats.at(i) == invalidFormats.at(j).getName());
      EXPECT_TRUE(invalidFormats.at(i) != invalidFormats.at(j).getName());
    }
}

TEST(PixelFormatRGBTest, testBrightnessCalculation)
{
  constexpr rgba_t black           = {0, 0, 0, 255};
  constexpr rgba_t lowerLuminance  = {126, 125, 124, 255};
  constexpr rgba_t higherLuminance = {130, 130, 130, 255};
  constexpr rgba_t white           = {255, 255, 255, 255};

  auto scaleRgbToPixelFormatBitDepth = [](const PixelFormatRGB &pixelFormat, rgba_t value) -> rgba_t
  {
    if (pixelFormat.getPredefinedPixelFormat())
      return {value.r >> 3, value.g << 2, value.b << 3, 255};

    const auto shift = pixelFormat.getBitsPerComponent() - 8;
    return {value.r >> shift, value.g >> shift, value.b >> shift, 255};
  };

  for (const auto &format : getAllValidFormats())
  {
    EXPECT_EQ(format.getPixelValueTextRendering(scaleRgbToPixelFormatBitDepth(format, black)),
              TextRendering::White);
    EXPECT_EQ(
      format.getPixelValueTextRendering(scaleRgbToPixelFormatBitDepth(format, lowerLuminance)),
      TextRendering::White);
    EXPECT_EQ(
      format.getPixelValueTextRendering(scaleRgbToPixelFormatBitDepth(format, higherLuminance)),
      TextRendering::Black);
    EXPECT_EQ(format.getPixelValueTextRendering(scaleRgbToPixelFormatBitDepth(format, white)),
              TextRendering::Black);

    break;
  }
}

} // namespace video::rgb::test
