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

#include <video/PixelFormat.h>
#include <video/rgb/ConversionRGB.h>
#include <video/rgb/PixelFormatRGB.h>

#include "CreateTestData.h"

namespace video::rgb::test
{

namespace
{

void testGetPixelValueFromBuffer(const QByteArray     &sourceBuffer,
                                 const PixelFormatRGB &srcPixelFormat)
{
  const auto bitDepth = srcPixelFormat.getBitsPerSample();
  const auto shift    = 12 - bitDepth;

  int testValueIndex = 0;
  for (int y : {0, 1, 2, 3})
  {
    for (int x : {0, 1, 2, 3})
    {
      const QPoint pixelPos(x, y);
      const auto   actualValue =
          getPixelValueFromBuffer(sourceBuffer, srcPixelFormat, TEST_FRAME_SIZE, pixelPos);

      const auto testValue     = TEST_VALUES_12BIT[testValueIndex++];
      auto       expectedValue = rgba_t(
          {testValue.R >> shift, testValue.G >> shift, testValue.B >> shift, testValue.A >> shift});
      if (!srcPixelFormat.hasAlpha())
        expectedValue.A = 0;

      if (actualValue != expectedValue)
        throw std::runtime_error("Error checking pixel [" + std::to_string(x) + "," +
                                 std::to_string(y) + " format " + srcPixelFormat.getName());
    }
  }
}

} // namespace

TEST(GetPixelValueTest, TestGetPixelValueFromBuffer)
{
  for (const auto endianness : EndianessMapper.getValues())
  {
    for (auto bitDepth : {8, 10, 12})
    {
      for (const auto &alphaMode : AlphaModeMapper.getValues())
      {
        for (const auto &dataLayout : DataLayoutMapper.getValues())
        {
          for (const auto &channelOrder : ChannelOrderMapper.getValues())
          {
            const PixelFormatRGB pixelFormat(
                bitDepth, dataLayout, channelOrder, alphaMode, endianness);
            const auto data = createRawRGBData(pixelFormat);

            EXPECT_NO_THROW(testGetPixelValueFromBuffer(data, pixelFormat))
                << "Failed for pixel format " << pixelFormat.getName();
          }
        }
      }
    }
  }
}

} // namespace video::rgb::test
