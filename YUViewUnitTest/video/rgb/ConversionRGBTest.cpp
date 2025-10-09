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

#include "gtest/gtest.h"
#include <array>
#include <common/Testing.h>

#include <string>
#include <tuple>
#include <video/LimitedRangeToFullRange.h>
#include <video/rgb/ConversionRGB.h>

#include "CreateTestData.h"
#include "video/rgb/PixelFormatRGB.h"

using OutputHasAlpha        = bool;
using PremultiplyAlpha      = bool;
using ScalingPerComponent   = std::array<int, 4>;
using InversionPerComponent = std::array<bool, 4>;
using UChaVector            = std::vector<unsigned char>;

namespace video::rgb::test
{

namespace
{

constexpr auto ScalingPerComponentToTest = {ScalingPerComponent({1, 1, 1, 1}),
                                            ScalingPerComponent({2, 1, 1, 1}),
                                            ScalingPerComponent({1, 2, 1, 1}),
                                            ScalingPerComponent({1, 1, 2, 1}),
                                            ScalingPerComponent({1, 1, 1, 2}),
                                            ScalingPerComponent({1, 8, 1, 1})};

constexpr auto InversionPerComponentToTest = {InversionPerComponent({false, false, false, false}),
                                              InversionPerComponent({true, false, false, false}),
                                              InversionPerComponent({false, true, false, false}),
                                              InversionPerComponent({false, false, true, false}),
                                              InversionPerComponent({false, false, false, true}),
                                              InversionPerComponent({true, true, true, true})};

int scaleShiftClipInvertValue(const int  value,
                              const int  bitDepth,
                              const int  scale,
                              const bool invert)
{
  const auto valueOriginalDepth = static_cast<int64_t>(convertBitness(value, 12, bitDepth));
  const auto valueScaled        = valueOriginalDepth * scale;
  const auto value8BitDepth     = (valueScaled >> (bitDepth - 8));
  const auto valueClipped       = functions::clip(value8BitDepth, 0, 255);
  return invert ? (255 - valueClipped) : valueClipped;
};

int scaleShiftClipInvertValueRGB565(const int  value,
                                    Channel    channel,
                                    const int  scale,
                                    const bool invert)
{
  const auto bitDepthIntermediate = (channel == Channel::Green ? 6 : 5);
  const auto valueOriginalDepth =
    static_cast<int64_t>(convertBitness(value, 12, bitDepthIntermediate));
  const auto valueScaled    = valueOriginalDepth * scale;
  const auto value8BitDepth = (valueScaled << (8 - bitDepthIntermediate));
  const auto valueClipped   = functions::clip(value8BitDepth, 0, 255);
  return invert ? (255 - valueClipped) : valueClipped;
}

rgba_t getARGBValueFromDataLittleEndian(const UChaVector &data, const size_t i)
{
  const auto pixelOffset = i * 4;
  return rgba_t({data.at(pixelOffset + 2),
                 data.at(pixelOffset + 1),
                 data.at(pixelOffset),
                 data.at(pixelOffset + 3)});
}

void checkOutputValues(const UChaVector            &data,
                       const PixelFormatRGB        &pixelFormat,
                       const ScalingPerComponent   &scaling,
                       const bool                   limitedRange,
                       const InversionPerComponent &inversion,
                       const bool                   alphaShouldBeSet)
{
  for (size_t i = 0; i < TEST_FRAME_NR_VALUES; ++i)
  {
    auto expectedValue = TEST_VALUES_12BIT.at(i);

    if (pixelFormat.getPredefinedPixelFormat() == PredefinedPixelFormat::RGB565)
    {
      expectedValue.r =
        scaleShiftClipInvertValueRGB565(expectedValue.r, Channel::Red, scaling[0], inversion[0]);
      expectedValue.g =
        scaleShiftClipInvertValueRGB565(expectedValue.g, Channel::Green, scaling[1], inversion[1]);
      expectedValue.b =
        scaleShiftClipInvertValueRGB565(expectedValue.b, Channel::Blue, scaling[2], inversion[2]);
      expectedValue.a = 255;
    }
    else if (pixelFormat.getPredefinedPixelFormat())
      throw std::runtime_error("Unsupported predefined pixel format");
    else
    {
      const auto bitDepth = pixelFormat.getBitsPerComponent();
      expectedValue.r =
        scaleShiftClipInvertValue(expectedValue.r, bitDepth, scaling[0], inversion[0]);
      expectedValue.g =
        scaleShiftClipInvertValue(expectedValue.g, bitDepth, scaling[1], inversion[1]);
      expectedValue.b =
        scaleShiftClipInvertValue(expectedValue.b, bitDepth, scaling[2], inversion[2]);
      expectedValue.a =
        scaleShiftClipInvertValue(expectedValue.a, bitDepth, scaling[3], inversion[3]);
    }

    if (limitedRange)
    {
      expectedValue.r = LimitedRangeToFullRange.at(expectedValue.r);
      expectedValue.g = LimitedRangeToFullRange.at(expectedValue.g);
      expectedValue.b = LimitedRangeToFullRange.at(expectedValue.b);
      // No limited range for alpha
    }

    if (!alphaShouldBeSet)
      expectedValue.a = 255;

    const auto actualValue = getARGBValueFromDataLittleEndian(data, i);

    if (expectedValue != actualValue)
      throw std::runtime_error("For value " + std::to_string(i) + " Expected " +
                               to_string(expectedValue) + " Actual " + to_string(actualValue));
  }
}

void checkOutputValuesForPlane(const UChaVector            &data,
                               const PixelFormatRGB        &pixelFormat,
                               const ScalingPerComponent   &scaling,
                               const bool                   limitedRange,
                               const InversionPerComponent &inversion,
                               const Channel                channel)
{
  for (size_t i = 0; i < TEST_FRAME_NR_VALUES; ++i)
  {
    auto expectedPlaneValue = TEST_VALUES_12BIT[i].at(channel);

    const auto channelIndex = ChannelMapper.indexOf(channel);
    if (pixelFormat.getPredefinedPixelFormat() == PredefinedPixelFormat::RGB565)
    {
      expectedPlaneValue = scaleShiftClipInvertValueRGB565(
        expectedPlaneValue, channel, scaling[channelIndex], inversion[channelIndex]);
    }
    else if (pixelFormat.getPredefinedPixelFormat())
      throw std::runtime_error("Unsupported predefined pixel format");
    else
    {
      const auto bitDepth = pixelFormat.getBitsPerComponent();
      expectedPlaneValue  = scaleShiftClipInvertValue(
        expectedPlaneValue, bitDepth, scaling[channelIndex], inversion[channelIndex]);
    }

    if (limitedRange)
      expectedPlaneValue = LimitedRangeToFullRange.at(expectedPlaneValue);

    const auto expectedValue =
      rgba_t({expectedPlaneValue, expectedPlaneValue, expectedPlaneValue, 255});

    const auto actualValue = getARGBValueFromDataLittleEndian(data, i);

    if (expectedValue != actualValue)
      throw std::runtime_error("For value " + std::to_string(i) + " Expected " +
                               to_string(expectedValue) + " Actual " + to_string(actualValue));
  }
}

void testConversionToRGBA(const QByteArray            &sourceBuffer,
                          const PixelFormatRGB        &srcPixelFormat,
                          const InversionPerComponent &inversion,
                          const ScalingPerComponent   &componentScale,
                          const bool                   limitedRange,
                          const bool                   outputHasAlpha)
{
  UChaVector outputBuffer;
  outputBuffer.resize(TEST_FRAME_NR_VALUES * 4);

  convertInputRGBToARGB(sourceBuffer,
                        srcPixelFormat,
                        outputBuffer.data(),
                        TEST_FRAME_SIZE,
                        inversion.data(),
                        componentScale.data(),
                        limitedRange,
                        outputHasAlpha,
                        PremultiplyAlpha(false));

  const auto alphaShouldBeSet = (outputHasAlpha && srcPixelFormat.hasAlpha());
  checkOutputValues(
    outputBuffer, srcPixelFormat, componentScale, limitedRange, inversion, alphaShouldBeSet);
}

void testConversionToRGBASinglePlane(const QByteArray            &sourceBuffer,
                                     const PixelFormatRGB        &srcPixelFormat,
                                     const InversionPerComponent &inversion,
                                     const ScalingPerComponent   &componentScale,
                                     const bool                   limitedRange,
                                     const bool)
{
  for (const auto channel : ChannelMapper.getValues())
  {
    if (channel == Channel::Alpha && !srcPixelFormat.hasAlpha())
      continue;

    UChaVector outputBuffer;
    outputBuffer.resize(TEST_FRAME_NR_VALUES * 4);

    const auto channelIndex = ChannelMapper.indexOf(channel);

    convertSinglePlaneOfRGBToGreyscaleARGB(sourceBuffer,
                                           srcPixelFormat,
                                           outputBuffer.data(),
                                           TEST_FRAME_SIZE,
                                           channel,
                                           componentScale[channelIndex],
                                           inversion[channelIndex],
                                           limitedRange);

    checkOutputValuesForPlane(
      outputBuffer, srcPixelFormat, componentScale, limitedRange, inversion, channel);
  }
}

using TestingFunction = std::function<void(const QByteArray &,
                                           const video::rgb::PixelFormatRGB &,
                                           const InversionPerComponent &,
                                           const ScalingPerComponent &,
                                           const bool,
                                           const bool)>;

void runTestForAllParameters(const PixelFormatRGB &pixelFormat, TestingFunction testingFunction)
{
  QByteArray data;
  if (pixelFormat.getPredefinedPixelFormat())
    data = createRawRGBData(
      *pixelFormat.getPredefinedPixelFormat(), pixelFormat.getEndianess(), TEST_VALUES_12BIT, 12);
  else
    data = createRawRGBData(pixelFormat, TEST_VALUES_12BIT, 12);

  for (const auto &inversionPerComponent : InversionPerComponentToTest)
    for (const auto &scalingPerComponent : ScalingPerComponentToTest)
      for (const auto &limitedRange : {false, true})
        for (const auto &outputHasAlpha : {false, true})
          testingFunction(data,
                          pixelFormat,
                          inversionPerComponent,
                          scalingPerComponent,
                          limitedRange,
                          outputHasAlpha);
}

class ConversionRGBTest : public TestWithParam<PixelFormatRGB>
{
};

TEST_P(ConversionRGBTest, TestConversionToRGBA)
{
  runTestForAllParameters(GetParam(), testConversionToRGBA);
}

TEST_P(ConversionRGBTest, TestConversionOfSinglePlaneToRGBA)
{
  runTestForAllParameters(GetParam(), testConversionToRGBASinglePlane);
}

std::string getName(const testing::TestParamInfo<ConversionRGBTest::ParamType> &info)
{
  return yuviewTest::replaceNonSupportedCharacters(*info.param.getName());
}

INSTANTIATE_TEST_SUITE_P(VideoRGBTest,
                         ConversionRGBTest,
                         ValuesIn(createTestSetOfPixelFormatRGB()),
                         getName);

} // namespace

} // namespace video::rgb::test
