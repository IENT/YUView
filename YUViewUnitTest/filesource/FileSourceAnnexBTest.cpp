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

#include <TemporaryFile.h>
#include <filesource/FileSourceAnnexBFile.h>

namespace
{

struct TestParameters
{
  int              startCodeLength{};
  int              totalDataLength{};
  std::vector<int> startCodePositions{};
};

using NalSizes = std::vector<int>;
std::pair<NalSizes, ByteVector> generateAnnexBStream(const TestParameters &testParameters)
{
  NalSizes   nalSizes;
  ByteVector data;

  std::optional<int> lastStartPos;
  for (const auto pos : testParameters.startCodePositions)
  {
    unsigned nonStartCodeBytesToAdd;
    if (lastStartPos)
    {
      EXPECT_GT(pos,
                *lastStartPos +
                    testParameters.startCodeLength); // Start codes can not be closer together
      nonStartCodeBytesToAdd = pos - *lastStartPos - testParameters.startCodeLength;
      nalSizes.push_back(nonStartCodeBytesToAdd + testParameters.startCodeLength);
    }
    else
      nonStartCodeBytesToAdd = pos;
    lastStartPos = pos;

    data.insert(data.end(), nonStartCodeBytesToAdd, static_cast<char>(128));

    static constexpr auto START_CODE_4BYTES = {char(0), char(0), char(0), char(1)};
    static constexpr auto START_CODE_3BYTES = {char(0), char(0), char(1)};

    if (testParameters.startCodeLength == 4)
      data.insert(data.end(), START_CODE_4BYTES.begin(), START_CODE_4BYTES.end());
    else
      data.insert(data.end(), START_CODE_3BYTES.begin(), START_CODE_3BYTES.end());
  }

  const auto remainder =
      testParameters.totalDataLength - *lastStartPos - testParameters.startCodeLength;
  nalSizes.push_back(remainder + testParameters.startCodeLength);
  data.insert(data.end(), remainder, char(128));

  return {nalSizes, data};
}

class FileSourceAnnexBTest : public TestWithParam<TestParameters>
{
};

std::string getTestName(const testing::TestParamInfo<TestParameters> &testParam)
{
  const auto testParameters = testParam.param;
  return yuviewTest::formatTestName("StartCodeLength",
                                    testParameters.startCodeLength,
                                    "TotalDataLength",
                                    testParameters.totalDataLength,
                                    "StartCodePositions",
                                    testParameters.startCodePositions);
}

TEST_P(FileSourceAnnexBTest, TestNalUnitParsing)
{
  const auto testParameters = GetParam();

  const auto [nalSizes, data] = generateAnnexBStream(testParameters);
  yuviewTest::TemporaryFile temporaryFile(data);

  FileSourceAnnexBFile annexBFile(QString::fromStdString(temporaryFile.getFilePathString()));
  EXPECT_EQ(static_cast<int>(annexBFile.getNrBytesBeforeFirstNAL()),
            testParameters.startCodePositions.at(0));

  auto nalData = annexBFile.getNextNALUnit();
  int  counter = 0;
  while (nalData.size() > 0)
  {
    EXPECT_EQ(nalSizes.at(counter++), static_cast<int>(nalData.size()));
    nalData = annexBFile.getNextNALUnit();
  }
}

INSTANTIATE_TEST_SUITE_P(
    FilesourceTest,
    FileSourceAnnexBTest,
    Values(TestParameters({3, 10000, {80, 208, 500}}),
           TestParameters({3, 10000, {0, 80, 208, 500}}),
           TestParameters({3, 10000, {1, 80, 208, 500}}),
           TestParameters({3, 10000, {2, 80, 208, 500}}),
           TestParameters({3, 10000, {3, 80, 208, 500}}),
           TestParameters({3, 10000, {4, 80, 208, 500}}),
           TestParameters({3, 10000, {4, 80, 208, 9990}}),
           TestParameters({3, 10000, {4, 80, 208, 9997}}),

           // Test cases where a buffer reload is needed (the buffer is 500k)
           TestParameters({3, 1000000, {80, 208, 500, 50000, 800000}}),

           // The buffer is 500k in size. Test all variations with a start code around this position
           TestParameters({3, 800000, {80, 208, 500, 50000, 499997}}),
           TestParameters({3, 800000, {80, 208, 500, 50000, 499998}}),
           TestParameters({3, 800000, {80, 208, 500, 50000, 499999}}),
           TestParameters({3, 800000, {80, 208, 500, 50000, 500000}}),
           TestParameters({3, 800000, {80, 208, 500, 50000, 500001}}),
           TestParameters({3, 800000, {80, 208, 500, 50000, 500002}}),

           TestParameters({3, 10000, {80, 208, 500, 9995}}),
           TestParameters({3, 10000, {80, 208, 500, 9996}}),
           TestParameters({3, 10000, {80, 208, 500, 9997}})),
    getTestName);

} // namespace
