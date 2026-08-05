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

#include <common/Formatting.h>
#include <filesource/FrameFormatGuess.h>

#include "FormatGuessingParameters.h"

namespace filesource::frameFormatGuess::test
{

using ExpectedGuessResult = GuessedFrameFormat;
using TestParam           = std::pair<FileInfoForGuess, ExpectedGuessResult>;

class GuessFormatFromFilenameTest : public TestWithParam<TestParam>
{
};

std::string getTestName(const testing::TestParamInfo<TestParam> &testParam)
{
  const auto [fileInfoForGuess, expectedFormat] = testParam.param;
  return formatFileInfoForGuessForTestName(fileInfoForGuess) + "_" +
         formatGuessedFrameFormatForTestName(expectedFormat);
}

TEST_P(GuessFormatFromFilenameTest, TestFormatFromFilename)
{
  const auto [fileInfoForGuess, expectedFormat] = GetParam();

  const auto actualFormat = guessFrameFormat(fileInfoForGuess);

  EXPECT_EQ(actualFormat, expectedFormat);
}

INSTANTIATE_TEST_SUITE_P(
    FilesourceTest,
    GuessFormatFromFilenameTest,
    Values(
        // Resolution must use an 'x' (case irrelevant) or a '*' between width/height
        std::make_pair(FileInfoForGuess({"something_1920x1080.yuv", "", {}}),
                       ExpectedGuessResult({Size(1920, 1080), {}, {}, {}})),
        std::make_pair(FileInfoForGuess({"something_295x289.yuv", "", {}}),
                       ExpectedGuessResult({Size(295, 289), {}, {}, {}})),
        std::make_pair(FileInfoForGuess({"something_295234x289234.yuv", "", {}}),
                       ExpectedGuessResult({Size(295234, 289234), {}, {}, {}})),
        std::make_pair(FileInfoForGuess({"something_1920X1080.yuv", "", {}}),
                       ExpectedGuessResult({Size(1920, 1080), {}, {}, {}})),
        std::make_pair(FileInfoForGuess({"something_1920*1080.yuv", "", {}}),
                       ExpectedGuessResult({Size(1920, 1080), {}, {}, {}})),
        std::make_pair(FileInfoForGuess({"something_1920x1080_something.yuv", "", {}}),
                       ExpectedGuessResult({Size(1920, 1080), {}, {}, {}})),

        // Other characters are not supported
        std::make_pair(FileInfoForGuess({"something_1920_1080.yuv", "", {}}),
                       ExpectedGuessResult({{}, {}, {}, {}})),
        std::make_pair(FileInfoForGuess({"something_19201080.yuv", "", {}}),
                       ExpectedGuessResult({{}, {}, {}, {}})),
        std::make_pair(FileInfoForGuess({"something_1280-720.yuv", "", {}}),
                       ExpectedGuessResult({{}, {}, {}, {}})),
        std::make_pair(FileInfoForGuess({"something_1920-1080_something.yuv", "", {}}),
                       ExpectedGuessResult({{}, {}, {}, {}})),

        // Test fps detection with 'xxxhz' or 'xxfps'. Cases should not matter.
        std::make_pair(FileInfoForGuess({"something_1920x1080_25.yuv", "", {}}),
                       ExpectedGuessResult({Size(1920, 1080), 25, {}, {}})),
        std::make_pair(FileInfoForGuess({"something_1920x1080_999.yuv", "", {}}),
                       ExpectedGuessResult({Size(1920, 1080), 999, {}, {}})),
        std::make_pair(FileInfoForGuess({"something_1920x1080_60Hz.yuv", "", {}}),
                       ExpectedGuessResult({Size(1920, 1080), 60, {}, {}})),
        std::make_pair(FileInfoForGuess({"something_1920x1080_999_something.yuv", "", {}}),
                       ExpectedGuessResult({Size(1920, 1080), 999, {}, {}})),
        std::make_pair(FileInfoForGuess({"something_1920x1080_60hz.yuv", "", {}}),
                       ExpectedGuessResult({Size(1920, 1080), 60, {}, {}})),
        std::make_pair(FileInfoForGuess({"something_1920x1080_60HZ.yuv", "", {}}),
                       ExpectedGuessResult({Size(1920, 1080), 60, {}, {}})),
        std::make_pair(FileInfoForGuess({"something_1920x1080_60hZ.yuv", "", {}}),
                       ExpectedGuessResult({Size(1920, 1080), 60, {}, {}})),
        std::make_pair(FileInfoForGuess({"something_1920x1080_60fps.yuv", "", {}}),
                       ExpectedGuessResult({Size(1920, 1080), 60, {}, {}})),
        std::make_pair(FileInfoForGuess({"something_1920x1080_60FPS.yuv", "", {}}),
                       ExpectedGuessResult({Size(1920, 1080), 60, {}, {}})),
        std::make_pair(FileInfoForGuess({"something_1920x1080_60fPs.yuv", "", {}}),
                       ExpectedGuessResult({Size(1920, 1080), 60, {}, {}})),
        // The indicator can even be anywhere
        std::make_pair(FileInfoForGuess({"something240fPssomething_1920x1080.yuv", "", {}}),
                       ExpectedGuessResult({Size(1920, 1080), 240, {}, {}})),

        std::make_pair(FileInfoForGuess({"something_1920x1080_25_8.yuv", "", {}}),
                       ExpectedGuessResult({Size(1920, 1080), 25, 8, {}})),
        std::make_pair(FileInfoForGuess({"something_1920x1080_25_12.yuv", "", {}}),
                       ExpectedGuessResult({Size(1920, 1080), 25, 12, {}})),
        std::make_pair(FileInfoForGuess({"something_1920x1080_25_8b.yuv", "", {}}),
                       ExpectedGuessResult({Size(1920, 1080), 25, 8, {}})),
        std::make_pair(FileInfoForGuess({"something_1920x1080_25_8b_something.yuv", "", {}}),
                       ExpectedGuessResult({Size(1920, 1080), 25, 8, {}})),

        std::make_pair(FileInfoForGuess({"something1080p.yuv", "", {}}),
                       ExpectedGuessResult({Size(1920, 1080), {}, {}, {}})),
        std::make_pair(FileInfoForGuess({"something1080pSomething.yuv", "", {}}),
                       ExpectedGuessResult({Size(1920, 1080), {}, {}, {}})),
        std::make_pair(FileInfoForGuess({"something1080p33.yuv", "", {}}),
                       ExpectedGuessResult({Size(1920, 1080), 33, {}, {}})),
        std::make_pair(FileInfoForGuess({"something1080p33Something.yuv", "", {}}),
                       ExpectedGuessResult({Size(1920, 1080), 33, {}, {}})),
        std::make_pair(FileInfoForGuess({"something720p.yuv", "", {}}),
                       ExpectedGuessResult({Size(1280, 720), {}, {}, {}})),
        std::make_pair(FileInfoForGuess({"something720pSomething.yuv", "", {}}),
                       ExpectedGuessResult({Size(1280, 720), {}, {}, {}})),
        std::make_pair(FileInfoForGuess({"something720p44.yuv", "", {}}),
                       ExpectedGuessResult({Size(1280, 720), 44, {}, {}})),
        std::make_pair(FileInfoForGuess({"something720p44Something.yuv", "", {}}),
                       ExpectedGuessResult({Size(1280, 720), 44, {}, {}})),

        std::make_pair(FileInfoForGuess({"something_cif.yuv", "", {}}),
                       ExpectedGuessResult({Size(352, 288), {}, {}, {}})),
        std::make_pair(FileInfoForGuess({"something_cifSomething.yuv", "", {}}),
                       ExpectedGuessResult({Size(352, 288), {}, {}, {}})),
        std::make_pair(FileInfoForGuess({"something_qcif.yuv", "", {}}),
                       ExpectedGuessResult({Size(176, 144), {}, {}, {}})),
        std::make_pair(FileInfoForGuess({"something_qcifSomething.yuv", "", {}}),
                       ExpectedGuessResult({Size(176, 144), {}, {}, {}})),
        std::make_pair(FileInfoForGuess({"something_4cif.yuv", "", {}}),
                       ExpectedGuessResult({Size(704, 576), {}, {}, {}})),
        std::make_pair(FileInfoForGuess({"something_4cifSomething.yuv", "", {}}),
                       ExpectedGuessResult({Size(704, 576), {}, {}, {}})),
        std::make_pair(FileInfoForGuess({"somethingUHDSomething.yuv", "", {}}),
                       ExpectedGuessResult({Size(3840, 2160), {}, {}, {}})),
        std::make_pair(FileInfoForGuess({"somethingHDSomething.yuv", "", {}}),
                       ExpectedGuessResult({Size(1920, 1080), {}, {}, {}})),

        std::make_pair(FileInfoForGuess({"something_1920x1080_8Bit.yuv", "", {}}),
                       ExpectedGuessResult({Size(1920, 1080), {}, 8, {}})),
        std::make_pair(FileInfoForGuess({"something_1920x1080_10Bit.yuv", "", {}}),
                       ExpectedGuessResult({Size(1920, 1080), {}, 10, {}})),
        std::make_pair(FileInfoForGuess({"something_1920x1080_12Bit.yuv", "", {}}),
                       ExpectedGuessResult({Size(1920, 1080), {}, 12, {}})),
        std::make_pair(FileInfoForGuess({"something_1920x1080_16Bit.yuv", "", {}}),
                       ExpectedGuessResult({Size(1920, 1080), {}, 16, {}})),
        std::make_pair(FileInfoForGuess({"something_1920x1080_8bit.yuv", "", {}}),
                       ExpectedGuessResult({Size(1920, 1080), {}, 8, {}})),
        std::make_pair(FileInfoForGuess({"something_1920x1080_8BIT.yuv", "", {}}),
                       ExpectedGuessResult({Size(1920, 1080), {}, 8, {}})),
        std::make_pair(FileInfoForGuess({"something_1920x1080_8-Bit.yuv", "", {}}),
                       ExpectedGuessResult({Size(1920, 1080), {}, 8, {}})),
        std::make_pair(FileInfoForGuess({"something_1920x1080_8-BIT.yuv", "", {}}),
                       ExpectedGuessResult({Size(1920, 1080), {}, 8, {}})),

        std::make_pair(FileInfoForGuess({"something_1920x1080_packed.yuv", "", {}}),
                       ExpectedGuessResult({Size(1920, 1080), {}, {}, video::DataLayout::Packed})),
        std::make_pair(FileInfoForGuess({"something_1920x1080_packed-something.yuv", "", {}}),
                       ExpectedGuessResult({Size(1920, 1080), {}, {}, video::DataLayout::Packed})),
        std::make_pair(FileInfoForGuess({"something_1920x1080packed.yuv", "", {}}),
                       ExpectedGuessResult({Size(1920, 1080), {}, {}, {}})),
        std::make_pair(FileInfoForGuess({"packed_something_1920x1080.yuv", "", {}}),
                       ExpectedGuessResult({Size(1920, 1080), {}, {}, {}})),

        std::make_pair(
            FileInfoForGuess({"sample_1280x720_16bit_444_packed_20200109_114812.yuv", "", {}}),
            ExpectedGuessResult({Size(1280, 720), {}, 16u, video::DataLayout::Packed})),
        std::make_pair(
            FileInfoForGuess({"sample_1280x720_16b_yuv44416le_packed_20200109_114812.yuv", "", {}}),
            ExpectedGuessResult({Size(1280, 720), {}, 16u, video::DataLayout::Packed})),
        std::make_pair(
            FileInfoForGuess({"sample_1280x720_16b_yuv16le_packed_444_20200109_114812", "", {}}),
            ExpectedGuessResult({Size(1280, 720), {}, 16u, video::DataLayout::Packed}))

            ),
    getTestName);

} // namespace filesource::frameFormatGuess::test
