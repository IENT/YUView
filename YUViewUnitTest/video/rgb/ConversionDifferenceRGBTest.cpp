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

#include "common/Typedef.h"

#include "video/rgb/CreateTestData.h"
#include <common/FunctionsGui.h>
#include <video/rgb/ConversionDifferenceRGB.h>

#include <common/Testing.h>

#include <random>
#include <stdexcept>

namespace video::rgb::test
{

namespace
{

constexpr Size TEST_FRAME_SIZE    = {4, 4};
constexpr auto NR_PIXELS_IN_FRAME = TEST_FRAME_SIZE.width * TEST_FRAME_SIZE.height;

struct TestParameters
{
  using ParameterTuple = std::tuple<PixelFormatRGB, int, bool>;

  TestParameters(const ParameterTuple &params)
      : pixelFormat(std::get<0>(params)), amplificationFactor(std::get<1>(params)),
        markDifference(std::get<2>(params))
  {
  }

  PixelFormatRGB pixelFormat{};
  int            amplificationFactor{};
  bool           markDifference{};
};

class ConversionDifferenceRGBTest : public TestWithParam<TestParameters>
{
};

class ConversionDifferenceRGBTestPredefinedPixelFormat : public TestWithParam<PredefinedPixelFormat>
{
};

using FrameAandB = std::pair<std::vector<rgba_t>, std::vector<rgba_t>>;
FrameAandB createTestFrameData(const int bitDepth)
{
  std::vector<rgba_t> testValuesA;
  std::vector<rgba_t> testValuesB;

  const auto maxValue = (1 << bitDepth) - 1;
  const auto midValue = (1 << (bitDepth - 1));

  // Add some special values that we definitely want to test
  testValuesA.push_back(rgba_t({0, maxValue, maxValue}));
  testValuesB.push_back(rgba_t({maxValue, 0, maxValue}));
  testValuesA.push_back(rgba_t({0, midValue, 0}));
  testValuesB.push_back(rgba_t({0, 0, midValue}));
  testValuesA.push_back(rgba_t({0, 0, 0}));
  testValuesB.push_back(rgba_t({midValue, midValue - 1, midValue + 1}));
  testValuesA.push_back(rgba_t({midValue, 44, 129}));
  testValuesB.push_back(rgba_t({maxValue, maxValue, maxValue}));

  // The rest of the values will be random
  std::random_device                                       randomDevice;
  std::mt19937                                             randomNumberGenerator(randomDevice());
  std::uniform_int_distribution<std::mt19937::result_type> distribution(0, maxValue);

  constexpr auto NR_REMAINING_PIXELS = NR_PIXELS_IN_FRAME - 4;
  for (unsigned int i = 0; i < NR_REMAINING_PIXELS; ++i)
  {
    testValuesA.push_back(rgba_t({static_cast<int>(distribution(randomNumberGenerator)),
                                  static_cast<int>(distribution(randomNumberGenerator)),
                                  static_cast<int>(distribution(randomNumberGenerator))}));
    testValuesB.push_back(rgba_t({static_cast<int>(distribution(randomNumberGenerator)),
                                  static_cast<int>(distribution(randomNumberGenerator)),
                                  static_cast<int>(distribution(randomNumberGenerator))}));
  }

  return {testValuesA, testValuesB};
}

FrameAandB createTestFrameDataRGB565()
{
  std::vector<rgba_t> testValuesA;
  std::vector<rgba_t> testValuesB;

  const auto maxValueRB = (1 << 5) - 1;
  const auto maxValueG  = (1 << 6) - 1;
  const auto midValueRB = (1 << (5 - 1));
  const auto midValueG  = (1 << (6 - 1));

  // Add some special values that we definitely want to test
  testValuesA.push_back(rgba_t({0, 0, 0}));
  testValuesB.push_back(rgba_t({maxValueRB, maxValueG, maxValueRB}));
  testValuesA.push_back(rgba_t({maxValueRB, maxValueG, maxValueRB}));
  testValuesB.push_back(rgba_t({0, 0, 0}));
  testValuesA.push_back(rgba_t({maxValueRB, maxValueG, maxValueRB}));
  testValuesB.push_back(rgba_t({maxValueRB, maxValueG, maxValueRB}));
  testValuesA.push_back(rgba_t({0, 0, 0}));
  testValuesB.push_back(rgba_t({0, 0, 0}));

  testValuesA.push_back(rgba_t({midValueRB, midValueG, midValueRB}));
  testValuesB.push_back(rgba_t({0, 0, 0}));
  testValuesA.push_back(rgba_t({0, 0, 0}));
  testValuesB.push_back(rgba_t({midValueRB, midValueG, midValueRB}));
  testValuesA.push_back(rgba_t({0, 0, 0}));
  testValuesB.push_back(rgba_t({midValueRB - 1, midValueG - 1, midValueRB - 1}));
  testValuesA.push_back(rgba_t({0, 0, 0}));
  testValuesB.push_back(rgba_t({midValueRB + 1, midValueG + 1, midValueRB + 1}));

  testValuesA.push_back(rgba_t({midValueRB, midValueG, midValueRB}));
  testValuesB.push_back(rgba_t({maxValueRB, maxValueG, maxValueRB}));
  testValuesA.push_back(rgba_t({maxValueRB, maxValueG, maxValueRB}));
  testValuesB.push_back(rgba_t({midValueRB, midValueG, midValueRB}));

  // The rest of the values will be random
  std::random_device                                       randomDevice;
  std::mt19937                                             randomNumberGenerator(randomDevice());
  std::uniform_int_distribution<std::mt19937::result_type> distributionRB(0, maxValueRB);
  std::uniform_int_distribution<std::mt19937::result_type> distributionG(0, maxValueG);

  constexpr auto NR_REMAINING_PIXELS = NR_PIXELS_IN_FRAME - 10;
  for (unsigned int i = 0; i < NR_REMAINING_PIXELS; ++i)
  {
    testValuesA.push_back(rgba_t({static_cast<int>(distributionRB(randomNumberGenerator)),
                                  static_cast<int>(distributionG(randomNumberGenerator)),
                                  static_cast<int>(distributionRB(randomNumberGenerator))}));
    testValuesB.push_back(rgba_t({static_cast<int>(distributionRB(randomNumberGenerator)),
                                  static_cast<int>(distributionG(randomNumberGenerator)),
                                  static_cast<int>(distributionRB(randomNumberGenerator))}));
  }

  return {testValuesA, testValuesB};
}

using ExpectedImageAndMse = std::pair<QImage, MSE>;
ExpectedImageAndMse generateExpectedImageAndMse(const FrameAandB &testFrames,
                                                const int         amplificationFactor,
                                                bool              markDifference)
{
  QImage image(QSize(TEST_FRAME_SIZE.width, TEST_FRAME_SIZE.height),
               functionsGui::platformImageFormat(false));
  SSE    sse;

  const auto nrValues = testFrames.first.size();
  for (size_t i = 0; i < nrValues; ++i)
  {
    const auto &pixelA = testFrames.first.at(i);
    const auto &pixelB = testFrames.second.at(i);

    const auto diff = pixelA - pixelB;

    sse.addSample(diff);

    rgba_t outputPixel;
    if (markDifference)
      outputPixel = {
        .r = diff.r != 0 ? 255 : 0, .g = diff.g != 0 ? 255 : 0, .b = diff.b != 0 ? 255 : 0};
    else
      outputPixel = {.r = functions::clip(128 + diff.r * amplificationFactor, 0, 255),
                     .g = functions::clip(128 + diff.g * amplificationFactor, 0, 255),
                     .b = functions::clip(128 + diff.b * amplificationFactor, 0, 255)};

    const auto x = i % TEST_FRAME_SIZE.width;
    const auto y = i / TEST_FRAME_SIZE.width;
    image.setPixel(x, y, qRgb(outputPixel.r, outputPixel.g, outputPixel.b));
  }

  return {image, sse.getMSE()};
}

using GenerationResult = std::tuple<QByteArray, QByteArray, QImage, MSE>;
GenerationResult generateRawDataFramesExpectedResultAndMse(const PixelFormatRGB &pixelFormat,
                                                           const int  amplificationFactor,
                                                           const bool markDifference)
{
  GenerationResult result;

  const auto bitDepth = pixelFormat.getBitsPerComponent();

  FrameAandB testFrames;
  if (pixelFormat.getPredefinedPixelFormat() == PredefinedPixelFormat::RGB565)
    testFrames = createTestFrameDataRGB565();
  else if (pixelFormat.getPredefinedPixelFormat())
    throw std::logic_error("Support for predefined pixel format not implemented.");
  else
    testFrames = createTestFrameData(bitDepth);

  std::get<0>(result) = createRawRGBData(pixelFormat, testFrames.first, bitDepth);
  std::get<1>(result) = createRawRGBData(pixelFormat, testFrames.second, bitDepth);
  std::tie(std::get<2>(result), std::get<3>(result)) =
    generateExpectedImageAndMse(testFrames, amplificationFactor, markDifference);

  return result;
}

TEST_P(ConversionDifferenceRGBTest, testCalculationOfDifferenceAndMSE)
{
  const auto &param = GetParam();

  const auto [dataFrameA, dataFrameB, expectedImage, expectedMse] =
    generateRawDataFramesExpectedResultAndMse(
      param.pixelFormat, param.amplificationFactor, param.markDifference);

  auto [outputImage, mse] = calculateDifferenceAndMSE({dataFrameA, TEST_FRAME_SIZE},
                                                      {dataFrameB, TEST_FRAME_SIZE},
                                                      param.pixelFormat,
                                                      param.amplificationFactor,
                                                      param.markDifference);

  for (unsigned int y = 0; y < TEST_FRAME_SIZE.height; ++y)
    for (unsigned int x = 0; x < TEST_FRAME_SIZE.width; ++x)
    {
      const auto expectedPixel = expectedImage.pixel(x, y);
      const auto actualPixel   = outputImage.pixel(x, y);
      EXPECT_EQ(qRed(actualPixel), qRed(expectedPixel)) << " at position " << x << "," << y;
      EXPECT_EQ(qGreen(actualPixel), qGreen(expectedPixel)) << " at position " << x << "," << y;
      EXPECT_EQ(qBlue(actualPixel), qBlue(expectedPixel)) << " at position " << x << "," << y;
    }

  EXPECT_EQ(mse, expectedMse);
}

std::string getName(const testing::TestParamInfo<ConversionDifferenceRGBTest::ParamType> &info)
{
  return yuviewTest::replaceNonSupportedCharacters(*info.param.pixelFormat.getName()) +
         "_Amplification" + std::to_string(info.param.amplificationFactor) +
         (info.param.markDifference ? "_MarkDiff" : "");
}

INSTANTIATE_TEST_SUITE_P(VideoRGBTest,
                         ConversionDifferenceRGBTest,
                         ConvertGenerator<TestParameters::ParameterTuple>(Combine(
                           ValuesIn(createTestSetOfPixelFormatRGB()), Values(1, 2, 5), Bool())),
                         getName);

} // namespace

} // namespace video::rgb::test
