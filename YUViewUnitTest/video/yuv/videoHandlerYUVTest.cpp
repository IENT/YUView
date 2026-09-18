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
#include <video/yuv/videoHandlerYUV.h>

#include "../VideoHandlerRawTestDataLoader.h"

#include "PixelFormatYUVHelper.h"

namespace video::yuv::test
{

using video::test::videoHandlerDataLoadingTest;

namespace
{

// 4x4 YUV 4:4:4 planar test data encoded as Y plane, U plane, V plane.
// The values are chosen so that the raw YUV samples can be asserted directly.
const auto TEST_DATA_YUV_444_4x4 = QByteArray::fromHex("000102030405060708090a0b0c0d0e0f"
                                                       "101112131415161718191a1b1c1d1e1f"
                                                       "202122232425262728292a2b2c2d2e2f");

void expectDifferenceImageToBeZero(const QImage &differenceImage)
{
  ASSERT_EQ(differenceImage.size(), QSize(4, 4));

  for (int y = 0; y < differenceImage.height(); y++)
  {
    for (int x = 0; x < differenceImage.width(); x++)
    {
      const auto pixelValue = differenceImage.pixel(x, y);
      EXPECT_EQ(qRed(pixelValue), 130);
      EXPECT_EQ(qGreen(pixelValue), 130);
      EXPECT_EQ(qBlue(pixelValue), 130);
    }
  }
}

void expectPixelValues(const QStringPairList &values,
                       const int              expectedY,
                       const int              expectedU,
                       const int              expectedV)
{
  ASSERT_EQ(values.size(), 3u);
  EXPECT_EQ(values[0].first, "Y");
  EXPECT_EQ(values[0].second.toInt(), expectedY);
  EXPECT_EQ(values[1].first, "U");
  EXPECT_EQ(values[1].second.toInt(), expectedU);
  EXPECT_EQ(values[2].first, "V");
  EXPECT_EQ(values[2].second.toInt(), expectedV);
}

} // namespace

TEST(videoHandlerYUVTest, defaultConstructor_shouldHaveInvalidFormat)
{
  videoHandlerYUV handler;

  EXPECT_FALSE(handler.isFormatValid());
  EXPECT_EQ(handler.getCachingFrameSize(), 0u);
  EXPECT_EQ(handler.getBytesPerFrame(), 0);
  EXPECT_TRUE(handler.getPixelValues(QPoint(0, 0), 0).isEmpty());
  EXPECT_FALSE(handler.getFormatAsString().has_value());
  EXPECT_EQ(handler.getRawPixelFormatYUVName(), QString("YUV 4:2:0 8-bit"));
}

TEST(videoHandlerYUVTest, settingFormat_shouldReturnExpectedFormatAsStringAndBeLoadableFromString)
{
  videoHandlerYUV handler;
  handler.setFrameSize({4, 4});
  videoHandlerYUV loadedHandler;

  for (const auto format : getAllPixelFormats())
  {
    handler.setPixelFormatYUV(format);

    const auto formatAsString = handler.getFormatAsString();
    ASSERT_TRUE(formatAsString.has_value());

    std::string expectedFormatAsString = "4;4;YUV;" + format.getName();
    EXPECT_EQ(*formatAsString, expectedFormatAsString);

    EXPECT_TRUE(loadedHandler.setFormatFromString(*formatAsString));
    EXPECT_EQ(loadedHandler.getFrameSize(), Size(4, 4));
    EXPECT_EQ(loadedHandler.getRawPixelFormatYUVName(), QString::fromStdString(format.getName()));
  }
}

TEST(videoHandlerYUVTest, testLoadFrame_444Planar_shouldReturnExpectedPixelValues)
{
  videoHandlerYUV handler;
  handler.setFrameSize({4, 4});
  handler.setPixelFormatYUV(PixelFormatYUV(Subsampling::YUV_444, 8, PlaneOrder::YUV));

  videoHandlerDataLoadingTest dataLoader(&handler);
  dataLoader.addExpectedLoadingRequests({0, TEST_DATA_YUV_444_4x4});

  handler.loadFrame(0, false);

  const auto valuesOrigin = handler.getPixelValues(QPoint(0, 0), 0);
  expectPixelValues(valuesOrigin, 0x00, 0x10, 0x20);

  const auto valuesCenter = handler.getPixelValues(QPoint(2, 2), 0);
  expectPixelValues(valuesCenter, 0x0a, 0x1a, 0x2a);
}

TEST(videoHandlerYUVTest, testCalculateDifference_444Planar_sameInput_shouldReturnZeroDifference)
{
  videoHandlerYUV handler1;
  handler1.setFrameSize({4, 4});
  handler1.setPixelFormatYUV(PixelFormatYUV(Subsampling::YUV_444, 8, PlaneOrder::YUV));
  videoHandlerDataLoadingTest dataLoader1(&handler1);
  dataLoader1.addExpectedLoadingRequests({0, TEST_DATA_YUV_444_4x4});

  videoHandlerYUV handler2;
  handler2.setFrameSize({4, 4});
  handler2.setPixelFormatYUV(PixelFormatYUV(Subsampling::YUV_444, 8, PlaneOrder::YUV));
  videoHandlerDataLoadingTest dataLoader2(&handler2);
  dataLoader2.addExpectedLoadingRequests({0, TEST_DATA_YUV_444_4x4});

  QList<InfoItem> differenceInfoList;
  QImage          differenceImage =
    handler1.calculateDifference(&handler2, 0, 0, differenceInfoList, 1, false);

  EXPECT_FALSE(differenceImage.isNull());
  EXPECT_EQ(differenceImage.size(), QSize(4, 4));
  expectDifferenceImageToBeZero(differenceImage);
  EXPECT_FALSE(differenceInfoList.isEmpty());
}

} // namespace video::yuv::test
