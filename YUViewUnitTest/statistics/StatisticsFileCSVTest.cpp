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

#include <common/PrettyPrinting/Statistics.h>

#include "CheckFunctions.h"
#include "StatisticsFileCSVTestData.h"

#include <TemporaryFile.h>
#include <statistics/ColorMapper.h>
#include <statistics/StatisticsFileCSV.h>
#include <statistics/StatisticsTypeBuilder.h>

namespace stats::test
{

using stats::color::ColorMapper;

struct TestParameters
{
  std::string                   testName;
  std::string                   csv;
  std::optional<StatisticsType> expectedType;
};

using Tc = TestParameters;

class TestParsingOfCSVTypesFormatV12 : public TestWithParam<TestParameters>
{
};

std::string getTestName(const testing::TestParamInfo<TestParameters> &TestCaseInfo)
{
  return TestCaseInfo.param.testName;
}

TEST_P(TestParsingOfCSVTypesFormatV12, TestParsing)
{
  const auto &parameters = GetParam();

  const auto csvTestData = "%;syntax-version;v1.2\n"
                           "%;seq-specs;SequenceName;0;1920;1080;0;\n" +
                           parameters.csv;

  yuviewTest::TemporaryFile csvFile(csvTestData);

  stats::StatisticsData    statData;
  stats::StatisticsFileCSV statFile(csvFile.getFilePathString(), statData);

  const auto types = statData.getStatisticsTypes();

  ASSERT_EQ(types.size(), (parameters.expectedType ? 1u : 0u));
  if (parameters.expectedType)
    EXPECT_EQ(types.at(0), parameters.expectedType);
}

INSTANTIATE_TEST_SUITE_P(
  StatisticsFileCSVTest,
  TestParsingOfCSVTypesFormatV12,
  Values(

    // Range with a custom range (min/max values and min/max color)
    Tc({.testName = "RangeTypeWithCustomRange_ShouldParse",
        .csv      = "%;type;0;RangeTypeColor;range;\n"
                    "%;range;12;24;126;77;127;78;128;79;129;80\n",
        .expectedType =
          StatisticsTypeBuilder(0, "RangeTypeColor")
            .withValueDataOptions({.colorMapper = color::ColorMapper(
                                     {12, 24}, Color(126, 127, 128, 129), Color(77, 78, 79, 80))})
            .build()}),
    Tc({.testName = "RangeTypeWithCustomRange_NegativeRange_ShouldParse",
        .csv      = "%;type;0;RangeTypeColor;range;\n"
                    "%;range;-12;-7;126;77;127;78;128;79;129;80\n",
        .expectedType =
          StatisticsTypeBuilder(0, "RangeTypeColor")
            .withValueDataOptions({.colorMapper = color::ColorMapper(
                                     {-12, -7}, Color(126, 127, 128, 129), Color(77, 78, 79, 80))})
            .build()}),
    Tc({.testName = "RangeTypeWithCustomRange_BackwardsRange_ShouldParse",
        .csv      = "%;type;0;RangeTypeColor;range;\n"
                    "%;range;30;10;126;77;127;78;128;79;129;80\n",
        .expectedType =
          StatisticsTypeBuilder(0, "RangeTypeColor")
            .withValueDataOptions({.colorMapper = color::ColorMapper(
                                     {30, 10}, Color(126, 127, 128, 129), Color(77, 78, 79, 80))})
            .build()}),
    Tc({.testName = "RangeTypeWithCustomRange_WithGridColor_ShouldParse",
        .csv      = "%;type;0;RangeTypeColor;range;\n"
                    "%;range;30;10;126;77;127;78;128;79;129;80\n"
                    "%;gridColor;123;124;125;\n",
        .expectedType =
          StatisticsTypeBuilder(0, "RangeTypeColor")
            .withValueDataOptions({.colorMapper = color::ColorMapper(
                                     {30, 10}, Color(126, 127, 128, 129), Color(77, 78, 79, 80))})
            .withGridOptions({.style = LineDrawStyle({.color = Color(123, 124, 125)})})
            .build()}),
    Tc({.testName = "RangeTypeWithCustomRange_WithScaleFactor_ScaleFactorShouldNotBeParsed",
        .csv      = "%;type;0;RangeTypeColor;range;\n"
                    "%;range;12;24;126;77;127;78;128;79;129;80\n"
                    "%;scaleFactor;11\n",
        .expectedType =
          StatisticsTypeBuilder(0, "RangeTypeColor")
            .withValueDataOptions({.colorMapper = color::ColorMapper(
                                     {12, 24}, Color(126, 127, 128, 129), Color(77, 78, 79, 80))})
            .build()}),
    Tc({.testName = "RangeTypeWithCustomRange_WithSacleToBlockSize_ShouldParse",
        .csv      = "%;type;0;RangeTypeColor;range;\n"
                    "%;range;12;24;126;77;127;78;128;79;129;80\n"
                    "%;scaleToBlockSize;1\n",
        .expectedType =
          StatisticsTypeBuilder(0, "RangeTypeColor")
            .withValueDataOptions({.scaleToBlockSize = true,
                                   .colorMapper      = color::ColorMapper(
                                     {12, 24}, Color(126, 127, 128, 129), Color(77, 78, 79, 80))})
            .build()}),
    Tc({.testName = "RangeTypeWithCustomRange_OutOfRangeRGBValues_ShouldBeClippedTo0To255",
        .csv      = "%;type;0;RangeTypeColor;range;\n"
                    "%;range;12;24;-792;-1;0;1;128;255;256;812372\n",
        .expectedType =
          StatisticsTypeBuilder(0, "RangeTypeColor")
            .withValueDataOptions({.colorMapper = color::ColorMapper(
                                     {12, 24}, Color(0, 0, 128, 255), Color(0, 1, 255, 255))})
            .build()}),
    Tc({.testName     = "RangeTypeWithCustomRange_InvalidRGBValues_ShouldNotAddType",
        .csv          = "%;type;0;RangeTypeColor;range;\n"
                        "%;range;12;AA;-792;-1;0;1;128;255;256;812372\n",
        .expectedType = {}}),
    Tc({.testName = "RangeTypeWithCustomRange_WithTwoRanges_SecondShouldOverrideFirstOne",
        .csv      = "%;type;0;RangeTypeColor;range;\n"
                    "%;range;0;7;122;123;124;78;82;11;234;22\n"
                    "%;range;30;10;126;77;127;78;128;79;129;80\n",
        .expectedType =
          StatisticsTypeBuilder(0, "RangeTypeColor")
            .withValueDataOptions({.colorMapper = color::ColorMapper(
                                     {30, 10}, Color(126, 127, 128, 129), Color(77, 78, 79, 80))})
            .build()}),

    // Range with default color mapper (e.g. jet)
    Tc({.testName     = "RangeTypeWithDefaultRange_ShouldParse",
        .csv          = "%;type;0;RangeTypeColor;range;\n"
                        "%;defaultRange;0;11;jet\n",
        .expectedType = StatisticsTypeBuilder(0, "RangeTypeColor")
                          .withValueDataOptions({.colorMapper = color::ColorMapper(
                                                   {0, 11}, color::PredefinedType::Jet)})
                          .build()}),
    Tc({.testName     = "RangeTypeWithDefaultRange_WithUpperCase_ShouldParse",
        .csv          = "%;type;0;RangeTypeColor;range;\n"
                        "%;defaultRange;0;11;jET\n",
        .expectedType = StatisticsTypeBuilder(0, "RangeTypeColor")
                          .withValueDataOptions({.colorMapper = color::ColorMapper(
                                                   {0, 11}, color::PredefinedType::Jet)})
                          .build()}),
    Tc({.testName     = "RangeTypeWithDefaultRange_WithTwoRangeDefinitions_"
                        "SecondOneShouldOverrideFirstOne",
        .csv          = "%;type;0;RangeTypeColor;range;\n"
                        "%;defaultRange;29;119;jET\n"
                        "%;defaultRange;0;11;Winter\n",
        .expectedType = StatisticsTypeBuilder(0, "RangeTypeColor")
                          .withValueDataOptions({.colorMapper = color::ColorMapper(
                                                   {0, 11}, color::PredefinedType::Winter)})
                          .build()}),

    // Range type with wrong options or none at all
    Tc({.testName     = "RangeType_WithMapColor_ShouldNotAddType",
        .csv          = "%;type;0;RangeTypeColor;range;\n"
                        "%;mapColor;12;124;125;126;127\n",
        .expectedType = {}}),
    Tc({.testName     = "RangeType_WithVectorColor_ShouldNotAddType",
        .csv          = "%;type;0;RangeTypeColor;range;\n"
                        "%;vectorColor;22;23;24;25\n",
        .expectedType = {}}),
    Tc({.testName     = "RangeType_WithNoOptions_ShouldNotAddType",
        .csv          = "%;type;0;RangeTypeColor;range;\n",
        .expectedType = {}}),

    // Mapping type
    Tc({.testName     = "MapType_WithSingleMapColor_ShouldParse",
        .csv          = "%;type;1;MapType;map;\n"
                        "%;mapColor;12;124;125;126;127\n",
        .expectedType = StatisticsTypeBuilder(1, "MapType")
                          .withValueDataOptions(
                            {.colorMapper = color::ColorMapper({{12, Color(124, 125, 126, 127)}})})
                          .build()}),
    Tc({.testName     = "MapType_WithMultipleMapColor_ShouldParse",
        .csv          = "%;type;1;MapType;map;\n"
                        "%;mapColor;1;124;125;126;127\n"
                        "%;mapColor;2;22;23;24;25\n"
                        "%;mapColor;9;77;78;79;80\n",
        .expectedType = StatisticsTypeBuilder(1, "MapType")
                          .withValueDataOptions(
                            {.colorMapper = color::ColorMapper({{1, Color(124, 125, 126, 127)},
                                                                {2, Color(22, 23, 24, 25)},
                                                                {9, Color(77, 78, 79, 80)}})})
                          .build()}),
    Tc({.testName     = "MapType_WithNegativeMapIndex_ShouldParse",
        .csv          = "%;type;1;MapType;map;\n"
                        "%;mapColor;-56;124;125;126;127\n",
        .expectedType = StatisticsTypeBuilder(1, "MapType")
                          .withValueDataOptions(
                            {.colorMapper = color::ColorMapper({{-56, Color(124, 125, 126, 127)}})})
                          .build()}),
    Tc(
      {.testName     = "MapType_WithGridColor_ShouldParse",
       .csv          = "%;type;1;MapType;map;\n"
                       "%;mapColor;12;124;125;126;127\n"
                       "%;gridColor;123;124;125;\n",
       .expectedType = StatisticsTypeBuilder(1, "MapType")
                         .withValueDataOptions(
                           {.colorMapper = color::ColorMapper({{12, Color(124, 125, 126, 127)}})})
                         .withGridOptions({.style = LineDrawStyle({.color = Color(123, 124, 125)})})
                         .build()}),
    Tc({.testName     = "MapType_WithScaleFactor_ScaleFactorShouldNotBeParsed",
        .csv          = "%;type;1;MapType;map;\n"
                        "%;mapColor;12;124;125;126;127\n"
                        "%;scaleFactor;11\n",
        .expectedType = StatisticsTypeBuilder(1, "MapType")
                          .withValueDataOptions(
                            {.colorMapper = color::ColorMapper({{12, Color(124, 125, 126, 127)}})})
                          .build()}),
    Tc({.testName     = "MapType_WithSacleToBlockSize_ShouldParse",
        .csv          = "%;type;1;MapType;map;\n"
                        "%;mapColor;12;124;125;126;127\n"
                        "%;scaleToBlockSize;1\n",
        .expectedType = StatisticsTypeBuilder(1, "MapType")
                          .withValueDataOptions(
                            {.scaleToBlockSize = true,
                             .colorMapper = color::ColorMapper({{12, Color(124, 125, 126, 127)}})})
                          .build()}),
    Tc({.testName     = "MapType_OutOfRangeRGBValues_ShouldBeClippedTo0To255",
        .csv          = "%;type;1;MapType;map;\n"
                        "%;mapColor;12;-77;-1;0;1\n"
                        "%;mapColor;13;255;256;257;81239842\n",
        .expectedType = StatisticsTypeBuilder(1, "MapType")
                          .withValueDataOptions(
                            {.colorMapper = color::ColorMapper({{12, Color(0, 0, 0, 1)},
                                                                {13, Color(255, 255, 255, 255)}})})
                          .build()}),
    Tc({.testName = "MapType_WithSuplicateMapEntries_SecondShouldOverrideFirstOne",
        .csv      = "%;type;1;MapType;map;\n"
                    "%;mapColor;12;124;125;126;127\n"
                    "%;mapColor;12;88;2;1;90\n",
        .expectedType =
          StatisticsTypeBuilder(1, "MapType")
            .withValueDataOptions({.colorMapper = color::ColorMapper({{12, Color(88, 2, 1, 90)}})})
            .build()}),

    // Mapping type with wrong data
    Tc({.testName     = "MapType_InvalidRGBValues_ShouldNotAddType",
        .csv          = "%;type;1;MapType;map;\n"
                        "%;mapColor;FF;-77;-1;0;1\n",
        .expectedType = {}}),
    Tc({.testName     = "MapType_WithRangeColor_ShouldNotAddType",
        .csv          = "%;type;0;MapType;map;\n"
                        "%;range;12;24;126;77;127;78;128;79;129;80\n",
        .expectedType = {}}),
    Tc({.testName     = "MapType_WithDefaultRange_ShouldNotAddType",
        .csv          = "%;type;0;MapType;map;\n"
                        "%;defaultRange;0;11;jet\n",
        .expectedType = {}}),
    Tc({.testName     = "MapType_WithVectorColor_ShouldNotAddType",
        .csv          = "%;type;0;MapType;map;\n"
                        "%;vectorColor;22;23;24;25\n",
        .expectedType = {}}),
    Tc({.testName     = "MapType_WithNoOptions_ShouldNotAddType",
        .csv          = "%;type;0;MapType;map;\n",
        .expectedType = {}}),
    Tc({.testName     = "MapType_NoMapColor_ShouldNotAddType",
        .csv          = "%;type;1;MapType;map;\n",
        .expectedType = {}}),

    // Vector type
    Tc({.testName     = "VectorType_Default_ShouldParse",
        .csv          = "%;type;2;VectorType;vector;\n",
        .expectedType = StatisticsTypeBuilder(2, "VectorType").withVectorDataOptions({}).build()})

      ),

  getTestName);

// TEST(StatisticsFileCSVTest,
// loadingFromTestData1_ShouldLoadFramesizeAndTypesAndFileSortingCorrectly)
// {
//   yuviewTest::TemporaryFile csvFile(getCSVTestData1());

//   stats::StatisticsData    statData;
//   stats::StatisticsFileCSV statFile(csvFile.getFilePathString(), statData);

//   EXPECT_EQ(statData.getFrameSize(), Size(1920, 1080));

//   const auto types = statData.getStatisticsTypes();

//   const StatisticsTypesVec expectedTypes = {
//       StatisticsTypeBuilder(0, "RangeTypeColor")
//           .withValueDataOptions({.colorMapper = color::ColorMapper(
//                                      {12, 24}, Color(126, 127, 128, 129), Color(77, 78, 79,
//                                      80))})
//           .build(),
//       StatisticsTypeBuilder(1, "RangeTypeDefault")
//           .withValueDataOptions({.colorMapper = color::ColorMapper({0, 50},
//           "someRangeNameEGJet")}) .build(),
//       StatisticsTypeBuilder(2, "RangeTypeDefaultWithGridColor")
//           .withValueDataOptions({.colorMapper = color::ColorMapper({17, 22},
//           "someRangeNameEGJet")}) .withGridOptions({.style = LineDrawStyle({.color = Color(123,
//           124, 125)})}) .build(),
//       StatisticsTypeBuilder(3, "MapType")
//           .withValueDataOptions(
//               {.colorMapper = color::ColorMapper(
//                    {{12, Color(124, 125, 126, 127)}, {15, Color(224, 225, 226, 227)}},
//                    Color())})
//           .build(),
//       StatisticsTypeBuilder(4, "VectorDefault").withVectorDataOptions({}).build(),
//       StatisticsTypeBuilder(5, "VectorWithColor")
//           .withVectorDataOptions({.style = LineDrawStyle({.color = Color(22, 23, 24)})})
//           .build(),
//       StatisticsTypeBuilder(6, "VectorWithScaleFactor").withVectorDataOptions({.scale =
//       4}).build(), StatisticsTypeBuilder(7, "VectorWithScaleToBlockSize")
//           .withVectorDataOptions({.scaleToBlockSize = true})
//           .build()};

//   EXPECT_EQ(types, expectedTypes);
// }

TEST(StatisticsFileCSVTest, loadingFromTestData1_testStateBeforeLoadingData_ShouldHaveNo)
{
  yuviewTest::TemporaryFile csvFile(getCSVTestData1());

  stats::StatisticsData    statData;
  stats::StatisticsFileCSV statFile(csvFile.getFilePathString(), statData);

  EXPECT_EQ(statFile.getParsingInfo().fileSorting,
            StatisticsFileBase::ParsingInfo::FileSorting::Unknown)
    << "At this point (before reading frame and type positions from file) this is unknown";

  // We did not let the file parse the positions of the start of each poc/type yet so loading
  // should not yield any data yet.
  statFile.loadStatisticData(statData, 1, 9);
  EXPECT_EQ(statData.getFrameIndex(), 1);
  {
    const auto &frameData = statData[9];
    EXPECT_EQ(frameData.vectorData.size(), size_t(0));
    EXPECT_EQ(frameData.valueData.size(), size_t(0));
  }

  EXPECT_EQ(statFile.getParsingInfo().fileSorting,
            StatisticsFileBase::ParsingInfo::FileSorting::Unknown);

  std::atomic_bool breakAtomic;
  breakAtomic.store(false);
  statFile.readFrameAndTypePositionsFromFile(std::ref(breakAtomic));
}

TEST(StatisticsFileCSVTest, loadingFromTestData1_ShouldLoadTypesCorrectly)
{
  yuviewTest::TemporaryFile csvFile(getCSVTestData1());

  stats::StatisticsData    statData;
  stats::StatisticsFileCSV statFile(csvFile.getFilePathString(), statData);

  EXPECT_EQ(statFile.getParsingInfo().fileSorting,
            StatisticsFileBase::ParsingInfo::FileSorting::Unknown)
    << "At this point (before reading frame and type positions from file) this is unknown";

  // We did not let the file parse the positions of the start of each poc/type yet so loading
  // should not yield any data yet.
  statFile.loadStatisticData(statData, 1, 9);
  EXPECT_EQ(statData.getFrameIndex(), 1);
  {
    const auto &frameData = statData[9];
    EXPECT_EQ(frameData.vectorData.size(), size_t(0));
    EXPECT_EQ(frameData.valueData.size(), size_t(0));
  }

  EXPECT_EQ(statFile.getParsingInfo().fileSorting,
            StatisticsFileBase::ParsingInfo::FileSorting::Unknown);

  std::atomic_bool breakAtomic;
  breakAtomic.store(false);
  statFile.readFrameAndTypePositionsFromFile(std::ref(breakAtomic));

  // Now we should get the data
  statFile.loadStatisticData(statData, 1, 9);
  EXPECT_EQ(statData.getFrameIndex(), 1);
  yuviewTest::statistics::checkVectorList(statData[9].vectorData,
                                          {{0, 32, 8, 16, 1, 0},
                                           {8, 32, 8, 16, 0, 0},
                                           {112, 56, 4, 8, 0, 0},
                                           {116, 56, 4, 8, 0, 0},
                                           {128, 32, 32, 16, 0, 0},
                                           {128, 48, 32, 16, 0, 0}});
  EXPECT_EQ(statData[9].valueData.size(), size_t(0));
  EXPECT_EQ(statFile.getParsingInfo().fileSorting,
            StatisticsFileBase::ParsingInfo::FileSorting::SortedByType);

  statFile.loadStatisticData(statData, 1, 11);
  EXPECT_EQ(statData.getFrameIndex(), 1);
  yuviewTest::statistics::checkVectorList(statData[11].vectorData,
                                          {{0, 32, 8, 16, 31, 0},
                                           {8, 32, 8, 16, -33, 0},
                                           {112, 56, 4, 8, -30, 0},
                                           {116, 56, 4, 8, -30, 0},
                                           {128, 32, 32, 16, -31, 0},
                                           {128, 48, 32, 16, -31, 0},
                                           {160, 32, 32, 16, -31, 0}});
  EXPECT_EQ(statData[11].valueData.size(), size_t(0));
  EXPECT_EQ(statFile.getParsingInfo().fileSorting,
            StatisticsFileBase::ParsingInfo::FileSorting::SortedByType);

  statFile.loadStatisticData(statData, 7, 3);
  EXPECT_EQ(statData.getFrameIndex(), 7);
  EXPECT_EQ(statData[3].vectorData.size(), size_t(0));
  yuviewTest::statistics::checkValueList(statData[3].valueData,
                                         {{0, 32, 8, 16, 1},
                                          {128, 48, 32, 16, 0},
                                          {384, 0, 64, 64, 0},
                                          {520, 32, 24, 32, 0},
                                          {576, 40, 32, 24, 0}});
  EXPECT_EQ(statFile.getParsingInfo().fileSorting,
            StatisticsFileBase::ParsingInfo::FileSorting::SortedByType);
}

TEST(StatisticsFileCSVTest, testCSVFileParsingRealFile_shouldLoadDataCorrectly)
{
  yuviewTest::TemporaryFile csvFile(getCSVTestData2());

  stats::StatisticsData    statData;
  stats::StatisticsFileCSV statFile(csvFile.getFilePathString(), statData);

  EXPECT_EQ(statData.getFrameSize(), Size(832, 480));

  std::atomic_bool breakAtomic;
  breakAtomic.store(false);
  statFile.readFrameAndTypePositionsFromFile(std::ref(breakAtomic));

  const StatisticsTypesVec expectedTypes = {
    StatisticsTypeBuilder(0, "PredictionMode")
      .withValueDataOptions(
        {.colorMapper = ColorMapper(
           {{0, Color(0, 0, 255)}, {1, Color(255, 0, 255)}, {2, Color(0, 255, 255)}}, {})})
      .build(),
    StatisticsTypeBuilder(1, "MotionVector0")
      .withVectorDataOptions({.style = LineDrawStyle({.color = Color(0, 0, 0)}), .scale = 4})
      .build(),
    StatisticsTypeBuilder(2, "MotionVector1")
      .withVectorDataOptions({.style = LineDrawStyle({.color = Color(0, 0, 0)}), .scale = 4})
      .build()};

  auto &statTypes = statData.getStatisticsTypes();
  EXPECT_EQ(statTypes, expectedTypes);

  const auto FRAME_0 = 0;

  EXPECT_EQ(statData.getTypesThatNeedLoading(FRAME_0).size(), 0u)
    << "As long as no types are set the render, none should need loading.";

  statTypes[0].render = true;
  statTypes[1].render = true;

  {
    const auto typesThatNeedLoading = statData.getTypesThatNeedLoading(FRAME_0);
    EXPECT_EQ(typesThatNeedLoading.size(), 2u)
      << "After types are set to render, they should need loading.";
  }

  statFile.loadStatisticData(statData, FRAME_0, 0);
  EXPECT_EQ(statFile.getParsingInfo().fileSorting,
            StatisticsFileBase::ParsingInfo::FileSorting::SortedByType);

  EXPECT_TRUE(statData.hasDataForTypeID(0))
    << "After loading type 0, it should have data for this type.";
  EXPECT_FALSE(statData.hasDataForTypeID(1))
    << "After loading type 0, it should not have data for type 1 yet.";

  {
    const auto typesThatNeedLoading = statData.getTypesThatNeedLoading(FRAME_0);
    EXPECT_EQ(typesThatNeedLoading.size(), 1u) << "One type was loaded, one was not.";
  }

  statFile.loadStatisticData(statData, FRAME_0, 1);
  EXPECT_TRUE(statData.hasDataForTypeID(1))
    << "After loading type 1, it should have data for this type.";

  {
    const auto typesThatNeedLoading = statData.getTypesThatNeedLoading(FRAME_0);
    EXPECT_EQ(typesThatNeedLoading.size(), 0u)
      << "Both types were loaded, none should need loading.";
  }

  EXPECT_EQ(statData.getFrameIndex(), FRAME_0);
  EXPECT_EQ(statData[0].vectorData.size(), size_t(0));
  yuviewTest::statistics::checkValueListStartsWith(
    statData[0].valueData,
    {{0, 0, 16, 4, 1}, {0, 4, 8, 4, 1}, {0, 8, 8, 4, 1}, {8, 4, 8, 8, 1}, {0, 12, 16, 4, 1}});

  EXPECT_EQ(statData[1].vectorData.size(), size_t(0));
  EXPECT_EQ(statData[1].valueData.size(), size_t(0)) << "There is no motion data for frame 0";

  const auto FRAME_1 = 1;
  statFile.loadStatisticData(statData, FRAME_1, 0);

  int debugStop = 22;
}

} // namespace stats::test
