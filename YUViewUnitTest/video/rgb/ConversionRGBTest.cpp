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

#include <video/LimitedRangeToFullRange.h>
#include <video/rgb/ConversionRGB.h>

#include "CreateTestData.h"

using OutputHasAlpha        = bool;
using PremultiplyAlpha      = bool;
using ScalingPerComponent   = std::array<int, 4>;
using InversionPerComponent = std::array<bool, 4>;
using UChaVector            = std::vector<unsigned char>;

namespace video::rgb::test
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

std::string getTestName(const video::rgb::PixelFormatRGB &pixelFormat,
                        const bool                        outputHasAlpha,
                        const ScalingPerComponent        &scalingPerComponent,
                        const InversionPerComponent      &inversionPerComponent,
                        const bool                        limitedRange)
{
  return yuviewTest::formatTestName("PixelFormat",
                                    pixelFormat.getName(),
                                    "OutputHasAlpha",
                                    outputHasAlpha,
                                    "ScalingPerComponent",
                                    scalingPerComponent,
                                    "InversionPerComponen",
                                    inversionPerComponent,
                                    (limitedRange ? "_limitedRange" : "_fullRange"));
}

int scaleShiftClipInvertValue(const int  value,
                              const int  bitDepth,
                              const int  scale,
                              const bool invert)
{
  const auto valueOriginalDepth = (value >> (12 - bitDepth));
  const auto valueScaled        = valueOriginalDepth * scale;
  const auto value8BitDepth     = (valueScaled >> (bitDepth - 8));
  const auto valueClipped       = functions::clip(value8BitDepth, 0, 255);
  return invert ? (255 - valueClipped) : valueClipped;
};

rgba_t getARGBValueFromDataLittleEndian(const UChaVector &data, const size_t i)
{
  const auto pixelOffset = i * 4;
  return rgba_t({data.at(pixelOffset + 2),
                 data.at(pixelOffset + 1),
                 data.at(pixelOffset),
                 data.at(pixelOffset + 3)});
}

void checkOutputValues(const UChaVector            &data,
                       const int                    bitDepth,
                       const ScalingPerComponent   &scaling,
                       const bool                   limitedRange,
                       const InversionPerComponent &inversion,
                       const bool                   alphaShouldBeSet)
{
  for (size_t i = 0; i < TEST_FRAME_NR_VALUES; ++i)
  {
    auto expectedValue = TEST_VALUES_12BIT.at(i);

    expectedValue.R =
        scaleShiftClipInvertValue(expectedValue.R, bitDepth, scaling[0], inversion[0]);
    expectedValue.G =
        scaleShiftClipInvertValue(expectedValue.G, bitDepth, scaling[1], inversion[1]);
    expectedValue.B =
        scaleShiftClipInvertValue(expectedValue.B, bitDepth, scaling[2], inversion[2]);
    expectedValue.A =
        scaleShiftClipInvertValue(expectedValue.A, bitDepth, scaling[3], inversion[3]);

    if (limitedRange)
    {
      expectedValue.R = LimitedRangeToFullRange.at(expectedValue.R);
      expectedValue.G = LimitedRangeToFullRange.at(expectedValue.G);
      expectedValue.B = LimitedRangeToFullRange.at(expectedValue.B);
      // No limited range for alpha
    }

    if (!alphaShouldBeSet)
      expectedValue.A = 255;

    const auto actualValue = getARGBValueFromDataLittleEndian(data, i);

    if (expectedValue != actualValue)
      throw std::runtime_error("Value " + std::to_string(i));
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
    const auto bitDepth     = pixelFormat.getBitsPerSample();

    expectedPlaneValue = scaleShiftClipInvertValue(
        expectedPlaneValue, bitDepth, scaling[channelIndex], inversion[channelIndex]);

    if (limitedRange)
      expectedPlaneValue = LimitedRangeToFullRange.at(expectedPlaneValue);

    const auto expectedValue =
        rgba_t({expectedPlaneValue, expectedPlaneValue, expectedPlaneValue, 255});

    const auto actualValue = getARGBValueFromDataLittleEndian(data, i);

    if (expectedValue != actualValue)
      throw std::runtime_error("Value " + std::to_string(i));
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
  checkOutputValues(outputBuffer,
                    srcPixelFormat.getBitsPerSample(),
                    componentScale,
                    limitedRange,
                    inversion,
                    alphaShouldBeSet);
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

using TestingFunction = std::function<void(const QByteArray &,
                                           const video::rgb::PixelFormatRGB &,
                                           const InversionPerComponent &,
                                           const ScalingPerComponent &,
                                           const bool,
                                           const bool)>;

void runTestForAllParameters(TestingFunction testingFunction)
{
  for (const auto endianness : {Endianness::Little, Endianness::Big})
  {
    for (const auto bitDepth : {8, 10, 12})
    {
      for (const auto &alphaMode : AlphaModeMapper.getValues())
      {
        for (const auto &dataLayout : video::DataLayoutMapper.getValues())
        {
          for (const auto &channelOrder : video::rgb::ChannelOrderMapper.getValues())
          {
            const video::rgb::PixelFormatRGB format(
                bitDepth, dataLayout, channelOrder, alphaMode, endianness);
            const auto data = createRawRGBData(format);

            for (const auto outputHasAlpha : {false, true})
            {
              for (const auto &componentScale : ScalingPerComponentToTest)
              {
                for (const auto &inversion : InversionPerComponentToTest)
                {
                  for (const auto limitedRange : {false, true})
                  {
                    EXPECT_NO_THROW(testingFunction(
                        data, format, inversion, componentScale, limitedRange, outputHasAlpha))
                        << "parametersAsString";
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

TEST(ConversionRGBTest, TestConversionToRGBA)
{
  runTestForAllParameters(testConversionToRGBA);
}

TEST(ConversionRGBTest, TestConversionOfSinglePlaneToRGBA)
{
  runTestForAllParameters(testConversionToRGBASinglePlane);
}

} // namespace video::rgb::test
