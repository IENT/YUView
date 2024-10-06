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

#include "CheckFunctions.h"

#include "gtest/gtest.h"

namespace yuviewTest::statistics
{

void checkVectorList(const std::vector<stats::StatsItemVector> &vectors,
                     const std::vector<CheckStatsItem>         &checkItems)
{
  EXPECT_EQ(vectors.size(), checkItems.size());
  for (unsigned i = 0; i < vectors.size(); i++)
  {
    const auto vec = vectors[i];
    const auto chk = checkItems[i];
    EXPECT_EQ(unsigned(vec.pos[0]), chk.x);
    EXPECT_EQ(unsigned(vec.pos[1]), chk.y);
    EXPECT_EQ(unsigned(vec.size[0]), chk.w);
    EXPECT_EQ(unsigned(vec.size[1]), chk.h);
    EXPECT_EQ(vec.point[0].x, chk.v0);
    EXPECT_EQ(vec.point[0].y, chk.v1);
  }
}

void checkValueList(const std::vector<stats::StatsItemValue> &values,
                    const std::vector<CheckStatsItem>        &checkItems)
{
  EXPECT_EQ(values.size(), checkItems.size());
  for (unsigned i = 0; i < values.size(); i++)
  {
    const auto val = values[i];
    const auto chk = checkItems[i];
    EXPECT_EQ(unsigned(val.pos[0]), chk.x);
    EXPECT_EQ(unsigned(val.pos[1]), chk.y);
    EXPECT_EQ(unsigned(val.size[0]), chk.w);
    EXPECT_EQ(unsigned(val.size[1]), chk.h);
    EXPECT_EQ(val.value, chk.v0);
  }
}

void checkAffineTFVectorList(const std::vector<stats::StatsItemAffineTF> &affineTFvectors,
                             const std::vector<CheckAffineTFItem>        &checkItems)
{
  EXPECT_EQ(affineTFvectors.size(), checkItems.size());
  for (unsigned i = 0; i < affineTFvectors.size(); i++)
  {
    auto vec = affineTFvectors[i];
    auto chk = checkItems[i];
    EXPECT_EQ(unsigned(vec.pos[0]), chk.x);
    EXPECT_EQ(unsigned(vec.pos[1]), chk.y);
    EXPECT_EQ(unsigned(vec.size[0]), chk.w);
    EXPECT_EQ(unsigned(vec.size[1]), chk.h);
    EXPECT_EQ(vec.point[0].x, chk.v0);
    EXPECT_EQ(vec.point[0].y, chk.v1);
    EXPECT_EQ(vec.point[1].x, chk.v2);
    EXPECT_EQ(vec.point[1].y, chk.v3);
    EXPECT_EQ(vec.point[2].x, chk.v4);
    EXPECT_EQ(vec.point[2].y, chk.v5);
  }
}

void checkLineList(const std::vector<stats::StatsItemVector> &lines,
                   const std::vector<CheckLineItem>          &checkItems)
{
  EXPECT_EQ(lines.size(), checkItems.size());
  for (unsigned i = 0; i < lines.size(); i++)
  {
    auto vec = lines[i];
    auto chk = checkItems[i];
    EXPECT_EQ(unsigned(vec.pos[0]), chk.x);
    EXPECT_EQ(unsigned(vec.pos[1]), chk.y);
    EXPECT_EQ(unsigned(vec.size[0]), chk.w);
    EXPECT_EQ(unsigned(vec.size[1]), chk.h);
    EXPECT_EQ(vec.point[0].x, chk.v0);
    EXPECT_EQ(vec.point[0].y, chk.v1);
    EXPECT_EQ(vec.point[1].x, chk.v2);
    EXPECT_EQ(vec.point[1].y, chk.v3);
  }
}

void checkPolygonvectorList(const std::vector<stats::StatsItemPolygonVector> &polygonList,
                            const std::vector<CheckPolygonVectorItem>        &checkItems)
{
  EXPECT_EQ(polygonList.size(), checkItems.size());
  for (unsigned i = 0; i < polygonList.size(); i++)
  {
    auto polygon = polygonList[i];
    auto chk     = checkItems[i];
    for (unsigned i = 0; i < polygon.corners.size(); i++)
    {
      auto corner = polygon.corners[i];
      EXPECT_EQ(unsigned(corner.x), chk.x[i]);
      EXPECT_EQ(unsigned(corner.y), chk.y[i]);
    }
    EXPECT_EQ(polygon.point.x, chk.v0);
    EXPECT_EQ(polygon.point.y, chk.v1);
  }
}

} // namespace yuviewTest::statistics
