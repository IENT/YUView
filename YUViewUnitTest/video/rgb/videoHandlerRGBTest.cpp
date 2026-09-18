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

#include "../VideoHandlerRawTestDataLoader.h"

namespace video::rgb::test
{

using namespace std::string_literals;
using video::test::videoHandlerDataLoadingTest;

namespace
{

constexpr auto EXPECTED_RGB_FORMAT =
  is_Q_OS_WIN ? QImage::Format_ARGB32_Premultiplied : QImage::Format_RGB32;

// clang-format off
const auto TEST_DATA_RGB_8BIT_4x4_ONE = QByteArray::fromHex(
  "000000" "ffffff" "ff0000" "00ff00"
  "0000ff" "800000" "008000" "000080"
  "100000" "001000" "000010" "500000"
  "005000" "000050" "f00000" "00f000"
);

const auto TEST_PIXELS_RGB_8BIT_4x4_ONE = std::array<rgba_t, 16>{
  rgba_t{  0,   0,   0, 255}, rgba_t{255, 255, 255, 255}, rgba_t{255,   0,   0, 255}, rgba_t{  0, 255,   0, 255},
  rgba_t{  0,   0, 255, 255}, rgba_t{128,   0,   0, 255}, rgba_t{  0, 128,   0, 255}, rgba_t{  0,   0, 128, 255},
  rgba_t{ 16,   0,   0, 255}, rgba_t{  0,  16,   0, 255}, rgba_t{  0,   0,  16, 255}, rgba_t{ 80,   0,   0, 255},
  rgba_t{  0,  80,   0, 255}, rgba_t{  0,   0,  80, 255}, rgba_t{240,   0,   0, 255}, rgba_t{  0, 240,   0, 255}
};

const auto TEST_DATA_RGB_8BIT_4x4_TWO = QByteArray::fromHex(
  "100000" "001000" "000010" "500000"
  "005000" "000050" "f00000" "00f000"
  "000000" "ffffff" "ff0000" "00ff00"
  "0000ff" "800000" "008000" "000080"
);

const auto TEST_PIXELS_RGB_8BIT_4x4_DIFFERENCE_ONE_MINUS_TWO = std::array<rgba_t, 16>{
  rgba_t{ -16,   0,    0, 255}, rgba_t{  255,  239,  255, 255}, rgba_t{  255,    0, -16, 255}, rgba_t{ -80,  255,    0, 255},
  rgba_t{   0, -80,  255, 255}, rgba_t{  128,    0,  -80, 255}, rgba_t{ -240,  128,   0, 255}, rgba_t{   0, -240,  128, 255},
  rgba_t{  16,   0,    0, 255}, rgba_t{ -255, -239, -255, 255}, rgba_t{ -255,    0,  16, 255}, rgba_t{  80, -255,    0, 255},
  rgba_t{   0,  80, -255, 255}, rgba_t{ -128,    0,   80, 255}, rgba_t{  240, -128,   0, 255}, rgba_t{   0,  240, -128, 255}
};

const auto TEST_DIFFERENCE_ONE_MINUS_TWO = std::array<rgba_t, 16>{
  rgba_t{112, 128, 128, 255}, rgba_t{255, 255, 255, 255}, rgba_t{255, 128, 112, 255}, rgba_t{ 48, 255, 128, 255},
  rgba_t{128,  48, 255, 255}, rgba_t{255, 128,  48, 255}, rgba_t{  0, 255, 128, 255}, rgba_t{128,   0, 255, 255},
  rgba_t{144, 128, 128, 255}, rgba_t{  0,   0,   0, 255}, rgba_t{  0, 128, 144, 255}, rgba_t{208,   0, 128, 255},
  rgba_t{128, 208,   0, 255}, rgba_t{  0, 128, 208, 255}, rgba_t{255,   0, 128, 255}, rgba_t{128, 255,   0, 255}
};

// RGB565 (little-endian) test data. Each pixel is encoded as
// (R << 11) | (G << 5) | B with 5/6/5 bits, written as two bytes low/high.
// The 16 pixels are designed to exercise positive/negative deltas on each
// channel as well as combined-channel deltas.
const auto TEST_DATA_RGB565_4x4_ONE = QByteArray::fromHex(
  "0000" "ffff" "00f8" "e007"
  "1f00" "1084" "0000" "0000"
  "0000" "8a52" "4529" "ffff"
  "0000" "cf7b" "14a5" "8a52"
);

const auto TEST_PIXELS_RGB565_4x4_ONE = std::array<rgba_t, 16>{
  rgba_t{  0,   0,   0, 255}, rgba_t{ 31,  63,  31, 255}, rgba_t{ 31,   0,   0, 255}, rgba_t{  0,  63,   0, 255},
  rgba_t{  0,   0,  31, 255}, rgba_t{ 16,  32,  16, 255}, rgba_t{  0,   0,   0, 255}, rgba_t{  0,   0,   0, 255},
  rgba_t{  0,   0,   0, 255}, rgba_t{ 10,  20,  10, 255}, rgba_t{  5,  10,   5, 255}, rgba_t{ 31,  63,  31, 255},
  rgba_t{  0,   0,   0, 255}, rgba_t{ 15,  30,  15, 255}, rgba_t{ 20,  40,  20, 255}, rgba_t{ 10,  20,  10, 255}
};

const auto TEST_DATA_RGB565_4x4_TWO = QByteArray::fromHex(
  "0000" "ffff" "0000" "0000"
  "0000" "0000" "00f8" "e007"
  "1f00" "4529" "8a52" "0000"
  "ffff" "cf7b" "8a52" "14a5"
);

const auto TEST_PIXELS_RGB565_4x4_DIFFERENCE_ONE_MINUS_TWO = std::array<rgba_t, 16>{
  rgba_t{   0,   0,   0, 255}, rgba_t{  0,  0,  0, 255}, rgba_t{ 31,   0,  0, 255}, rgba_t{   0,  63,   0, 255},
  rgba_t{   0,   0,  31, 255}, rgba_t{ 16, 32, 16, 255}, rgba_t{-31,   0,  0, 255}, rgba_t{   0, -63,   0, 255},
  rgba_t{   0,   0, -31, 255}, rgba_t{  5, 10,  5, 255}, rgba_t{ -5, -10, -5, 255}, rgba_t{  31,  63,  31, 255},
  rgba_t{ -31, -63, -31, 255}, rgba_t{  0,  0,  0, 255}, rgba_t{ 10,  20, 10, 255}, rgba_t{ -10, -20, -10, 255}
};

const auto TEST_DIFFERENCE_RGB565_ONE_MINUS_TWO = std::array<rgba_t, 16>{
  rgba_t{128, 128, 128, 255}, rgba_t{128, 128, 128, 255}, rgba_t{159, 128, 128, 255}, rgba_t{128, 191, 128, 255},
  rgba_t{128, 128, 159, 255}, rgba_t{144, 160, 144, 255}, rgba_t{ 97, 128, 128, 255}, rgba_t{128,  65, 128, 255},
  rgba_t{128, 128,  97, 255}, rgba_t{133, 138, 133, 255}, rgba_t{123, 118, 123, 255}, rgba_t{159, 191, 159, 255},
  rgba_t{ 97,  65,  97, 255}, rgba_t{128, 128, 128, 255}, rgba_t{138, 148, 138, 255}, rgba_t{118, 108, 118, 255}
};
// clang-format on

void expectDifferenceImageMatchesExpectedValues(const QImage                 &differenceImage,
                                                const std::array<rgba_t, 16> &expectedValues)
{
  for (int y = 0; y < differenceImage.height(); y++)
  {
    for (int x = 0; x < differenceImage.width(); x++)
    {
      const auto pixelValue = differenceImage.pixel(x, y);
      const auto pixelValueRGBA =
        rgba_t{qRed(pixelValue), qGreen(pixelValue), qBlue(pixelValue), qAlpha(pixelValue)};

      const auto expectedPixelValue = expectedValues[y * 4 + x];

      EXPECT_EQ(expectedPixelValue, pixelValueRGBA)
        << "Pixel at position (" << x << ", " << y << ") does not match expected value. Expected: ("
        << expectedPixelValue.r << ", " << expectedPixelValue.g << ", " << expectedPixelValue.b
        << ", " << expectedPixelValue.a << ") but was: (" << qRed(pixelValue) << ", "
        << qGreen(pixelValue) << ", " << qBlue(pixelValue) << ", " << qAlpha(pixelValue) << ")";
    }
  }
}

void expectDifferenceImageToBeZero(const QImage &differenceImage)
{
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
}

void expectThatGetPixelValuesAre(const std::array<rgba_t, 16> &expectedValues,
                                 const videoHandlerRGB        *handler,
                                 const videoHandlerRGB        *handlerForDifference = nullptr)
{
  const auto frameSize = handler->getFrameSize();
  ASSERT_EQ(frameSize, Size(4, 4));
  ASSERT_NE(handler, nullptr);

  for (unsigned y = 0; y < frameSize.height; y++)
  {
    for (unsigned x = 0; x < frameSize.width; x++)
    {
      const auto pixelValues = handler->getPixelValues(QPoint(x, y), 0, handlerForDifference);
      const auto expectedPixelValue = expectedValues[y * frameSize.width + x];

      ASSERT_EQ(pixelValues.size(), 3u)
        << "Expected 3 channel values for pixel at position (" << x << ", " << y << ") but got "
        << pixelValues.size() << " values.";

      EXPECT_EQ(pixelValues[0].first, "R")
        << "First channel should be R but was: " << pixelValues[0].first.toStdString();
      EXPECT_EQ(pixelValues[0].second.toInt(), expectedPixelValue.r)
        << "Red value of pixel at position (" << x << ", " << y
        << ") does not match expected value. Expected: " << expectedPixelValue.r
        << " but was: " << pixelValues[0].second.toInt();

      EXPECT_EQ(pixelValues[1].first, "G")
        << "Second channel should be G but was: " << pixelValues[1].first.toStdString();
      EXPECT_EQ(pixelValues[1].second.toInt(), expectedPixelValue.g)
        << "Green value of pixel at position (" << x << ", " << y
        << ") does not match expected value. Expected: " << expectedPixelValue.g
        << " but was: " << pixelValues[1].second.toInt();

      EXPECT_EQ(pixelValues[2].first, "B")
        << "Third channel should be B but was: " << pixelValues[2].first.toStdString();
      EXPECT_EQ(pixelValues[2].second.toInt(), expectedPixelValue.b)
        << "Blue value of pixel at position (" << x << ", " << y
        << ") does not match expected value. Expected: " << expectedPixelValue.b
        << " but was: " << pixelValues[2].second.toInt();
    }
  }
}

} // namespace

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

TEST(videoHandlerRGBTest,
     testCalculateDifference_8bitRGBPacked_sameInputSignal_shouldReturnEmtpyDifference)
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
  EXPECT_EQ(differenceImage.format(), EXPECTED_RGB_FORMAT);

  expectDifferenceImageToBeZero(differenceImage);

  EXPECT_THAT(differenceInfoList,
              testing::ElementsAre(InfoItem("Difference domain"s, "RGB 8bit"s),
                                   InfoItem("MSE R"s, "0.000000"s),
                                   InfoItem("MSE G"s, "0.000000"s),
                                   InfoItem("MSE B"s, "0.000000"s),
                                   InfoItem("MSE All"s, "0.000000"s)));
}

TEST(videoHandlerRGBTest, testCalculateDifference_8bitRGBPacked_shouldReturnDifference)
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
  dataLoader2.addExpectedLoadingRequests({0, TEST_DATA_RGB_8BIT_4x4_TWO});

  QList<InfoItem> differenceInfoList;
  QImage          differenceImage =
    handler1.calculateDifference(&handler2, 0, 0, differenceInfoList, 1, false);

  EXPECT_FALSE(differenceImage.isNull());
  EXPECT_EQ(differenceImage.size(), QSize(4, 4));
  EXPECT_EQ(differenceImage.format(), EXPECTED_RGB_FORMAT);

  expectDifferenceImageMatchesExpectedValues(differenceImage, TEST_DIFFERENCE_ONE_MINUS_TWO);

  EXPECT_THAT(differenceInfoList,
              testing::ElementsAre(InfoItem("Difference domain"s, "RGB 8bit"s),
                                   InfoItem("MSE R"s, "1646.015625"s),
                                   InfoItem("MSE G"s, "1582.265625"s),
                                   InfoItem("MSE B"s, "1196.015625"s),
                                   InfoItem("MSE All"s, "4424.296875"s)));
}

TEST(videoHandlerRGBTest, testGetPixelValues_8bitRGBPacked_shouldReturnCorrectValues)
{
  videoHandlerRGB handler;
  handler.setFrameSize({4, 4});
  handler.setRGBPixelFormatByName("RGB 8bit");
  videoHandlerDataLoadingTest dataLoader(&handler);
  dataLoader.addExpectedLoadingRequests({0, TEST_DATA_RGB_8BIT_4x4_ONE});

  EXPECT_TRUE(handler.getPixelValues(QPoint(0, 0), 0, nullptr).empty())
    << "If no data was loaded yet, an empty list should be returned.";

  handler.loadFrame(0);

  expectThatGetPixelValuesAre(TEST_PIXELS_RGB_8BIT_4x4_ONE, &handler);

  EXPECT_TRUE(handler.getPixelValues(QPoint(5, 0), 0, nullptr).empty())
    << "For an out-of-bounds pixel position, an empty list should be returned.";
  EXPECT_TRUE(handler.getPixelValues(QPoint(-5, 0), 0, nullptr).empty())
    << "For negative pixel position, an empty list should be returned.";
}

TEST(videoHandlerRGBTest, testGetPixelValuesDifference_8bitRGBPacked_shouldReturnCorrectValues)
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
  dataLoader2.addExpectedLoadingRequests({0, TEST_DATA_RGB_8BIT_4x4_TWO});

  EXPECT_TRUE(handler1.getPixelValues(QPoint(0, 0), 0, &handler2).empty())
    << "If no data was loaded yet, an empty list should be returned.";
  handler1.loadFrame(0);

  EXPECT_TRUE(handler1.getPixelValues(QPoint(0, 0), 0, &handler2).empty())
    << "If no data for both handlers was loaded yet, an empty list should be returned.";
  handler2.loadFrame(0);

  expectThatGetPixelValuesAre(
    TEST_PIXELS_RGB_8BIT_4x4_DIFFERENCE_ONE_MINUS_TWO, &handler1, &handler2);

  EXPECT_TRUE(handler1.getPixelValues(QPoint(5, 0), 0, &handler2).empty())
    << "For an out-of-bounds pixel position, an empty list should be returned.";
  EXPECT_TRUE(handler1.getPixelValues(QPoint(-5, 0), 0, &handler2).empty())
    << "For negative pixel position, an empty list should be returned.";
}

TEST(videoHandlerRGBTest,
     testCalculateDifference_RGB565Packed_sameInputSignal_shouldReturnEmtpyDifference)
{
  videoHandlerRGB handler1;
  handler1.setFrameSize({4, 4});
  handler1.setRGBPixelFormatByName("RGB565");
  videoHandlerDataLoadingTest dataLoader(&handler1);
  dataLoader.addExpectedLoadingRequests({0, TEST_DATA_RGB565_4x4_ONE});

  videoHandlerRGB handler2;
  handler2.setFrameSize({4, 4});
  handler2.setRGBPixelFormatByName("RGB565");
  videoHandlerDataLoadingTest dataLoader2(&handler2);
  dataLoader2.addExpectedLoadingRequests({0, TEST_DATA_RGB565_4x4_ONE});

  QList<InfoItem> differenceInfoList;
  QImage          differenceImage =
    handler1.calculateDifference(&handler2, 0, 0, differenceInfoList, 1, false);

  EXPECT_FALSE(differenceImage.isNull());
  EXPECT_EQ(differenceImage.size(), QSize(4, 4));
  EXPECT_EQ(differenceImage.format(), EXPECTED_RGB_FORMAT);

  expectDifferenceImageToBeZero(differenceImage);

  EXPECT_THAT(differenceInfoList,
              testing::ElementsAre(InfoItem("Difference domain"s, "RGB565"s),
                                   InfoItem("MSE R"s, "0.000000"s),
                                   InfoItem("MSE G"s, "0.000000"s),
                                   InfoItem("MSE B"s, "0.000000"s),
                                   InfoItem("MSE All"s, "0.000000"s)));
}

TEST(videoHandlerRGBTest, testCalculateDifference_RGB565Packed_shouldReturnDifference)
{
  videoHandlerRGB handler1;
  handler1.setFrameSize({4, 4});
  handler1.setRGBPixelFormatByName("RGB565");
  videoHandlerDataLoadingTest dataLoader(&handler1);
  dataLoader.addExpectedLoadingRequests({0, TEST_DATA_RGB565_4x4_ONE});

  videoHandlerRGB handler2;
  handler2.setFrameSize({4, 4});
  handler2.setRGBPixelFormatByName("RGB565");
  videoHandlerDataLoadingTest dataLoader2(&handler2);
  dataLoader2.addExpectedLoadingRequests({0, TEST_DATA_RGB565_4x4_TWO});

  QList<InfoItem> differenceInfoList;
  QImage          differenceImage =
    handler1.calculateDifference(&handler2, 0, 0, differenceInfoList, 1, false);

  EXPECT_FALSE(differenceImage.isNull());
  EXPECT_EQ(differenceImage.size(), QSize(4, 4));
  EXPECT_EQ(differenceImage.format(), EXPECTED_RGB_FORMAT);

  expectDifferenceImageMatchesExpectedValues(differenceImage, TEST_DIFFERENCE_RGB565_ONE_MINUS_TWO);

  EXPECT_THAT(differenceInfoList,
              testing::ElementsAre(InfoItem("Difference domain"s, "RGB565"s),
                                   InfoItem("MSE R"s, "16.992188"s),
                                   InfoItem("MSE G"s, "69.921875"s),
                                   InfoItem("MSE B"s, "16.992188"s),
                                   InfoItem("MSE All"s, "103.906250"s)));
}

TEST(videoHandlerRGBTest, testGetPixelValues_RGB565Packed_shouldReturnCorrectValues)
{
  videoHandlerRGB handler;
  handler.setFrameSize({4, 4});
  handler.setRGBPixelFormatByName("RGB565");
  videoHandlerDataLoadingTest dataLoader(&handler);
  dataLoader.addExpectedLoadingRequests({0, TEST_DATA_RGB565_4x4_ONE});

  EXPECT_TRUE(handler.getPixelValues(QPoint(0, 0), 0, nullptr).empty())
    << "If no data was loaded yet, an empty list should be returned.";

  handler.loadFrame(0);

  expectThatGetPixelValuesAre(TEST_PIXELS_RGB565_4x4_ONE, &handler);

  EXPECT_TRUE(handler.getPixelValues(QPoint(5, 0), 0, nullptr).empty())
    << "For an out-of-bounds pixel position, an empty list should be returned.";
  EXPECT_TRUE(handler.getPixelValues(QPoint(-5, 0), 0, nullptr).empty())
    << "For negative pixel position, an empty list should be returned.";
}

TEST(videoHandlerRGBTest, testGetPixelValuesDifference_RGB565Packed_shouldReturnCorrectValues)
{
  videoHandlerRGB handler1;
  handler1.setFrameSize({4, 4});
  handler1.setRGBPixelFormatByName("RGB565");
  videoHandlerDataLoadingTest dataLoader(&handler1);
  dataLoader.addExpectedLoadingRequests({0, TEST_DATA_RGB565_4x4_ONE});

  videoHandlerRGB handler2;
  handler2.setFrameSize({4, 4});
  handler2.setRGBPixelFormatByName("RGB565");
  videoHandlerDataLoadingTest dataLoader2(&handler2);
  dataLoader2.addExpectedLoadingRequests({0, TEST_DATA_RGB565_4x4_TWO});

  EXPECT_TRUE(handler1.getPixelValues(QPoint(0, 0), 0, &handler2).empty())
    << "If no data was loaded yet, an empty list should be returned.";
  handler1.loadFrame(0);

  EXPECT_TRUE(handler1.getPixelValues(QPoint(0, 0), 0, &handler2).empty())
    << "If no data for both handlers was loaded yet, an empty list should be returned.";
  handler2.loadFrame(0);

  expectThatGetPixelValuesAre(
    TEST_PIXELS_RGB565_4x4_DIFFERENCE_ONE_MINUS_TWO, &handler1, &handler2);

  EXPECT_TRUE(handler1.getPixelValues(QPoint(5, 0), 0, &handler2).empty())
    << "For an out-of-bounds pixel position, an empty list should be returned.";
  EXPECT_TRUE(handler1.getPixelValues(QPoint(-5, 0), 0, &handler2).empty())
    << "For negative pixel position, an empty list should be returned.";
}

} // namespace video::rgb::test
