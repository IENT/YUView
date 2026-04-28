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

#include <video/rgb/ConversionFunctions.h>

#include "CreateTestData.h"
#include "video/PixelFormat.h"
#include "video/rgb/PixelFormatRGB.h"

namespace video::rgb::test
{

namespace
{

struct Offsets
{
  int r{};
  int g{};
  int b{};
};

using TestParameters = std::tuple<int, DataLayout, ChannelOrder>;

class ConversionFunctionsTest : public TestWithParam<TestParameters>
{
};

TEST_F(ConversionFunctionsTest,
       TestCalculatePointersToStartOfComponents_InvalidPixelFormat_ShouldThrow)
{
  const auto pixelFormat = PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB);
  auto       data        = createRawRGBData(pixelFormat, TEST_VALUES_12BIT, 12);
  EXPECT_THROW(calculatePointersToStartOfComponents<uint8_t>(data, {128, 128}, {}),
               std::invalid_argument);
}

TEST_F(ConversionFunctionsTest,
       TestCalculatePointersToStartOfComponents_InvalidFrameSize_ShouldThrow)
{
  const auto pixelFormat = PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB);
  auto       data        = createRawRGBData(pixelFormat, TEST_VALUES_12BIT, 12);
  EXPECT_THROW(calculatePointersToStartOfComponents<uint8_t>(data, {128, 0}, pixelFormat),
               std::invalid_argument);
}

TEST_F(ConversionFunctionsTest, TestCalculatePointersToStartOfComponents_NotEnoughData_ShouldThrow)
{
  QByteArray data;
  EXPECT_THROW(calculatePointersToStartOfComponents<uint8_t>(
                 data, TEST_FRAME_SIZE, PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB)),
               std::invalid_argument);
}

std::string getTestName(const testing::TestParamInfo<TestParameters> &testParametersInfo)
{
  const auto [bitsPerPixel, dataLayout, channelOrder] = testParametersInfo.param;
  const auto pixelFormat = PixelFormatRGB(bitsPerPixel, dataLayout, channelOrder);

  return "TestCalculatePointersToStartOfComponents_PixelFormat" +
         yuviewTest::replaceNonSupportedCharacters(*pixelFormat.getName()) +
         "_shouldReturnCorrectOffsets";
}

TEST_P(ConversionFunctionsTest, TestCalculatePointersToStartOfComponents)
{
  const auto [bitsPerPixel, dataLayout, channelOrder] = GetParam();
  const auto pixelFormat = PixelFormatRGB(bitsPerPixel, dataLayout, channelOrder);

  auto data = createRawRGBData(pixelFormat, TEST_VALUES_12BIT, 12);

  std::map<ChannelOrder, Offsets> expectedOffsetsMap = {{ChannelOrder::RGB, {0, 1, 2}},
                                                        {ChannelOrder::RBG, {0, 2, 1}},
                                                        {ChannelOrder::GRB, {1, 0, 2}},
                                                        {ChannelOrder::GBR, {2, 0, 1}},
                                                        {ChannelOrder::BRG, {1, 2, 0}},
                                                        {ChannelOrder::BGR, {2, 1, 0}}};
  auto                            expectedOffsets    = expectedOffsetsMap[channelOrder];
  if (dataLayout == DataLayout::Planar)
  {
    expectedOffsets = Offsets({expectedOffsets.r * TEST_FRAME_NR_VALUES,
                               expectedOffsets.g * TEST_FRAME_NR_VALUES,
                               expectedOffsets.b * TEST_FRAME_NR_VALUES});
  }

  if (bitsPerPixel == 8)
  {
    const auto offsets =
      calculatePointersToStartOfComponents<uint8_t>(data, TEST_FRAME_SIZE, pixelFormat);

    const auto rawDataPointer = reinterpret_cast<uint8_t *>(data.data());
    EXPECT_EQ(offsets.r, rawDataPointer + expectedOffsets.r);
    EXPECT_EQ(offsets.g, rawDataPointer + expectedOffsets.g);
    EXPECT_EQ(offsets.b, rawDataPointer + expectedOffsets.b);
  }
  else
  {
    const auto offsets =
      calculatePointersToStartOfComponents<uint16_t>(data, TEST_FRAME_SIZE, pixelFormat);

    const auto rawDataPointer = reinterpret_cast<uint16_t *>(data.data());
    EXPECT_EQ(offsets.r, rawDataPointer + expectedOffsets.r);
    EXPECT_EQ(offsets.g, rawDataPointer + expectedOffsets.g);
    EXPECT_EQ(offsets.b, rawDataPointer + expectedOffsets.b);
  }
}

INSTANTIATE_TEST_SUITE_P(VideoRGBTest,
                         ConversionFunctionsTest,
                         Combine(Values(8, 9, 10, 12, 16),
                                 Values(DataLayout::Packed, DataLayout::Planar),
                                 ValuesIn(ChannelOrderMapper.getValues())),
                         getTestName);

} // namespace

} // namespace video::rgb::test
