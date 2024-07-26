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

#include <common/Formatting.h>
#include <common/Testing.h>
#include <video/rgb/PixelFormatRGBGuess.h>

namespace video::rgb::test
{

struct TestParameters
{
  std::string    filename{};
  Size           frameSize{};
  std::int64_t   fileSize{};
  PixelFormatRGB expectedPixelFormst{};
};

class GuessRGBFormatFromFilenameFrameSizeAndFileSize : public TestWithParam<TestParameters>
{
};

std::string getTestName(const testing::TestParamInfo<TestParameters> &testParametersInfo)
{
  const auto testParameters = testParametersInfo.param;

  return yuviewTest::formatTestName("FileName",
                                    testParameters.filename,
                                    "Size",
                                    testParameters.frameSize.width,
                                    testParameters.frameSize.height,
                                    "FileSize",
                                    testParameters.fileSize,
                                    "ExpectedRGBFormat",
                                    testParameters.expectedPixelFormst.getName());
}

TEST_P(GuessRGBFormatFromFilenameFrameSizeAndFileSize, TestGuess)
{
  const auto parameters = GetParam();

  const QFileInfo fileInfo(QString::fromStdString(parameters.filename));
  const auto      guessedRGBFormat =
      video::rgb::guessFormatFromSizeAndName(fileInfo, parameters.frameSize, parameters.fileSize);

  EXPECT_TRUE(guessedRGBFormat.isValid());
  EXPECT_EQ(guessedRGBFormat, parameters.expectedPixelFormst);
}

constexpr auto BytesNoAlpha   = 1920u * 1080 * 12 * 3; // 12 frames RGB
constexpr auto NotEnoughBytes = 22u;
constexpr auto UnfittingBytes = 1920u * 1080 * 5;

INSTANTIATE_TEST_SUITE_P(
    VideoRGBTest,
    GuessRGBFormatFromFilenameFrameSizeAndFileSize,
    Values(
        TestParameters({"noIndicatorHere.yuv",
                        Size(0, 0),
                        0,
                        PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB)}),
        TestParameters({"something_1920x1080.yuv",
                        Size(1920, 1080),
                        BytesNoAlpha,
                        PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB)}),

        // No Alpha
        TestParameters({"something_1920x1080_rgb.yuv",
                        Size(1920, 1080),
                        BytesNoAlpha,
                        PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB)}),
        TestParameters({"something_1920x1080_rbg.yuv",
                        Size(1920, 1080),
                        BytesNoAlpha,
                        PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RBG)}),
        TestParameters({"something_1920x1080_grb.yuv",
                        Size(1920, 1080),
                        BytesNoAlpha,
                        PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::GRB)}),
        TestParameters({"something_1920x1080_gbr.yuv",
                        Size(1920, 1080),
                        BytesNoAlpha,
                        PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::GBR)}),
        TestParameters({"something_1920x1080_brg.yuv",
                        Size(1920, 1080),
                        BytesNoAlpha,
                        PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::BRG)}),
        TestParameters({"something_1920x1080_bgr.yuv",
                        Size(1920, 1080),
                        BytesNoAlpha,
                        PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::BGR)}),

        // Alpha First
        TestParameters(
            {"something_1920x1080_argb.yuv",
             Size(1920, 1080),
             BytesNoAlpha,
             PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB, AlphaMode::First)}),
        TestParameters(
            {"something_1920x1080_arbg.yuv",
             Size(1920, 1080),
             BytesNoAlpha,
             PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RBG, AlphaMode::First)}),
        TestParameters(
            {"something_1920x1080_agrb.yuv",
             Size(1920, 1080),
             BytesNoAlpha,
             PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::GRB, AlphaMode::First)}),
        TestParameters(
            {"something_1920x1080_agbr.yuv",
             Size(1920, 1080),
             BytesNoAlpha,
             PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::GBR, AlphaMode::First)}),
        TestParameters(
            {"something_1920x1080_abrg.yuv",
             Size(1920, 1080),
             BytesNoAlpha,
             PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::BRG, AlphaMode::First)}),
        TestParameters(
            {"something_1920x1080_abgr.yuv",
             Size(1920, 1080),
             BytesNoAlpha,
             PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::BGR, AlphaMode::First)}),

        // Alpha Last
        TestParameters({"something_1920x1080_rgba.yuv",
                        Size(1920, 1080),
                        BytesNoAlpha,
                        PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB, AlphaMode::Last)}),
        TestParameters({"something_1920x1080_rbga.yuv",
                        Size(1920, 1080),
                        BytesNoAlpha,
                        PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RBG, AlphaMode::Last)}),
        TestParameters({"something_1920x1080_grba.yuv",
                        Size(1920, 1080),
                        BytesNoAlpha,
                        PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::GRB, AlphaMode::Last)}),
        TestParameters({"something_1920x1080_gbra.yuv",
                        Size(1920, 1080),
                        BytesNoAlpha,
                        PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::GBR, AlphaMode::Last)}),
        TestParameters({"something_1920x1080_brga.yuv",
                        Size(1920, 1080),
                        BytesNoAlpha,
                        PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::BRG, AlphaMode::Last)}),
        TestParameters({"something_1920x1080_bgra.yuv",
                        Size(1920, 1080),
                        BytesNoAlpha,
                        PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::BGR, AlphaMode::Last)}),

        // Bit dephts
        TestParameters({"something_1920x1080_rgb10.yuv",
                        Size(1920, 1080),
                        BytesNoAlpha,
                        PixelFormatRGB(10, DataLayout::Packed, ChannelOrder::RGB)}),
        TestParameters({"something_1920x1080_rgb12.yuv",
                        Size(1920, 1080),
                        BytesNoAlpha,
                        PixelFormatRGB(12, DataLayout::Packed, ChannelOrder::RGB)}),
        TestParameters({"something_1920x1080_rgb16.yuv",
                        Size(1920, 1080),
                        BytesNoAlpha,
                        PixelFormatRGB(16, DataLayout::Packed, ChannelOrder::RGB)}),
        TestParameters({"something_1920x1080_rgb48.yuv",
                        Size(1920, 1080),
                        BytesNoAlpha,
                        PixelFormatRGB(16, DataLayout::Packed, ChannelOrder::RGB)}),
        TestParameters({"something_1920x1080_rgb64.yuv",
                        Size(1920, 1080),
                        BytesNoAlpha,
                        PixelFormatRGB(16, DataLayout::Packed, ChannelOrder::RGB)}),
        TestParameters({"something_1920x1080_rgb11.yuv",
                        Size(1920, 1080),
                        BytesNoAlpha,
                        PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB)}),

        // Endianness
        TestParameters({"something_1920x1080_rgb8le.yuv",
                        Size(1920, 1080),
                        BytesNoAlpha,
                        PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB)}),
        TestParameters({"something_1920x1080_rgb8be.yuv",
                        Size(1920, 1080),
                        BytesNoAlpha,
                        PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB)}),
        TestParameters({"something_1920x1080_rgb10le.yuv",
                        Size(1920, 1080),
                        BytesNoAlpha,
                        PixelFormatRGB(10, DataLayout::Packed, ChannelOrder::RGB)}),
        TestParameters(
            {"something_1920x1080_rgb10be.yuv",
             Size(1920, 1080),
             BytesNoAlpha,
             PixelFormatRGB(
                 10, DataLayout::Packed, ChannelOrder::RGB, AlphaMode::None, Endianness::Big)}),
        TestParameters(
            {"something_1920x1080_rgb16be.yuv",
             Size(1920, 1080),
             BytesNoAlpha,
             PixelFormatRGB(
                 16, DataLayout::Packed, ChannelOrder::RGB, AlphaMode::None, Endianness::Big)}),

        // DataLayout
        TestParameters({"something_1920x1080_rgb_packed.yuv",
                        Size(1920, 1080),
                        BytesNoAlpha,
                        PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB)}),
        TestParameters({"something_1920x1080_rgb_planar.yuv",
                        Size(1920, 1080),
                        BytesNoAlpha,
                        PixelFormatRGB(8, DataLayout::Planar, ChannelOrder::RGB)}),
        TestParameters({"something_1920x1080_rgb10le_planar.yuv",
                        Size(1920, 1080),
                        BytesNoAlpha,
                        PixelFormatRGB(10, DataLayout::Planar, ChannelOrder::RGB)}),
        TestParameters(
            {"something_1920x1080_rgb10be_planar.yuv",
             Size(1920, 1080),
             BytesNoAlpha,
             PixelFormatRGB(
                 10, DataLayout::Planar, ChannelOrder::RGB, AlphaMode::None, Endianness::Big)}),
        TestParameters({"something_1920x1080_rgb16_planar.yuv",
                        Size(1920, 1080),
                        BytesNoAlpha,
                        PixelFormatRGB(16, DataLayout::Planar, ChannelOrder::RGB)}),
        TestParameters(
            {"something_1920x1080_rgb16be_planar.yuv",
             Size(1920, 1080),
             BytesNoAlpha,
             PixelFormatRGB(
                 16, DataLayout::Planar, ChannelOrder::RGB, AlphaMode::None, Endianness::Big)}),

        // File size check
        TestParameters({"something_1920x1080_rgb10.yuv",
                        Size(1920, 1080),
                        NotEnoughBytes,
                        PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB)}),
        TestParameters({"something_1920x1080_rgb16be.yuv",
                        Size(1920, 1080),
                        NotEnoughBytes,
                        PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB)}),
        TestParameters({"something_1920x1080_rgb16be.yuv",
                        Size(1920, 1080),
                        UnfittingBytes,
                        PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB)})

            ),
    getTestName);

} // namespace video::rgb::test
