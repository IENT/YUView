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

#include "CheckFunctions.h"

#include <TemporaryFile.h>
#include <statistics/StatisticsFileCSV.h>

namespace
{

ByteVector getCSVTestData()
{
  const std::string stats_str =
      R"(%;syntax-version;v1.2
%;seq-specs;BasketballDrive_L1_1920x1080_50_encoder+randomaccess+main+B+2x_FTBE9_IBD08_IBD18_IBD08_IBD18_IP48_QPL1022_SEIDPH0_stats;0;1920;1080;0;
%;type;9;MVDL0;vector;
%;vectorColor;100;0;0;255
%;scaleFactor;4
%;type;10;MVDL1;vector;
%;vectorColor;0;100;0;255
%;scaleFactor;4
%;type;11;MVL0;vector;
%;vectorColor;200;0;0;255
%;scaleFactor;4
%;type;12;MVL1;vector;
%;vectorColor;0;200;0;255
%;scaleFactor;4
%;type;7;MVPIdxL0;range;
%;defaultRange;0;1;jet
%;gridColor;255;255;255;
%;type;8;MVPIdxL1;range;
%;defaultRange;0;1;jet
%;gridColor;255;255;255;
%;type;5;MergeIdxL0;range;
%;defaultRange;0;5;jet
%;gridColor;255;255;255;
%;type;6;MergeIdxL1;range;
%;defaultRange;0;5;jet
%;gridColor;255;255;255;
%;type;0;PredMode;range;
%;defaultRange;0;1;jet
%;type;3;RefFrmIdxL0;range;
%;defaultRange;0;3;jet
%;gridColor;255;255;255;
%;type;4;RefFrmIdxL1;range;
%;defaultRange;0;3;jet
%;gridColor;255;255;255;
%;type;1;Skipflag;range;
%;defaultRange;0;1;jet
1;0;32;8;16;9;1;0
1;8;32;8;16;9;0;0
1;112;56;4;8;9;0;0
1;116;56;4;8;9;0;0
1;128;32;32;16;9;0;0
1;128;48;32;16;9;0;0
7;0;32;8;16;3;1
7;128;48;32;16;3;0
7;384;0;64;64;3;0
7;520;32;24;32;3;0
7;576;40;32;24;3;0
1;0;32;8;16;11;31;0
1;8;32;8;16;11;-33;0
1;112;56;4;8;11;-30;0
1;116;56;4;8;11;-30;0
1;128;32;32;16;11;-31;0
1;128;48;32;16;11;-31;0
1;160;32;32;16;11;-31;0
)";

  ByteVector data(stats_str.begin(), stats_str.end());
  return data;
}

TEST(StatisticsFileCSV, testCSVFileParsing)
{
  yuviewTest::TemporaryFile csvFile(getCSVTestData());

  stats::StatisticsData    statData;
  stats::StatisticsFileCSV statFile(QString::fromStdString(csvFile.getFilePathString()), statData);

  EXPECT_EQ(statData.getFrameSize(), Size(1920, 1080));

  const auto types = statData.getStatisticsTypes();
  EXPECT_EQ(types.size(), size_t(12));

  // Code on how to generate the lists:

  // QString typeIDs;
  // QString names;
  // QString description;
  // QString vectorColors;
  // QString vectorScaleFactors;
  // QString valueMins;
  // QString valueMaxs;
  // QString valueComplexTypes;
  // QString valueGridColors;

  // for (unsigned i = 0; i < 12; i++)
  // {
  //   const auto &t = types[i];
  //   typeIDs += QString(", %1").arg(t.typeID);
  //   names += QString(", %1").arg(t.typeName);
  //   description += QString(", %1").arg(t.description);

  //   vectorColors += ", ";
  //   vectorScaleFactors += ", ";
  //   if (t.hasVectorData)
  //   {
  //     vectorColors += QString::fromStdString(t.vectorStyle.color.toHex());
  //     vectorScaleFactors += QString("%1").arg(t.vectorScale);
  //   }

  //   valueMins += ", ";
  //   valueMaxs += ", ";
  //   valueComplexTypes += ", ";
  //   valueGridColors += ", ";
  //   if (t.hasValueData)
  //   {
  //     valueMins += QString("%1").arg(t.colorMapper.rangeMin);
  //     valueMaxs += QString("%1").arg(t.colorMapper.rangeMax);
  //     valueComplexTypes += t.colorMapper.complexType;
  //     valueGridColors += QString::fromStdString(t.gridStyle.color.toHex());
  //   }
  // }

  // std::cout << "typeIDs: " << typeIDs.toStdString() << "\n";
  // std::cout << "names: " << names.toStdString() << "\n";
  // std::cout << "description: " << description.toStdString() << "\n";
  // std::cout << "vectorColors: " << vectorColors.toStdString() << "\n";
  // std::cout << "vectorScaleFactors: " << vectorScaleFactors.toStdString() << "\n";
  // std::cout << "valueMins: " << valueMins.toStdString() << "\n";
  // std::cout << "valueMaxs: " << valueMaxs.toStdString() << "\n";
  // std::cout << "valueComplexTypes: " << valueComplexTypes.toStdString() << "\n";
  // std::cout << "valueGridColors: " << valueGridColors.toStdString() << "\n";

  const auto typeIDs       = std::vector<int>({9, 10, 11, 12, 7, 8, 5, 6, 0, 3, 4, 1});
  const auto typeNameNames = std::vector<QString>({"MVDL0",
                                                   "MVDL1",
                                                   "MVL0",
                                                   "MVL1",
                                                   "MVPIdxL0",
                                                   "MVPIdxL1",
                                                   "MergeIdxL0",
                                                   "MergeIdxL1",
                                                   "PredMode",
                                                   "RefFrmIdxL0",
                                                   "RefFrmIdxL1",
                                                   "Skipflag"});
  const auto vectorColors  = std::vector<std::string>(
      {"#640000", "#006400", "#c80000", "#00c800", "", "", "", "", "", "", "", ""});
  const auto vectorScaleFactors = std::vector<int>({4, 4, 4, 4, -1, -1, -1, -1, -1, -1, -1, -1});
  const auto valueColorRangeMin = std::vector<int>({-1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0});
  const auto valueColorRangeMax = std::vector<int>({-1, -1, -1, -1, 1, 1, 5, 5, 1, 3, 3, 1});
  const auto valueGridColors    = std::vector<std::string>({"",
                                                            "",
                                                            "",
                                                            "",
                                                            "#ffffff",
                                                            "#ffffff",
                                                            "#ffffff",
                                                            "#ffffff",
                                                            "#000000",
                                                            "#ffffff",
                                                            "#ffffff",
                                                            "#000000"});

  for (int i = 0; i < 12; i++)
  {
    const auto &t = types[i];

    EXPECT_EQ(t.typeID, typeIDs[i]);
    EXPECT_EQ(t.typeName, typeNameNames[i]);
    if (t.hasVectorData)
    {
      EXPECT_EQ(t.vectorStyle.color.toHex(), vectorColors[i]);
      EXPECT_EQ(t.vectorScale, vectorScaleFactors[i]);
    }
    if (t.hasValueData)
    {
      EXPECT_EQ(t.colorMapper.valueRange.min, valueColorRangeMin[i]);
      EXPECT_EQ(t.colorMapper.valueRange.max, valueColorRangeMax[i]);
      EXPECT_EQ(t.colorMapper.predefinedType, stats::color::PredefinedType::Jet);
      EXPECT_EQ(t.gridStyle.color.toHex(), valueGridColors[i]);
    }
  }

  // We did not let the file parse the positions of the start of each poc/type yet so loading should
  // not yield any data yet.
  statFile.loadStatisticData(statData, 1, 9);
  EXPECT_EQ(statData.getFrameIndex(), 1);
  {
    const auto &frameData = statData[9];
    EXPECT_EQ(frameData.vectorData.size(), size_t(0));
    EXPECT_EQ(frameData.valueData.size(), size_t(0));
  }

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

  statFile.loadStatisticData(statData, 7, 3);
  EXPECT_EQ(statData.getFrameIndex(), 7);
  EXPECT_EQ(statData[3].vectorData.size(), size_t(0));
  yuviewTest::statistics::checkValueList(statData[3].valueData,
                                         {{0, 32, 8, 16, 1},
                                          {128, 48, 32, 16, 0},
                                          {384, 0, 64, 64, 0},
                                          {520, 32, 24, 32, 0},
                                          {576, 40, 32, 24, 0}});
}

} // namespace