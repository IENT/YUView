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

#include "video/rgb/PixelFormatRGB.h"
#include <common/Testing.h>

#include <filesource/FormatGuessingParameters.h>
#include <video/rgb/PixelFormatRGBGuess.h>

namespace video::rgb::test
{

using filesource::frameFormatGuess::FileInfoForGuess;
using filesource::frameFormatGuess::GuessedFrameFormat;

struct TestParameters
{
  FileInfoForGuess fileInfoForGuess;
  PixelFormatRGB   expectedPixelFormat{};
};

class GuessRGBFormatFromFilenameFrameSizeAndFileSize : public TestWithParam<TestParameters>
{
};

std::string getTestName(const testing::TestParamInfo<TestParameters> &testParametersInfo)
{
  const auto testParameters = testParametersInfo.param;

  return filesource::frameFormatGuess::test::formatFileInfoForGuessForTestName(
           testParameters.fileInfoForGuess) +
         "_" +
         yuviewTest::replaceNonSupportedCharacters(
           testParameters.expectedPixelFormat.getName().value_or("InvalidRGBFormat"));
}

TEST_P(GuessRGBFormatFromFilenameFrameSizeAndFileSize, TestGuess)
{
  const auto parameters = GetParam();

  const auto guessedFrameFormat =
    filesource::frameFormatGuess::guessFrameFormat(parameters.fileInfoForGuess);

  const auto guessedFormat =
    video::rgb::guessPixelFormatFromSizeAndName(guessedFrameFormat, parameters.fileInfoForGuess);

  EXPECT_EQ(guessedFormat.isValid(), parameters.expectedPixelFormat.isValid());
  if (guessedFormat.isValid())
  {
    EXPECT_EQ(guessedFormat, parameters.expectedPixelFormat);
  }
}

constexpr auto BytesNoAlpha    = 1920u * 1080 * 12u * 3u; // 12 frames RGB
constexpr auto NotEnoughBytes  = 22u;
constexpr auto UnfittingBytes  = 1920u * 1080u * 5u;
constexpr auto BytesBayerFile  = 512u * 768u * 4u * 12u; // 12 frames raw bayer
constexpr auto BytesRGB565File = 512u * 768u * 2u * 12u; // 12 frames 2 bytes per pixel

INSTANTIATE_TEST_SUITE_P(
  VideoRGBTest,
  GuessRGBFormatFromFilenameFrameSizeAndFileSize,
  Values(
    // Cases that should not detect anything
    TestParameters({FileInfoForGuess({"noIndicatorHere.yuv", "", 0}), PixelFormatRGB()}),
    TestParameters({FileInfoForGuess({"something_1920x1080.yuv", "", BytesNoAlpha}),
                    PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB)}),

    // No Alpha
    TestParameters({FileInfoForGuess({"something_1920x1080_rgb.yuv", "", BytesNoAlpha}),
                    PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB)}),
    TestParameters({FileInfoForGuess({"something_1920x1080_rbg.yuv", "", BytesNoAlpha}),
                    PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RBG)}),
    TestParameters({FileInfoForGuess({"something_1920x1080_grb.yuv", "", BytesNoAlpha}),
                    PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::GRB)}),
    TestParameters({FileInfoForGuess({"something_1920x1080_gbr.yuv", "", BytesNoAlpha}),
                    PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::GBR)}),
    TestParameters({FileInfoForGuess({"something_1920x1080_brg.yuv", "", BytesNoAlpha}),
                    PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::BRG)}),
    TestParameters({FileInfoForGuess({"something_1920x1080_bgr.yuv", "", BytesNoAlpha}),
                    PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::BGR)}),

    // Alpha First
    TestParameters({FileInfoForGuess({"something_1920x1080_argb.yuv", "", BytesNoAlpha}),
                    PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB, AlphaMode::First)}),
    TestParameters({FileInfoForGuess({"something_1920x1080_arbg.yuv", "", BytesNoAlpha}),
                    PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RBG, AlphaMode::First)}),
    TestParameters({FileInfoForGuess({"something_1920x1080_agrb.yuv", "", BytesNoAlpha}),
                    PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::GRB, AlphaMode::First)}),
    TestParameters({FileInfoForGuess({"something_1920x1080_agbr.yuv", "", BytesNoAlpha}),
                    PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::GBR, AlphaMode::First)}),
    TestParameters({FileInfoForGuess({"something_1920x1080_abrg.yuv", "", BytesNoAlpha}),
                    PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::BRG, AlphaMode::First)}),
    TestParameters({FileInfoForGuess({"something_1920x1080_abgr.yuv", "", BytesNoAlpha}),
                    PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::BGR, AlphaMode::First)}),

    // Alpha Last
    TestParameters({FileInfoForGuess({"something_1920x1080_rgba.yuv", "", BytesNoAlpha}),
                    PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB, AlphaMode::Last)}),
    TestParameters({FileInfoForGuess({"something_1920x1080_rbga.yuv", "", BytesNoAlpha}),
                    PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RBG, AlphaMode::Last)}),
    TestParameters({FileInfoForGuess({"something_1920x1080_grba.yuv", "", BytesNoAlpha}),
                    PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::GRB, AlphaMode::Last)}),
    TestParameters({FileInfoForGuess({"something_1920x1080_gbra.yuv", "", BytesNoAlpha}),
                    PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::GBR, AlphaMode::Last)}),
    TestParameters({FileInfoForGuess({"something_1920x1080_brga.yuv", "", BytesNoAlpha}),
                    PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::BRG, AlphaMode::Last)}),
    TestParameters({FileInfoForGuess({"something_1920x1080_bgra.yuv", "", BytesNoAlpha}),
                    PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::BGR, AlphaMode::Last)}),

    // Bit dephts
    TestParameters({FileInfoForGuess({"something_1920x1080_rgb10.yuv", "", BytesNoAlpha}),
                    PixelFormatRGB(10, DataLayout::Packed, ChannelOrder::RGB)}),
    TestParameters({FileInfoForGuess({"something_1920x1080_rgb12.yuv", "", BytesNoAlpha}),
                    PixelFormatRGB(12, DataLayout::Packed, ChannelOrder::RGB)}),
    TestParameters({FileInfoForGuess({"something_1920x1080_rgb16.yuv", "", BytesNoAlpha}),
                    PixelFormatRGB(16, DataLayout::Packed, ChannelOrder::RGB)}),
    TestParameters({FileInfoForGuess({"something_1920x1080_rgb48.yuv", "", BytesNoAlpha}),
                    PixelFormatRGB(16, DataLayout::Packed, ChannelOrder::RGB)}),
    TestParameters({FileInfoForGuess({"something_1920x1080_rgb64.yuv", "", BytesNoAlpha}),
                    PixelFormatRGB(16, DataLayout::Packed, ChannelOrder::RGB)}),
    TestParameters({FileInfoForGuess({"something_1920x1080_rgb32.yuv", "", BytesNoAlpha}),
                    PixelFormatRGB(32, DataLayout::Packed, ChannelOrder::RGB)}),
    TestParameters({FileInfoForGuess({"something_1920x1080_rgb96.yuv", "", BytesNoAlpha}),
                    PixelFormatRGB(32, DataLayout::Packed, ChannelOrder::RGB)}),
    TestParameters({FileInfoForGuess({"something_1920x1080_rgb128.yuv", "", BytesNoAlpha}),
                    PixelFormatRGB(32, DataLayout::Packed, ChannelOrder::RGB)}),
    TestParameters({FileInfoForGuess({"something_1920x1080_rgb11.yuv", "", BytesNoAlpha}),
                    PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB)}),

    // Endianness
    TestParameters({FileInfoForGuess({"something_1920x1080_rgb8le.yuv", "", BytesNoAlpha}),
                    PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB)}),
    TestParameters({FileInfoForGuess({"something_1920x1080_rgb8be.yuv", "", BytesNoAlpha}),
                    PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB)}),
    TestParameters({FileInfoForGuess({"something_1920x1080_rgb10le.yuv", "", BytesNoAlpha}),
                    PixelFormatRGB(10, DataLayout::Packed, ChannelOrder::RGB)}),
    TestParameters(
      {FileInfoForGuess({"something_1920x1080_rgb10be.yuv", "", BytesNoAlpha}),
       PixelFormatRGB(
         10, DataLayout::Packed, ChannelOrder::RGB, AlphaMode::None, Endianness::Big)}),
    TestParameters(
      {FileInfoForGuess({"something_1920x1080_rgb16be.yuv", "", BytesNoAlpha}),
       PixelFormatRGB(
         16, DataLayout::Packed, ChannelOrder::RGB, AlphaMode::None, Endianness::Big)}),

    // DataLayout
    TestParameters({FileInfoForGuess({"something_1920x1080_rgb_packed.yuv", "", BytesNoAlpha}),
                    PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB)}),
    TestParameters({FileInfoForGuess({"something_1920x1080_rgb_planar.yuv", "", BytesNoAlpha}),
                    PixelFormatRGB(8, DataLayout::Planar, ChannelOrder::RGB)}),
    TestParameters({FileInfoForGuess({"something_1920x1080_rgb10le_planar.yuv", "", BytesNoAlpha}),
                    PixelFormatRGB(10, DataLayout::Planar, ChannelOrder::RGB)}),
    TestParameters(
      {FileInfoForGuess({"something_1920x1080_rgb10be_planar.yuv", "", BytesNoAlpha}),
       PixelFormatRGB(
         10, DataLayout::Planar, ChannelOrder::RGB, AlphaMode::None, Endianness::Big)}),
    TestParameters({FileInfoForGuess({"something_1920x1080_rgb16_planar.yuv", "", BytesNoAlpha}),
                    PixelFormatRGB(16, DataLayout::Planar, ChannelOrder::RGB)}),
    TestParameters(
      {FileInfoForGuess({"something_1920x1080_rgb16be_planar.yuv", "", BytesNoAlpha}),
       PixelFormatRGB(
         16, DataLayout::Planar, ChannelOrder::RGB, AlphaMode::None, Endianness::Big)}),

    // File size check
    TestParameters({FileInfoForGuess({"something_1920x1080_rgb10.yuv", "", NotEnoughBytes}),
                    PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB)}),
    TestParameters({FileInfoForGuess({"something_1920x1080_rgb16be.yuv", "", NotEnoughBytes}),
                    PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB)}),
    TestParameters({FileInfoForGuess({"something_1920x1080_rgb16be.yuv", "", UnfittingBytes}),
                    PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB)}),

    // Format from file extension
    TestParameters({FileInfoForGuess({"something_1920x1080.rgb", "", BytesNoAlpha}),
                    PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB)}),
    TestParameters({FileInfoForGuess({"something_1920x1080.rbg", "", BytesNoAlpha}),
                    PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RBG)}),
    TestParameters({FileInfoForGuess({"something_1920x1080.grb", "", BytesNoAlpha}),
                    PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::GRB)}),
    TestParameters({FileInfoForGuess({"something_1920x1080.gbr", "", BytesNoAlpha}),
                    PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::GBR)}),
    TestParameters({FileInfoForGuess({"something_1920x1080.brg", "", BytesNoAlpha}),
                    PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::BRG)}),
    TestParameters({FileInfoForGuess({"something_1920x1080.bgr", "", BytesNoAlpha}),
                    PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::BGR)}),

    // CMYK file
    TestParameters({FileInfoForGuess({"something_512x768.cmyk", "", BytesBayerFile}),
                    PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB, AlphaMode::Last)}),

    // RGB565
    TestParameters({FileInfoForGuess({"something_512x768_rgb565.rgb", "", BytesRGB565File}),
                    PixelFormatRGB(PredefinedPixelFormat::RGB565)}),
    TestParameters(
      {FileInfoForGuess({"something_512x768_rgb565_something.rgb", "", BytesRGB565File}),
       PixelFormatRGB(PredefinedPixelFormat::RGB565)}),
    TestParameters({FileInfoForGuess({"something_512x768_rgb565le.rgb", "", BytesRGB565File}),
                    PixelFormatRGB(PredefinedPixelFormat::RGB565)}),
    TestParameters({FileInfoForGuess({"something_512x768_rgb565be.rgb", "", BytesRGB565File}),
                    PixelFormatRGB(PredefinedPixelFormat::RGB565, Endianness::Big)})

      ),
  getTestName);

} // namespace video::rgb::test
