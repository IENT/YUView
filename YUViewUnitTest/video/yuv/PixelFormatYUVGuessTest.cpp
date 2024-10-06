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

#include <video/yuv/PixelFormatYUVGuess.h>

namespace video::yuv::test
{

using BytePacking   = bool;
using BigEndian     = bool;
using ChromaOffset  = Offset;
using UVInterleaved = bool;

struct TestParameters
{
  std::string  filename{};
  Size         frameSize{};
  unsigned     bitDepth{};
  DataLayout   dataLayout{};
  std::int64_t fileSize{};

  PixelFormatYUV expectedPixelFormat{};
};

class GuessYUVFormatFromFilenameFrameSizeFileSizeDataLayoutAndBitDepth
    : public TestWithParam<TestParameters>
{
};

std::string getTestName(const testing::TestParamInfo<TestParameters> &testParametersInfo)
{
  const auto testParameters = testParametersInfo.param;

  return yuviewTest::formatTestName("FileName",
                                    testParameters.filename,
                                    "FrameSize",
                                    testParameters.frameSize,
                                    "BitDepth",
                                    testParameters.bitDepth,
                                    "DataLayout",
                                    DataLayoutMapper.getName(testParameters.dataLayout),
                                    "FileSize",
                                    testParameters.fileSize,
                                    "ExpectedYUVFormat",
                                    testParameters.expectedPixelFormat.getName());
}

TEST_P(GuessYUVFormatFromFilenameFrameSizeFileSizeDataLayoutAndBitDepth, TestGuess)
{
  const auto parameters = GetParam();

  const QFileInfo fileInfo(QString::fromStdString(parameters.filename));
  const auto      guessedFormat = video::yuv::guessFormatFromSizeAndName(parameters.frameSize,
                                                                    parameters.bitDepth,
                                                                    parameters.dataLayout,
                                                                    parameters.fileSize,
                                                                    fileInfo);

  EXPECT_TRUE(guessedFormat.isValid());
  EXPECT_EQ(guessedFormat, parameters.expectedPixelFormat);
}

constexpr auto BYTES_1080P     = 1920 * 1080 * 3 * 6;      // 12 frames 420
constexpr auto BYTES_720P      = 1280 * 720 * 3 * 6;       // 6 frames 444
constexpr auto BYTES_720P_V210 = 1296u * 720 / 6 * 16 * 3; // 3 frames
constexpr auto BYTES_1808P_400 = 1920u * 1080 * 2;         // 2 frames 400

INSTANTIATE_TEST_SUITE_P(
    VideoYUVTest,
    GuessYUVFormatFromFilenameFrameSizeFileSizeDataLayoutAndBitDepth,
    Values(TestParameters({"something_1920x1080_25_8.yuv",
                           Size(1920, 1080),
                           0,
                           DataLayout::Planar,
                           BYTES_1080P,
                           PixelFormatYUV(Subsampling::YUV_420, 8)}),

           TestParameters({"something_1920x1080_25_8.yuv",
                           Size(1920, 1080),
                           8,
                           DataLayout::Planar,
                           BYTES_1080P,
                           PixelFormatYUV(Subsampling::YUV_420, 8)}),
           TestParameters({"something_1920x1080_25_12.yuv",
                           Size(1920, 1080),
                           12,
                           DataLayout::Planar,
                           BYTES_1080P,
                           PixelFormatYUV(Subsampling::YUV_420, 12)}),
           TestParameters({"something_1920x1080_25_16b.yuv",
                           Size(1920, 1080),
                           16,
                           DataLayout::Planar,
                           BYTES_1080P,
                           PixelFormatYUV(Subsampling::YUV_420, 16)}),
           TestParameters({"something_1920x1080_25_10b_something.yuv",
                           Size(1920, 1080),
                           10,
                           DataLayout::Planar,
                           BYTES_1080P,
                           PixelFormatYUV(Subsampling::YUV_420, 10)}),

           // Issue 211
           TestParameters({"sample_1280x720_16bit_444_packed_20200109_114812.yuv",
                           Size(1280, 720),
                           16,
                           DataLayout::Packed,
                           BYTES_720P,
                           PixelFormatYUV(Subsampling::YUV_444, 16, PackingOrder::YUV)}),
           TestParameters({"sample_1280x720_16b_yuv44416le_packed_20200109_114812.yuv",
                           Size(1280, 720),
                           16,
                           DataLayout::Packed,
                           BYTES_720P,
                           PixelFormatYUV(Subsampling::YUV_444, 16, PackingOrder::YUV)}),
           TestParameters({"sample_1280x720_16b_yuv16le_packed_444_20200109_114812",
                           Size(1280, 720),
                           16,
                           DataLayout::Packed,
                           BYTES_720P,
                           PixelFormatYUV(Subsampling::YUV_444, 16, PackingOrder::YUV)}),

           // Issue 221
           TestParameters({"sample_1280x720_yuv420pUVI_114812.yuv",
                           Size(1280, 720),
                           16,
                           DataLayout::Planar,
                           BYTES_720P,
                           PixelFormatYUV(Subsampling::YUV_420,
                                          8,
                                          PlaneOrder::YUV,
                                          BigEndian(false),
                                          ChromaOffset(0, 0),
                                          UVInterleaved(true))}),
           TestParameters({"sample_1280x720_yuv420pinterlaced_114812.yuv",
                           Size(1280, 720),
                           16,
                           DataLayout::Planar,
                           BYTES_720P,
                           PixelFormatYUV(Subsampling::YUV_420,
                                          8,
                                          PlaneOrder::YUV,
                                          BigEndian(false),
                                          ChromaOffset(0, 0),
                                          UVInterleaved(true))}),
           TestParameters({"sample_1280x720_yuv444p16leUVI_114812.yuv",
                           Size(1280, 720),
                           16,
                           DataLayout::Planar,
                           BYTES_720P,
                           PixelFormatYUV(Subsampling::YUV_444,
                                          16,
                                          PlaneOrder::YUV,
                                          BigEndian(false),
                                          ChromaOffset(0, 0),
                                          UVInterleaved(true))}),
           TestParameters({"sample_1280x720_yuv444p16leinterlaced_114812.yuv",
                           Size(1280, 720),
                           16,
                           DataLayout::Planar,
                           BYTES_720P,
                           PixelFormatYUV(Subsampling::YUV_444,
                                          16,
                                          PlaneOrder::YUV,
                                          BigEndian(false),
                                          ChromaOffset(0, 0),
                                          UVInterleaved(true))}),

           // Invalid interlaced indicators
           TestParameters({"sample_1280x720_yuv420pUVVI_114812.yuv",
                           Size(1280, 720),
                           8,
                           DataLayout::Planar,
                           BYTES_720P,
                           PixelFormatYUV(Subsampling::YUV_420, 8)}),
           TestParameters({"sample_1280x720_yuv420pinnterlaced_114812.yuv",
                           Size(1280, 720),
                           8,
                           DataLayout::Planar,
                           BYTES_720P,
                           PixelFormatYUV(Subsampling::YUV_420, 8)}),
           TestParameters({"sample_1280x720_yuv444p16leUVVI_114812.yuv",
                           Size(1280, 720),
                           16,
                           DataLayout::Planar,
                           BYTES_720P,
                           PixelFormatYUV(Subsampling::YUV_444, 16)}),
           TestParameters({"sample_1280x720_yuv444p16leinnterlaced_114812.yuv",
                           Size(1280, 720),
                           16,
                           DataLayout::Planar,
                           BYTES_720P,
                           PixelFormatYUV(Subsampling::YUV_444, 16)}),

           // V210 format (w must be multiple of 48)
           TestParameters({"sample_1280x720_v210.yuv",
                           Size(1280, 720),
                           10,
                           DataLayout::Packed,
                           BYTES_720P_V210,
                           PixelFormatYUV(PredefinedPixelFormat::V210)}),
           TestParameters({"sample_1280x720_v210.something.yuv",
                           Size(1280, 720),
                           10,
                           DataLayout::Packed,
                           BYTES_720P_V210,
                           PixelFormatYUV(PredefinedPixelFormat::V210)}),

           // 4:0:0 formats
           TestParameters({"sample_1920x1080_YUV400p16LE.yuv",
                           Size(1920, 1080),
                           16,
                           DataLayout::Planar,
                           BYTES_1808P_400,
                           PixelFormatYUV(Subsampling::YUV_400, 16)}),
           TestParameters({"sample_1920x1080_gray8le.yuv",
                           Size(1920, 1080),
                           0,
                           DataLayout::Planar,
                           BYTES_1808P_400,
                           PixelFormatYUV(Subsampling::YUV_400, 8)}),
           TestParameters({"sample_1920x1080_gray10le.yuv",
                           Size(1920, 1080),
                           0,
                           DataLayout::Planar,
                           BYTES_1808P_400,
                           PixelFormatYUV(Subsampling::YUV_400, 10)}),
           TestParameters({"sample_1920x1080_gray16le.yuv",
                           Size(1920, 1080),
                           0,
                           DataLayout::Planar,
                           BYTES_1808P_400,
                           PixelFormatYUV(Subsampling::YUV_400, 16)}),

           // Raw bayer file
           TestParameters({"sample_1920x1080_something.raw",
                           Size(1920, 1080),
                           8,
                           DataLayout::Planar,
                           BYTES_1808P_400,
                           PixelFormatYUV(Subsampling::YUV_400, 8)})

           // More tests please :)

           ),
    getTestName);

} // namespace video::yuv::test
