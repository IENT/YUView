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
#include <filesource/GuessFormatFromName.h>

namespace
{

using ExpectedFileFormat = FileFormat;
using FileName           = QString;

using TestParam = std::pair<FileName, ExpectedFileFormat>;

class GuessFormatFromFilenameTest : public TestWithParam<TestParam>
{
};

std::string getTestName(const testing::TestParamInfo<TestParam> &testParam)
{
  const auto [filename, expectedFormat] = testParam.param;

  return yuviewTest::formatTestName("TestName",
                                    filename.toStdString(),
                                    "Size",
                                    expectedFormat.frameSize,
                                    "fps",
                                    expectedFormat.frameRate,
                                    "BitDepth",
                                    expectedFormat.bitDepth,
                                    "Packed",
                                    expectedFormat.packed);
}

TEST_P(GuessFormatFromFilenameTest, TestFormatFromFilename)
{
  const auto [filename, expectedFormat] = GetParam();

  QFileInfo  fileInfo(filename);
  const auto actualFormat = guessFormatFromFilename(fileInfo);

  EXPECT_EQ(actualFormat, expectedFormat);
}

INSTANTIATE_TEST_SUITE_P(
    FilesourceTest,
    GuessFormatFromFilenameTest,
    Values(
        std::make_pair("something_1920x1080.yuv",
                       ExpectedFileFormat({Size(1920, 1080), -1, 0, false})),
        std::make_pair("something_295x289.yuv", ExpectedFileFormat({Size(295, 289), -1, 0, false})),
        std::make_pair("something_295234x289234.yuv",
                       ExpectedFileFormat({Size(295234, 289234), -1, 0, false})),
        std::make_pair("something_1920X1080.yuv",
                       ExpectedFileFormat({Size(1920, 1080), -1, 0, false})),
        std::make_pair("something_1920*1080.yuv",
                       ExpectedFileFormat({Size(1920, 1080), -1, 0, false})),
        std::make_pair("something_1920x1080_something.yuv",
                       ExpectedFileFormat({Size(1920, 1080), -1, 0, false})),
        std::make_pair("something_1920_1080.yuv", ExpectedFileFormat({Size(0, 0), -1, 0, false})),
        std::make_pair("something_19201080.yuv", ExpectedFileFormat({Size(0, 0), -1, 0, false})),
        std::make_pair("something_1280-720.yuv", ExpectedFileFormat({Size(0, 0), -1, 0, false})),
        std::make_pair("something_1920-1080_something.yuv",
                       ExpectedFileFormat({Size(0, 0), -1, 0, false})),

        std::make_pair("something_1920x1080_25.yuv",
                       ExpectedFileFormat({Size(1920, 1080), 25, 0, false})),
        std::make_pair("something_1920x1080_999.yuv",
                       ExpectedFileFormat({Size(1920, 1080), 999, 0, false})),
        std::make_pair("something_1920x1080_60Hz.yuv",
                       ExpectedFileFormat({Size(1920, 1080), 60, 0, false})),
        std::make_pair("something_1920x1080_999_something.yuv",
                       ExpectedFileFormat({Size(1920, 1080), 999, 0, false})),
        std::make_pair("something_1920x1080_60hz.yuv",
                       ExpectedFileFormat({Size(1920, 1080), 60, 0, false})),
        std::make_pair("something_1920x1080_60HZ.yuv",
                       ExpectedFileFormat({Size(1920, 1080), 60, 0, false})),
        std::make_pair("something_1920x1080_60fps.yuv",
                       ExpectedFileFormat({Size(1920, 1080), 60, 0, false})),
        std::make_pair("something_1920x1080_60FPS.yuv",
                       ExpectedFileFormat({Size(1920, 1080), 60, 0, false})),

        std::make_pair("something_1920x1080_25_8.yuv",
                       ExpectedFileFormat({Size(1920, 1080), 25, 8, false})),
        std::make_pair("something_1920x1080_25_12.yuv",
                       ExpectedFileFormat({Size(1920, 1080), 25, 12, false})),
        std::make_pair("something_1920x1080_25_8b.yuv",
                       ExpectedFileFormat({Size(1920, 1080), 25, 8, false})),
        std::make_pair("something_1920x1080_25_8b_something.yuv",
                       ExpectedFileFormat({Size(1920, 1080), 25, 8, false})),

        std::make_pair("something1080p.yuv", ExpectedFileFormat({Size(1920, 1080), -1, 0, false})),
        std::make_pair("something1080pSomething.yuv",
                       ExpectedFileFormat({Size(1920, 1080), -1, 0, false})),
        std::make_pair("something1080p33.yuv",
                       ExpectedFileFormat({Size(1920, 1080), 33, 0, false})),
        std::make_pair("something1080p33Something.yuv",
                       ExpectedFileFormat({Size(1920, 1080), 33, 0, false})),
        std::make_pair("something720p.yuv", ExpectedFileFormat({Size(1280, 720), -1, 0, false})),
        std::make_pair("something720pSomething.yuv",
                       ExpectedFileFormat({Size(1280, 720), -1, 0, false})),
        std::make_pair("something720p44.yuv", ExpectedFileFormat({Size(1280, 720), 44, 0, false})),
        std::make_pair("something720p44Something.yuv",
                       ExpectedFileFormat({Size(1280, 720), 44, 0, false})),

        std::make_pair("something_cif.yuv", ExpectedFileFormat({Size(352, 288), -1, 0, false})),
        std::make_pair("something_cifSomething.yuv",
                       ExpectedFileFormat({Size(352, 288), -1, 0, false})),
        std::make_pair("something_qcif.yuv", ExpectedFileFormat({Size(176, 144), -1, 0, false})),
        std::make_pair("something_qcifSomething.yuv",
                       ExpectedFileFormat({Size(176, 144), -1, 0, false})),
        std::make_pair("something_4cif.yuv", ExpectedFileFormat({Size(704, 576), -1, 0, false})),
        std::make_pair("something_4cifSomething.yuv",
                       ExpectedFileFormat({Size(704, 576), -1, 0, false})),
        std::make_pair("somethingUHDSomething.yuv",
                       ExpectedFileFormat({Size(3840, 2160), -1, 0, false})),
        std::make_pair("somethingHDSomething.yuv",
                       ExpectedFileFormat({Size(1920, 1080), -1, 0, false})),

        std::make_pair("something_1920x1080_8Bit.yuv",
                       ExpectedFileFormat({Size(1920, 1080), -1, 8, false})),
        std::make_pair("something_1920x1080_10Bit.yuv",
                       ExpectedFileFormat({Size(1920, 1080), -1, 10, false})),
        std::make_pair("something_1920x1080_12Bit.yuv",
                       ExpectedFileFormat({Size(1920, 1080), -1, 12, false})),
        std::make_pair("something_1920x1080_16Bit.yuv",
                       ExpectedFileFormat({Size(1920, 1080), -1, 16, false})),
        std::make_pair("something_1920x1080_8bit.yuv",
                       ExpectedFileFormat({Size(1920, 1080), -1, 8, false})),
        std::make_pair("something_1920x1080_8BIT.yuv",
                       ExpectedFileFormat({Size(1920, 1080), -1, 8, false})),
        std::make_pair("something_1920x1080_8-Bit.yuv",
                       ExpectedFileFormat({Size(1920, 1080), -1, 8, false})),
        std::make_pair("something_1920x1080_8-BIT.yuv",
                       ExpectedFileFormat({Size(1920, 1080), -1, 8, false})),

        std::make_pair("something_1920x1080_packed.yuv",
                       ExpectedFileFormat({Size(1920, 1080), -1, 0, true})),
        std::make_pair("something_1920x1080_packed-something.yuv",
                       ExpectedFileFormat({Size(1920, 1080), -1, 0, true})),
        std::make_pair("something_1920x1080packed.yuv",
                       ExpectedFileFormat({Size(1920, 1080), -1, 0, false})),
        std::make_pair("packed_something_1920x1080.yuv",
                       ExpectedFileFormat({Size(1920, 1080), -1, 0, false})),

        std::make_pair("sample_1280x720_16bit_444_packed_20200109_114812.yuv",
                       ExpectedFileFormat({Size(1280, 720), -1, 16u, true})),
        std::make_pair("sample_1280x720_16b_yuv44416le_packed_20200109_114812.yuv",
                       ExpectedFileFormat({Size(1280, 720), -1, 16u, true})),
        std::make_pair("sample_1280x720_16b_yuv16le_packed_444_20200109_114812",
                       ExpectedFileFormat({Size(1280, 720), -1, 16u, true}))

            ),
    getTestName);

} // namespace
