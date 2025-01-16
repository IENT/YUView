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

TEST(StatisticsTypeBuilderTest, DefaultValues)
{
  const auto statisticsType = StatisticsTypeBuilder().build();

  EXPECT_EQ(statisticsType.typeID, 0);
  EXPECT_TRUE(statisticsType.typeName.empty());
  EXPECT_TRUE(statisticsType.description.empty());
  EXPECT_FALSE(statisticsType.render);
  EXPECT_EQ(statisticsType.alphaFactor, 50);
  EXPECT_FALSE(statisticsType.valueDataOptions);
  EXPECT_FALSE(statisticsType.vectorDataOptions);
  EXPECT_FALSE(statisticsType.gridOptions.render);
  EXPECT_EQ(statisticsType.gridOptions.style, LineDrawStyle());
  EXPECT_FALSE(statisticsType.gridOptions.scaleToZoom);
}

TEST(StatisticsTypeBuilderTest, SetTypeNameDescriptionAndRenderValues)
{
  const auto statisticsType = stats::StatisticsTypeBuilder()
                                  .withTypeID(1)
                                  .withTypeName("TestType")
                                  .withDescription("TestDescription")
                                  .withRender(true)
                                  .withAlphaFactor(75)
                                  .build();

  EXPECT_EQ(statisticsType.typeID, 1);
  EXPECT_EQ(statisticsType.typeName, "TestType");
  EXPECT_EQ(statisticsType.description, "TestDescription");
  EXPECT_TRUE(statisticsType.render);
  EXPECT_EQ(statisticsType.alphaFactor, 75);
}

TEST(StatisticsTypeBuilderTest, SetValueDatDefaultValues)
{
  const auto statisticsType = stats::StatisticsTypeBuilder().withValueDataOptions({}).build();

  EXPECT_TRUE(statisticsType.valueDataOptions);
  EXPECT_EQ(statisticsType.valueDataOptions->render, true);
  EXPECT_EQ(statisticsType.valueDataOptions->scaleToBlockSize, false);
  EXPECT_EQ(statisticsType.valueDataOptions->colorMapper, color::ColorMapper());
}

TEST(StatisticsTypeBuilderTest, SetValueDataCustomValues)
{
  const auto colorMapper = color::ColorMapper({0, 255}, color::PredefinedType::Jet);

  const auto statisticsType =
      stats::StatisticsTypeBuilder()
          .withValueDataOptions(
              {.render = false, .scaleToBlockSize = true, .colorMapper = colorMapper})
          .build();

  EXPECT_TRUE(statisticsType.valueDataOptions);
  EXPECT_EQ(statisticsType.valueDataOptions->render, false);
  EXPECT_EQ(statisticsType.valueDataOptions->scaleToBlockSize, true);
  EXPECT_EQ(statisticsType.valueDataOptions->colorMapper, colorMapper);
}

TEST(StatisticsTypeBuilderTest, SetVectorDataDefaultValues)
{
  const auto statisticsType = StatisticsTypeBuilder().withVectorDataOptions({}).build();

  EXPECT_TRUE(statisticsType.vectorDataOptions);
  EXPECT_TRUE(statisticsType.vectorDataOptions->render);
  EXPECT_TRUE(statisticsType.vectorDataOptions->renderDataValues);
  EXPECT_FALSE(statisticsType.vectorDataOptions->scaleToZoom);
  EXPECT_EQ(statisticsType.vectorDataOptions->style, LineDrawStyle());
  EXPECT_EQ(statisticsType.vectorDataOptions->scale, 0);
  EXPECT_FALSE(statisticsType.vectorDataOptions->mapToColor);
  EXPECT_EQ(statisticsType.vectorDataOptions->arrowHead, StatisticsType::ArrowHead::arrow);
}

TEST(StatisticsTypeBuilderTest, SetVectorDataCustomValues)
{
  const auto lineDrawStyle = LineDrawStyle(Color(255, 0, 0), 2, Pattern::DashDot);

  const auto statisticsType = StatisticsTypeBuilder()
                                  .withVectorDataOptions({
                                      .render           = false,
                                      .renderDataValues = false,
                                      .scaleToZoom      = true,
                                      .style            = lineDrawStyle,
                                      .scale            = 3,
                                      .mapToColor       = true,
                                      .arrowHead        = StatisticsType::ArrowHead::circle,
                                  })
                                  .build();

  EXPECT_TRUE(statisticsType.vectorDataOptions);
  EXPECT_FALSE(statisticsType.vectorDataOptions->render);
  EXPECT_FALSE(statisticsType.vectorDataOptions->renderDataValues);
  EXPECT_TRUE(statisticsType.vectorDataOptions->scaleToZoom);
  EXPECT_EQ(statisticsType.vectorDataOptions->style, lineDrawStyle);
  EXPECT_EQ(statisticsType.vectorDataOptions->scale, 3);
  EXPECT_TRUE(statisticsType.vectorDataOptions->mapToColor);
  EXPECT_EQ(statisticsType.vectorDataOptions->arrowHead, StatisticsType::ArrowHead::circle);
}

TEST(StatisticsTypeBuilderTest, SetGridOptionsDefaultValues)
{
  const auto statisticsType = StatisticsTypeBuilder().withGridOptions({}).build();

  EXPECT_FALSE(statisticsType.gridOptions.render);
  EXPECT_EQ(statisticsType.gridOptions.style, LineDrawStyle());
  EXPECT_FALSE(statisticsType.gridOptions.scaleToZoom);
}

TEST(StatisticsTypeBuilderTest, SetGridOptionsCustomValues)
{
  const auto lineDrawStyle = LineDrawStyle(Color(123, 44, 99), 5, Pattern::DashDot);

  const auto statisticsType =
      StatisticsTypeBuilder()
          .withGridOptions({.render = true, .style = lineDrawStyle, .scaleToZoom = true})
          .build();

  EXPECT_TRUE(statisticsType.gridOptions.render);
  EXPECT_EQ(statisticsType.gridOptions.style, lineDrawStyle);
  EXPECT_TRUE(statisticsType.gridOptions.scaleToZoom);
}

} // namespace stats::test
