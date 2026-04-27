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
#include <video/rgb/videoHandlerRGB.h>

#include "VideoHandlerRawTestDataLoader.h"

namespace video::rgb::test
{

using namespace std::string_literals;

// clang-format off
const auto TEST_DATA_RGB_8BIT_4x4_ONE = QByteArray::fromHex(
  "000000" "ffffff" "ff0000" "00ff00"
  "0000ff" "800000" "008000" "000080"
  "100000" "001000" "000010" "500000"
  "005000" "000050" "f00000" "00f000"
);

const auto TEST_DATA_RGB_8BIT_4x4_TWO = QByteArray::fromHex(
  "100000" "001000" "000010" "500000"
  "005000" "000050" "f00000" "00f000"
  "000000" "ffffff" "ff0000" "00ff00"
  "0000ff" "800000" "008000" "000080"
);
// clang-format on

TEST(videoHandlerRGBTest, testDefaultConstructor)
{
  videoHandlerRGB handler;

  EXPECT_FALSE(handler.isFormatValid());
  EXPECT_EQ(handler.getCachingFrameSize(), 0u);
  EXPECT_TRUE(handler.getPixelValues(QPoint(0, 0), 0, nullptr).isEmpty());
  EXPECT_EQ(handler.getBytesPerFrame(), 0);
  EXPECT_FALSE(handler.getFormatAsString())
    << "Format should be Invalid but was: " << handler.getFormatAsString().value();

  EXPECT_TRUE(handler.getRawRGBPixelFormatName());
  EXPECT_EQ(handler.getRawRGBPixelFormatName(), "RGB 8bit");
}

TEST(videoHandlerRGBTest, testCalculateDifference_sameInputSignal_shouldReturnEmtpyDifference)
{
  videoHandlerRGB handler1;
  handler1.setFrameSize({4, 4});
  handler1.setRGBPixelFormatByName("RGB 8bit");
  videoHandlerDataLoadingTest dataLoader(&handler1);
  dataLoader.addExpectedLoadingRequests({0, TEST_DATA_RGB_8BIT_4x4_ONE});

  videoHandlerRGB handler2;
  handler2.setFrameSize({4, 4});
  handler2.setRGBPixelFormatByName("RGB 8bit");
  videoHandlerDataLoadingTest dataLoader2(&handler2);
  dataLoader2.addExpectedLoadingRequests({0, TEST_DATA_RGB_8BIT_4x4_ONE});

  QList<InfoItem> differenceInfoList;
  QImage          differenceImage =
    handler1.calculateDifference(&handler2, 0, 0, differenceInfoList, 1, false);

  EXPECT_FALSE(differenceImage.isNull());
  EXPECT_EQ(differenceImage.size(), QSize(4, 4));
  EXPECT_EQ(differenceImage.format(), QImage::Format_RGB32);

  for (int y = 0; y < differenceImage.height(); y++)
  {
    for (int x = 0; x < differenceImage.width(); x++)
    {
      const auto pixelValue = differenceImage.pixel(x, y);
      EXPECT_EQ(qRed(pixelValue), 128);
      EXPECT_EQ(qGreen(pixelValue), 128);
      EXPECT_EQ(qBlue(pixelValue), 128);
    }
  }

  EXPECT_THAT(differenceInfoList,
              testing::ElementsAre(InfoItem("Difference domain"s, "RGB 8bit"s),
                                   InfoItem("MSE R"s, "0.000000"s),
                                   InfoItem("MSE G"s, "0.000000"s),
                                   InfoItem("MSE B"s, "0.000000"s),
                                   InfoItem("MSE All"s, "0.000000"s)));
}

} // namespace video::rgb::test
