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

#include <filesource/FormatGuessingParameters.h>
#include <filesource/FrameFormatGuess.h>
#include <video/yuv/PixelFormatYUVGuess.h>

namespace video::yuv::test
{

using BytePacking   = bool;
using BigEndian     = bool;
using ChromaOffset  = Offset;
using UVInterleaved = bool;

using filesource::frameFormatGuess::FileInfoForGuess;
using filesource::frameFormatGuess::GuessedFrameFormat;

struct TestParameters
{
  FileInfoForGuess fileInfoForGuess;
  PixelFormatYUV   expectedPixelFormat{};
};

class GuessYUVFormatFromFilenameFrameSizeFileSizeDataLayoutAndBitDepth
    : public TestWithParam<TestParameters>
{
};

std::string getTestName(const testing::TestParamInfo<TestParameters> &testParametersInfo)
{
  const auto testParameters = testParametersInfo.param;
  return filesource::frameFormatGuess::test::formatFileInfoForGuessForTestName(
             testParameters.fileInfoForGuess) +
         "_" +
         yuviewTest::replaceNonSupportedCharacters(testParameters.expectedPixelFormat.getName());
}

TEST_P(GuessYUVFormatFromFilenameFrameSizeFileSizeDataLayoutAndBitDepth, TestGuess)
{
  const auto parameters = GetParam();

  const auto guessedFrameFormat =
      filesource::frameFormatGuess::guessFrameFormat(parameters.fileInfoForGuess);

  const auto guessedFormat =
      video::yuv::guessPixelFormatFromSizeAndName(guessedFrameFormat, parameters.fileInfoForGuess);

  EXPECT_TRUE(guessedFormat.isValid());
  EXPECT_EQ(guessedFormat, parameters.expectedPixelFormat)
      << "Guessed format '" << guessedFormat.getName() << "' expected format '"
      << parameters.expectedPixelFormat.getName() << "' for filename '"
      << parameters.fileInfoForGuess.filename << "' parentFolderName '"
      << parameters.fileInfoForGuess.parentFolderName << "' size "
      << parameters.fileInfoForGuess.fileSize.value_or(-1);
}

constexpr auto BYTES_1080P     = 1920 * 1080 * 3 * 6;      // 12 frames 420
constexpr auto BYTES_720P      = 1280 * 720 * 3 * 6;       // 6 frames 444
constexpr auto BYTES_720P_V210 = 1296u * 720 / 6 * 16 * 3; // 3 frames
constexpr auto BYTES_1808P_400 = 1920u * 1080 * 2;         // 2 frames 400

INSTANTIATE_TEST_SUITE_P(
    VideoYUVTest,
    GuessYUVFormatFromFilenameFrameSizeFileSizeDataLayoutAndBitDepth,
    Values(
        TestParameters({FileInfoForGuess({"something_1920x1080_25_8.yuv", "", BYTES_1080P}),
                        PixelFormatYUV(Subsampling::YUV_420, 8)}),
        TestParameters({FileInfoForGuess({"something_1920x1080_25_10.yuv", "", BYTES_1080P}),
                        PixelFormatYUV(Subsampling::YUV_420, 10)}),
        TestParameters({FileInfoForGuess({"something_1920x1080_25_12.yuv", "", BYTES_1080P}),
                        PixelFormatYUV(Subsampling::YUV_420, 12)}),
        TestParameters({FileInfoForGuess({"something_1920x1080_25_16b.yuv", "", BYTES_1080P}),
                        PixelFormatYUV(Subsampling::YUV_420, 16)}),
        TestParameters(
            {FileInfoForGuess({"something_1920x1080_25_10b_something.yuv", "", BYTES_1080P}),
             PixelFormatYUV(Subsampling::YUV_420, 10)}),

        // Issue 211
        TestParameters({FileInfoForGuess({"sample_1280x720_16bit_444_packed_20200109_114812.yuv",
                                          "",
                                          BYTES_720P}),
                        PixelFormatYUV(Subsampling::YUV_444, 16, PackingOrder::YUV)}),
        TestParameters(
            {FileInfoForGuess(
                 {"sample_1280x720_16b_yuv44416le_packed_20200109_114812.yuv", "", BYTES_720P}),
             PixelFormatYUV(Subsampling::YUV_444, 16, PackingOrder::YUV)}),
        TestParameters(
            {FileInfoForGuess(
                 {"sample_1280x720_16b_yuv16le_packed_444_20200109_114812.yuv", "", BYTES_720P}),
             PixelFormatYUV(Subsampling::YUV_444, 16, PackingOrder::YUV)}),

        // Issue 221
        TestParameters({FileInfoForGuess({"sample_1280x720_yuv420pUVI_114812.yuv", "", BYTES_720P}),
                        PixelFormatYUV(Subsampling::YUV_420,
                                       8,
                                       PlaneOrder::YUV,
                                       BigEndian(false),
                                       ChromaOffset(0, 0),
                                       UVInterleaved(true))}),
        TestParameters(
            {FileInfoForGuess({"sample_1280x720_yuv420pinterlaced_114812.yuv", "", BYTES_720P}),
             PixelFormatYUV(Subsampling::YUV_420,
                            8,
                            PlaneOrder::YUV,
                            BigEndian(false),
                            ChromaOffset(0, 0),
                            UVInterleaved(true))}),
        TestParameters(
            {FileInfoForGuess({"sample_1280x720_yuv444p16leUVI_114812.yuv", "", BYTES_720P}),
             PixelFormatYUV(Subsampling::YUV_444,
                            16,
                            PlaneOrder::YUV,
                            BigEndian(false),
                            ChromaOffset(0, 0),
                            UVInterleaved(true))}),
        TestParameters(
            {FileInfoForGuess({"sample_1280x720_yuv444p16leinterlaced_114812.yuv", "", BYTES_720P}),
             PixelFormatYUV(Subsampling::YUV_444,
                            16,
                            PlaneOrder::YUV,
                            BigEndian(false),
                            ChromaOffset(0, 0),
                            UVInterleaved(true))}),

        // Invalid interlaced indicators
        TestParameters(
            {FileInfoForGuess({"sample_1280x720_yuv420pUVVI_114812.yuv", "", BYTES_720P}),
             PixelFormatYUV(Subsampling::YUV_420, 8)}),
        TestParameters(
            {FileInfoForGuess({"sample_1280x720_yuv420pinnterlaced_114812.yuv", "", BYTES_720P}),
             PixelFormatYUV(Subsampling::YUV_420, 8)}),
        TestParameters(
            {FileInfoForGuess({"sample_1280x720_yuv444p16leUVVI_114812.yuv", "", BYTES_720P}),
             PixelFormatYUV(Subsampling::YUV_444, 16)}),
        TestParameters({FileInfoForGuess(
                            {"sample_1280x720_yuv444p16leinnterlaced_114812.yuv", "", BYTES_720P}),
                        PixelFormatYUV(Subsampling::YUV_444, 16)}),

        // V210 format (w must be multiple of 48)
        TestParameters({FileInfoForGuess({"sample_1280x720_v210.yuv", "", BYTES_720P_V210}),
                        PixelFormatYUV(PredefinedPixelFormat::V210)}),
        TestParameters({FileInfoForGuess({"something_1280x720_V210_som.yuv", "", BYTES_720P_V210}),
                        PixelFormatYUV(PredefinedPixelFormat::V210)}),
        TestParameters({FileInfoForGuess({"sample_1280x720.v210", "", BYTES_720P_V210}),
                        PixelFormatYUV(PredefinedPixelFormat::V210)}),
        TestParameters({FileInfoForGuess({"sample_1280x720.V210", "", BYTES_720P_V210}),
                        PixelFormatYUV(PredefinedPixelFormat::V210)}),

        // 4:0:0 formats
        TestParameters({FileInfoForGuess({"sample_1920x1080_YUV400p16LE.yuv", "", BYTES_1808P_400}),
                        PixelFormatYUV(Subsampling::YUV_400, 16)}),
        TestParameters({FileInfoForGuess({"sample_1920x1080_gray8le.yuv", "", BYTES_1808P_400}),
                        PixelFormatYUV(Subsampling::YUV_400, 8)}),
        TestParameters({FileInfoForGuess({"sample_1920x1080_gray10le.yuv", "", BYTES_1808P_400}),
                        PixelFormatYUV(Subsampling::YUV_400, 10)}),
        TestParameters({FileInfoForGuess({"sample_1920x1080_gray16le.yuv", "", BYTES_1808P_400}),
                        PixelFormatYUV(Subsampling::YUV_400, 16)}),

        // Raw bayer file
        TestParameters({FileInfoForGuess({"sample_1920x1080_something.raw", "", BYTES_1808P_400}),
                        PixelFormatYUV(Subsampling::YUV_400, 8)})

        // More tests please :)

        ),
    getTestName);

} // namespace video::yuv::test
