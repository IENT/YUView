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

#include <statistics/StatisticsData.h>

namespace
{

TEST(StatisticsData, testPixelValueRetrievalInteger)
{
  stats::StatisticsData data;

  constexpr auto typeID     = 0;
  constexpr auto frameIndex = 0;

  stats::StatisticsType valueType(
      typeID, "Something", stats::color::ColorMapper({0, 10}, stats::color::PredefinedType::Jet));
  valueType.render = true;
  data.addStatType(valueType);

  EXPECT_EQ(data.needsLoading(frameIndex), ItemLoadingState::LoadingNeeded);
  EXPECT_EQ(data.getTypesThatNeedLoading(frameIndex).size(), std::size_t(1));
  EXPECT_EQ(data.getTypesThatNeedLoading(frameIndex).at(0), typeID);

  // Load data
  data.setFrameIndex(frameIndex);
  data[typeID].addBlockValue(8, 8, 16, 16, 7);

  const auto dataInsideRect = data.getValuesAt(QPoint(10, 12));
  EXPECT_EQ(dataInsideRect.size(), 1);
  EXPECT_EQ(dataInsideRect.at(0), QStringPair({"Something", "7"}));

  const auto dataOutside = data.getValuesAt(QPoint(0, 0));
  EXPECT_EQ(dataOutside.size(), 1);
  EXPECT_EQ(dataOutside.at(0), QStringPair({"Something", "-"}));
}

TEST(StatisticsData, testPixelValueRetrievalVector)
{
  stats::StatisticsData data;

  constexpr auto typeID     = 0;
  constexpr auto frameIndex = 0;

  using VectorScaling = int;
  stats::StatisticsType valueType(typeID, "Something", VectorScaling(4));
  valueType.render = true;
  data.addStatType(valueType);

  EXPECT_EQ(data.needsLoading(frameIndex), ItemLoadingState::LoadingNeeded);
  EXPECT_EQ(data.getTypesThatNeedLoading(frameIndex).size(), std::size_t(1));
  EXPECT_EQ(data.getTypesThatNeedLoading(frameIndex).at(0), typeID);

  // Load data
  data.setFrameIndex(frameIndex);
  data[typeID].addBlockVector(8, 8, 16, 16, 5, 13);

  const auto dataInsideRect = data.getValuesAt(QPoint(10, 12));
  EXPECT_EQ(dataInsideRect.size(), 1);
  EXPECT_EQ(dataInsideRect.at(0), QStringPair({"Something", "(1.25,3.25)"}));

  const auto dataOutside = data.getValuesAt(QPoint(0, 0));
  EXPECT_EQ(dataOutside.size(), 1);
  EXPECT_EQ(dataOutside.at(0), QStringPair({"Something", "-"}));
}

} // namespace
