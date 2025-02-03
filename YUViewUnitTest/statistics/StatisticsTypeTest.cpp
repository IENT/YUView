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

#include <statistics/StatisticsTypeBuilder.h>

namespace stats::test
{

TEST(StatisticsTypeTest, GetValueText)
{
  const auto statisticsType =
      StatisticsTypeBuilder(0, "").withMappingValues({"First", "Second", "Third"}).build();

  EXPECT_EQ(statisticsType.getValueText(0), "First (0)");
  EXPECT_EQ(statisticsType.getValueText(1), "Second (1)");
  EXPECT_EQ(statisticsType.getValueText(2), "Third (2)");
  EXPECT_EQ(statisticsType.getValueText(3), "3");
  EXPECT_EQ(statisticsType.getValueText(99), "99");
  EXPECT_EQ(statisticsType.getValueText(1256), "1256");
  EXPECT_EQ(statisticsType.getValueText(-1), "-1");
}

TEST(StatisticsTypeTest, TestValueDataEqualityOperator)
{
  const StatisticsType::ValueDataOptions options = {
      .render = true, .scaleToBlockSize = false, .colorMapper = color::ColorMapper()};

  const StatisticsType::ValueDataOptions identicalOptions = {
      .render = true, .scaleToBlockSize = false, .colorMapper = color::ColorMapper()};

  const StatisticsType::ValueDataOptions optionsWithDifferentRenderFlag = {
      .render = false, .scaleToBlockSize = false, .colorMapper = color::ColorMapper()};

  const StatisticsType::ValueDataOptions optionsWithDifferentScaleToBlockSize = {
      .render = true, .scaleToBlockSize = true, .colorMapper = color::ColorMapper()};

  const StatisticsType::ValueDataOptions optionsWithDifferentColorMapper = {
      .render           = false,
      .scaleToBlockSize = false,
      .colorMapper      = color::ColorMapper({0, 255}, Color(0, 0, 0), Color(0, 0, 255))};

  EXPECT_TRUE(options == identicalOptions);
  EXPECT_FALSE(options == optionsWithDifferentRenderFlag);
  EXPECT_TRUE(options != optionsWithDifferentRenderFlag);
  EXPECT_FALSE(options == optionsWithDifferentScaleToBlockSize);
  EXPECT_TRUE(options != optionsWithDifferentScaleToBlockSize);
  EXPECT_FALSE(options == optionsWithDifferentColorMapper);
  EXPECT_TRUE(options != optionsWithDifferentColorMapper);
}

TEST(StatisticsTypeTest, TestVectorDataEqualityOperator)
{
  const StatisticsType::VectorDataOptions options = {.render           = true,
                                                     .renderDataValues = false,
                                                     .scaleToZoom      = true,
                                                     .style            = LineDrawStyle(),
                                                     .scale            = 3,
                                                     .mapToColor       = true,
                                                     .arrowHead =
                                                         StatisticsType::ArrowHead::circle};

  const StatisticsType::VectorDataOptions identicalOptions = {
      .render           = true,
      .renderDataValues = false,
      .scaleToZoom      = true,
      .style            = LineDrawStyle(),
      .scale            = 3,
      .mapToColor       = true,
      .arrowHead        = StatisticsType::ArrowHead::circle};

  const StatisticsType::VectorDataOptions optionsWithDifferentRenderFlag = {
      .render           = false,
      .renderDataValues = false,
      .scaleToZoom      = true,
      .style            = LineDrawStyle(),
      .scale            = 3,
      .mapToColor       = true,
      .arrowHead        = StatisticsType::ArrowHead::circle};

  const StatisticsType::VectorDataOptions optionsWithDifferentRenderDataValuesFlag = {
      .render           = true,
      .renderDataValues = true,
      .scaleToZoom      = true,
      .style            = LineDrawStyle(),
      .scale            = 3,
      .mapToColor       = true,
      .arrowHead        = StatisticsType::ArrowHead::circle};

  const StatisticsType::VectorDataOptions optionsWithDifferentScaleToZoomFlag = {
      .render           = true,
      .renderDataValues = false,
      .scaleToZoom      = false,
      .style            = LineDrawStyle(),
      .scale            = 3,
      .mapToColor       = true,
      .arrowHead        = StatisticsType::ArrowHead::circle};

  const StatisticsType::VectorDataOptions optionsWithDifferentStyle = {
      .render           = true,
      .renderDataValues = false,
      .scaleToZoom      = true,
      .style            = LineDrawStyle({Color(255, 0, 0), 2, Pattern::DashDot}),
      .scale            = 3,
      .mapToColor       = true,
      .arrowHead        = StatisticsType::ArrowHead::circle};

  const StatisticsType::VectorDataOptions optionsWithDifferentScale = {
      .render           = true,
      .renderDataValues = false,
      .scaleToZoom      = true,
      .style            = LineDrawStyle(),
      .scale            = 4,
      .mapToColor       = true,
      .arrowHead        = StatisticsType::ArrowHead::circle};

  const StatisticsType::VectorDataOptions optionsWithDifferentMapToColorFlag = {
      .render           = true,
      .renderDataValues = false,
      .scaleToZoom      = true,
      .style            = LineDrawStyle(),
      .scale            = 3,
      .mapToColor       = false,
      .arrowHead        = StatisticsType::ArrowHead::circle};

  const StatisticsType::VectorDataOptions optionsWithDifferentArrowHead = {
      .render           = true,
      .renderDataValues = false,
      .scaleToZoom      = true,
      .style            = LineDrawStyle(),
      .scale            = 3,
      .mapToColor       = true,
      .arrowHead        = StatisticsType::ArrowHead::arrow};

  EXPECT_TRUE(options == identicalOptions);
  EXPECT_FALSE(options == optionsWithDifferentRenderFlag);
  EXPECT_TRUE(options != optionsWithDifferentRenderFlag);
  EXPECT_FALSE(options == optionsWithDifferentRenderDataValuesFlag);
  EXPECT_TRUE(options != optionsWithDifferentRenderDataValuesFlag);
  EXPECT_FALSE(options == optionsWithDifferentScaleToZoomFlag);
  EXPECT_TRUE(options != optionsWithDifferentScaleToZoomFlag);
  EXPECT_FALSE(options == optionsWithDifferentStyle);
  EXPECT_TRUE(options != optionsWithDifferentStyle);
  EXPECT_FALSE(options == optionsWithDifferentScale);
  EXPECT_TRUE(options != optionsWithDifferentScale);
  EXPECT_FALSE(options == optionsWithDifferentMapToColorFlag);
  EXPECT_TRUE(options != optionsWithDifferentMapToColorFlag);
  EXPECT_FALSE(options == optionsWithDifferentArrowHead);
  EXPECT_TRUE(options != optionsWithDifferentArrowHead);
}

} // namespace stats::test
