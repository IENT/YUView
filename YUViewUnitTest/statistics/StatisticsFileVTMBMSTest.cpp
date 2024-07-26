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

#include "gtest/gtest.h"

#include "CheckFunctions.h"

#include <TemporaryFile.h>
#include <statistics/StatisticsFileVTMBMS.h>

namespace
{

ByteVector getVTMBSTestData()
{
  const std::string stats_str =
      R"(# VTMBMS Block Statistics
# Sequence size: [2048x 872]
# Block Statistic Type: PredMode; Integer; [0, 4]
# Block Statistic Type: MVDL0; Vector; Scale: 4
# Block Statistic Type: AffineMVL0; AffineTFVectors; Scale: 4
# Block Statistic Type: GeoPartitioning; Line;
# Block Statistic Type: GeoMVL0; VectorPolygon; Scale: 4
BlockStat: POC 0 @(   0,   0) [64x64] PredMode=1
BlockStat: POC 0 @(  64,   0) [32x16] PredMode=1
BlockStat: POC 0 @( 384,   0) [64x64] PredMode=2
BlockStat: POC 0 @( 520,  32) [16x32] PredMode=4
BlockStat: POC 0 @( 320,   0) [32x24] PredMode=1
BlockStat: POC 0 @( 320,  64) [32x8] PredMode=1
BlockStat: POC 2 @( 384, 128) [64x64] MVDL0={ 12,   3}
BlockStat: POC 2 @( 384, 128) [64x64] PredMode=1
BlockStat: POC 2 @( 448, 128) [64x64] PredMode=2
BlockStat: POC 2 @( 384, 192) [64x64] MVDL0={ 52,   -44}
BlockStat: POC 2 @( 480, 192) [ 8x16] MVDL0={ 520,   888}
BlockStat: POC 2 @( 488, 192) [ 8x16] MVDL0={ -234, -256}
BlockStat: POC 2 @( 496, 192) [16x16] PredMode=2
BlockStat: POC 8 @( 640, 128) [128x128] AffineMVL0={ -61,-128, -53,-101, -99,-139}
BlockStat: POC 8 @(1296, 128) [32x64] AffineMVL0={ -68,  32, -65,  39, -79,  25}
BlockStat: POC 8 @(1328, 128) [16x64] AffineMVL0={ -64,  40, -63,  43, -75,  33}
BlockStat: POC 8 @( 288, 544) [16x32] GeoPartitioning={   5,   0,   6,  32}
BlockStat: POC 8 @(1156, 592) [ 8x 8] GeoPartitioning={   1,   0,   8,   5}
BlockStat: POC 8 @( 276, 672) [ 8x16] GeoPartitioning={   5,   0,   3,  16}
BlockStat: POC 8 @[(240, 384)--(256, 384)--(256, 430)--(240, 416)--] GeoMVL0={ 291, 233}
BlockStat: POC 8 @[(1156, 592)--(1157, 592)--(1164, 597)--(1164, 600)--(1156, 600)--] GeoMVL0={ 152,  24}
BlockStat: POC 8 @[(544, 760)--(545, 768)--(544, 768)--] GeoMVL0={ 180,  38}
)";

  ByteVector data(stats_str.begin(), stats_str.end());
  return data;
}

TEST(StatisticsFileCSV, testCSVFileParsing)
{
  yuviewTest::TemporaryFile vtmbmsFile(getVTMBSTestData());

  stats::StatisticsData       statData;
  stats::StatisticsFileVTMBMS statFile(QString::fromStdString(vtmbmsFile.getFilePathString()),
                                       statData);

  EXPECT_EQ(statData.getFrameSize(), Size(2048, 872));

  auto types = statData.getStatisticsTypes();
  EXPECT_EQ(types.size(), size_t(5));

  EXPECT_EQ(types[0].typeID, 1);
  EXPECT_EQ(types[1].typeID, 2);
  EXPECT_EQ(types[2].typeID, 3);
  EXPECT_EQ(types[3].typeID, 4);
  EXPECT_EQ(types[4].typeID, 5);
  EXPECT_EQ(types[0].typeName, QString("PredMode"));
  EXPECT_EQ(types[1].typeName, QString("MVDL0"));
  EXPECT_EQ(types[2].typeName, QString("AffineMVL0"));
  EXPECT_EQ(types[3].typeName, QString("GeoPartitioning"));
  EXPECT_EQ(types[4].typeName, QString("GeoMVL0"));

  EXPECT_EQ(types[0].hasVectorData, false);
  EXPECT_EQ(types[0].hasValueData, true);
  EXPECT_EQ(types[0].colorMapper.valueRange.min, 0);
  EXPECT_EQ(types[0].colorMapper.valueRange.max, 4);
  EXPECT_EQ(types[0].colorMapper.predefinedType, stats::color::PredefinedType::Jet);
  EXPECT_EQ(types[0].gridStyle.color.toHex(), std::string("#000000"));

  EXPECT_EQ(types[1].hasVectorData, true);
  EXPECT_EQ(types[1].hasValueData, false);
  auto debugName = types[1].vectorStyle.color.toHex();
  EXPECT_EQ(types[1].vectorStyle.color.toHex(), std::string("#ff0000"));
  EXPECT_EQ(types[1].vectorScale, 4);

  EXPECT_EQ(types[2].hasVectorData, false);
  EXPECT_EQ(types[2].hasValueData, false);
  EXPECT_EQ(types[2].hasAffineTFData, true);
  debugName = types[2].vectorStyle.color.toHex();
  EXPECT_EQ(types[2].vectorStyle.color.toHex(), std::string("#ff0000"));
  EXPECT_EQ(types[2].vectorScale, 4);

  EXPECT_EQ(types[3].hasVectorData, true);
  EXPECT_EQ(types[3].hasValueData, false);
  debugName = types[3].vectorStyle.color.toHex();
  EXPECT_EQ(types[3].vectorStyle.color.toHex(), std::string("#ffffff"));
  debugName = types[3].gridStyle.color.toHex();
  EXPECT_EQ(types[3].gridStyle.color.toHex(), std::string("#ffffff"));
  EXPECT_EQ(types[3].vectorScale, 1);

  EXPECT_EQ(types[4].hasVectorData, true);
  EXPECT_EQ(types[4].hasValueData, false);
  debugName = types[4].vectorStyle.color.toHex();
  EXPECT_EQ(types[4].vectorStyle.color.toHex(), std::string("#ff0000"));
  EXPECT_EQ(types[4].vectorScale, 4);
  EXPECT_EQ(types[4].isPolygon, true);

  // We did not let the file parse the positions of the start of each poc/type yet so loading should
  // not yield any data yet.
  statFile.loadStatisticData(statData, 0, 1);
  EXPECT_EQ(statData.getFrameIndex(), 0);
  {
    auto &frameData = statData[9];
    EXPECT_EQ(frameData.vectorData.size(), size_t(0));
    EXPECT_EQ(frameData.valueData.size(), size_t(0));
    EXPECT_EQ(frameData.affineTFData.size(), size_t(0));
    EXPECT_EQ(frameData.polygonValueData.size(), size_t(0));
    EXPECT_EQ(frameData.polygonVectorData.size(), size_t(0));
  }

  std::atomic_bool breakAtomic;
  breakAtomic.store(false);
  statFile.readFrameAndTypePositionsFromFile(std::ref(breakAtomic));

  // Now we should get the data
  statFile.loadStatisticData(statData, 0, 1);
  EXPECT_EQ(statData.getFrameIndex(), 0);
  EXPECT_EQ(statData[2].valueData.size(), size_t(0));
  yuviewTest::statistics::checkValueList(statData[1].valueData,
                                         {{0, 0, 64, 64, 1},
                                          {64, 0, 32, 16, 1},
                                          {384, 0, 64, 64, 2},
                                          {520, 32, 16, 32, 4},
                                          {320, 0, 32, 24, 1},
                                          {320, 64, 32, 8, 1}});
  EXPECT_EQ(statData[1].vectorData.size(), size_t(0));

  statFile.loadStatisticData(statData, 2, 1);
  statFile.loadStatisticData(statData, 2, 2);
  EXPECT_EQ(statData.getFrameIndex(), 2);
  yuviewTest::statistics::checkValueList(
      statData[1].valueData, {{384, 128, 64, 64, 1}, {448, 128, 64, 64, 2}, {496, 192, 16, 16, 2}});
  EXPECT_EQ(statData[1].vectorData.size(), size_t(0));

  yuviewTest::statistics::checkVectorList(statData[2].vectorData,
                                          {{384, 128, 64, 64, 12, 3},
                                           {384, 192, 64, 64, 52, -44},
                                           {480, 192, 8, 16, 520, 888},
                                           {488, 192, 8, 16, -234, -256}});
  EXPECT_EQ(statData[2].valueData.size(), size_t(0));

  statFile.loadStatisticData(statData, 8, 3);
  EXPECT_EQ(statData.getFrameIndex(), 8);
  yuviewTest::statistics::checkAffineTFVectorList(
      statData[3].affineTFData,
      {
          {640, 128, 128, 128, -61, -128, -53, -101, -99, -139},
          {1296, 128, 32, 64, -68, 32, -65, 39, -79, 25},
          {1328, 128, 16, 64, -64, 40, -63, 43, -75, 33},
      });

  statFile.loadStatisticData(statData, 8, 4);
  EXPECT_EQ(statData.getFrameIndex(), 8);
  yuviewTest::statistics::checkLineList(statData[4].vectorData,
                                        {
                                            {288, 544, 16, 32, 5, 0, 6, 32},
                                            {1156, 592, 8, 8, 1, 0, 8, 5},
                                            {276, 672, 8, 16, 5, 0, 3, 16},
                                        });

  statFile.loadStatisticData(statData, 8, 5);
  EXPECT_EQ(statData.getFrameIndex(), 8);
  yuviewTest::statistics::checkPolygonvectorList(
      statData[5].polygonVectorData,
      {
          {291,
           233,
           {240, 256, 256, 240, 0},
           {384, 384, 430, 416, 0}}, // 4 pt polygon, zeros for padding test struct
          {152, 24, {1156, 1157, 1164, 1164, 1156}, {592, 592, 597, 600, 600}},
          {180,
           38,
           {544, 545, 544, 0, 0},
           {760, 768, 768, 0, 0}} // 3 pt polygon, zeros for padding test struct
      });
}

} // namespace
