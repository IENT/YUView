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

namespace video::rgb::test
{

TEST(ConversionFunctionsTest,
     TestCalculatePointersToStartOfComponents_InvalidPixelFormat_ShouldThrow)
{
  const auto pixelFormat = PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB);
  auto       data        = createRawRGBData(pixelFormat);
  EXPECT_THROW(calculatePointersToStartOfComponents<uint8_t>(data, {128, 128}, {}),
               std::invalid_argument);
}

TEST(ConversionFunctionsTest, TestCalculatePointersToStartOfComponents_InvalidFrameSize_ShouldThrow)
{
  const auto pixelFormat = PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB);
  auto       data        = createRawRGBData(pixelFormat);
  EXPECT_THROW(calculatePointersToStartOfComponents<uint8_t>(data, {128, 0}, pixelFormat),
               std::invalid_argument);
}

TEST(ConversionFunctionsTest, TestCalculatePointersToStartOfComponents_NotEnoughData_ShouldThrow)
{
  QByteArray data;
  EXPECT_THROW(calculatePointersToStartOfComponents<uint8_t>(
                 data, TEST_FRAME_SIZE, PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB)),
               std::invalid_argument);
}

TEST(ConversionFunctionsTest, TestCalculatePointersToStartOfComponents_OffsetForPacked8Bit)
{
  const auto pixelFormat = PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB);
  auto       data        = createRawRGBData(pixelFormat);

  const auto offsets =
    calculatePointersToStartOfComponents<uint8_t>(data, TEST_FRAME_SIZE, pixelFormat);

  const auto rawDataPointer = reinterpret_cast<uint8_t *>(data.data());
  EXPECT_EQ(offsets.r, rawDataPointer + 0);
  EXPECT_EQ(offsets.g, rawDataPointer + 1);
  EXPECT_EQ(offsets.b, rawDataPointer + 2);
}

// More tests

} // namespace video::rgb::test
