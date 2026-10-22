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
#include <statistics/StatisticsTypePlaylistHandler.h>

#include <QTextStream>

namespace stats::test
{

namespace
{

QDomElement saveItemAndTryGetAsElement(const StatisticsType &statisticsType)
{
  QDomDocument document;

  YUViewDomElement d = document.createElement("playlistItemStatisticsFile");
  StatisticsTypePlaylistHandler::saveToPlaylist(statisticsType, d);
  document.appendChild(d);

  return document.firstChildElement("playlistItemStatisticsFile").firstChild().toElement();
}

QDomDocument loadAndCheckXMLDocument(const QString &xml)
{
  QDomDocument document;
  EXPECT_TRUE(document.setContent(xml));

  const auto root    = document.documentElement();
  const auto tagName = root.tagName();
  EXPECT_EQ(tagName, "playlistItemStatisticsFile");

  return document;
}

bool anyValueModified(const StatisticsType &type)
{
  const auto valueDataOptionsModified =
      type.valueDataOptions && (type.valueDataOptions->render.wasModified() ||
                                type.valueDataOptions->scaleToBlockSize.wasModified() ||
                                type.valueDataOptions->colorMapper.wasModified());

  const auto vectorDataOptionsModified =
      type.vectorDataOptions &&
      (type.vectorDataOptions->render.wasModified() ||
       type.vectorDataOptions->renderDataValues.wasModified() ||
       type.vectorDataOptions->scaleToZoom.wasModified() ||
       type.vectorDataOptions->style.wasModified() || type.vectorDataOptions->scale.wasModified() ||
       type.vectorDataOptions->mapToColor.wasModified() ||
       type.vectorDataOptions->arrowHead.wasModified());

  const auto gridOptionsModified = type.gridOptions.render.wasModified() || //
                                   type.gridOptions.style.wasModified() ||  //
                                   type.gridOptions.scaleToZoom.wasModified();

  return type.render.wasModified() || type.alphaFactor.wasModified() || valueDataOptionsModified ||
         vectorDataOptionsModified || gridOptionsModified;
}

} // namespace

TEST(StatisticsTypePlaylistHandlerTest,
     SaveStatisticsType_NoOptionsModified_ShouldNotAddTheTypeAtAll)
{
  const color::ColorMapper colorMapper({0, 255}, color::PredefinedType::Jet);

  const auto statisticsType =
      stats::StatisticsTypeBuilder(1, "SomeTestType")
          .withValueDataOptions(
              {.render = false, .scaleToBlockSize = true, .colorMapper = colorMapper})
          .build();

  EXPECT_TRUE(saveItemAndTryGetAsElement(statisticsType).isNull());
}

TEST(StatisticsTypePlaylistHandlerTest,
     SaveStatisticsType_RenderAndAlphaFactorModified_ShouldAddTypeAndValues)
{
  auto statisticsType =
      stats::StatisticsTypeBuilder(1, "SomeTestType").withRender(false).withAlphaFactor(75).build();

  // Modify values
  statisticsType.render      = true;
  statisticsType.alphaFactor = 98;

  const auto item = saveItemAndTryGetAsElement(statisticsType);
  EXPECT_FALSE(item.isNull());
  EXPECT_EQ(item.tagName(), "statType1");
  EXPECT_EQ(item.text(), "SomeTestType");
  EXPECT_EQ(item.attribute("render").toInt(), 1);
  EXPECT_EQ(item.attribute("alphaFactor").toInt(), 98);
}

TEST(StatisticsTypePlaylistHandlerTest,
     SaveStatisticsType_ValueDataOptionsModified_ShouldAddTypeAndValues)
{
  auto statisticsType = stats::StatisticsTypeBuilder(1, "SomeTestType")
                            .withValueDataOptions({})
                            .withVectorDataOptions({})
                            .build();

  // Modify value data options
  statisticsType.valueDataOptions->render           = false;
  statisticsType.valueDataOptions->scaleToBlockSize = true;
  statisticsType.valueDataOptions->colorMapper =
      color::ColorMapper({0, 255}, color::PredefinedType::Jet);

  const auto item = saveItemAndTryGetAsElement(statisticsType);
  EXPECT_FALSE(item.isNull());
  EXPECT_EQ(item.tagName(), "statType1");
  EXPECT_EQ(item.text(), "SomeTestType");
  EXPECT_EQ(item.attribute("colorMapperPredefinedType"), "Jet");
  EXPECT_EQ(item.attribute("colorMapperRange"), "0|255");
  EXPECT_EQ(item.attribute("colorMapperType"), "Predefined");
  EXPECT_EQ(item.attribute("renderValueData"), "0");
  EXPECT_EQ(item.attribute("scaleValueToBlockSize"), "0");
}

TEST(StatisticsTypePlaylistHandlerTest,
     SaveStatisticsType_VectorDataOptionsModified_ShouldAddTypeAndValues)
{
  auto statisticsType = stats::StatisticsTypeBuilder(1, "SomeTestType")
                            .withValueDataOptions({})
                            .withVectorDataOptions({})
                            .build();

  // Modify vector data options
  statisticsType.vectorDataOptions->render           = false;
  statisticsType.vectorDataOptions->renderDataValues = false;
  statisticsType.vectorDataOptions->scaleToZoom      = true;
  statisticsType.vectorDataOptions->style =
      LineDrawStyle({Color(128, 22, 76), 2.0, Pattern::DashDot});
  statisticsType.vectorDataOptions->scale      = 4;
  statisticsType.vectorDataOptions->mapToColor = true;
  statisticsType.vectorDataOptions->arrowHead  = ArrowHead::circle;

  const auto item = saveItemAndTryGetAsElement(statisticsType);
  EXPECT_FALSE(item.isNull());
  EXPECT_EQ(item.tagName(), "statType1");
  EXPECT_EQ(item.text(), "SomeTestType");
  EXPECT_EQ(item.attribute("mapVectorToColor"), "1");
  EXPECT_EQ(item.attribute("renderVectorData"), "0");
  EXPECT_EQ(item.attribute("renderVectorDataValues"), "0");
  EXPECT_EQ(item.attribute("renderarrowHead"), "1");
  EXPECT_EQ(item.attribute("scaleVectorToZoom"), "1");
  EXPECT_EQ(item.attribute("vectorScale"), "4");
  EXPECT_EQ(item.attribute("vectorStyle"), "#80164c 2 3");
}

TEST(StatisticsTypePlaylistHandlerTest,
     SaveStatisticsType_GridOptionsModified_ShouldAddTypeAndValues)
{
  auto statisticsType = stats::StatisticsTypeBuilder(1, "SomeTestType")
                            .withValueDataOptions({})
                            .withVectorDataOptions({})
                            .build();

  // Modify grid options
  statisticsType.gridOptions.render      = true;
  statisticsType.gridOptions.style       = LineDrawStyle({Color(12, 15, 240), 1.25, Pattern::Dash});
  statisticsType.gridOptions.scaleToZoom = true;

  const auto item = saveItemAndTryGetAsElement(statisticsType);
  EXPECT_FALSE(item.isNull());
  EXPECT_EQ(item.tagName(), "statType1");
  EXPECT_EQ(item.text(), "SomeTestType");
  EXPECT_EQ(item.attribute("gridStyle"), "#0c0ff0 1.25 1");
  EXPECT_EQ(item.attribute("renderGrid"), "1");
  EXPECT_EQ(item.attribute("scaleGridToZoom"), "1");
}

TEST(StatisticsTypePlaylistHandlerTest, LoadStatisticsType_EmptyXML_ShouldNotModifyStatisticsType)
{
  auto statisticsType = stats::StatisticsTypeBuilder(1, "SomeTestType")
                            .withValueDataOptions({})
                            .withVectorDataOptions({})
                            .build();

  const auto document = loadAndCheckXMLDocument("<playlistItemStatisticsFile/>");

  StatisticsTypePlaylistHandler::tryToLoadFromPlaylist(statisticsType, document.documentElement());

  const auto expectedStatisticsType = stats::StatisticsTypeBuilder(1, "SomeTestType")
                                          .withValueDataOptions({})
                                          .withVectorDataOptions({})
                                          .build();
  EXPECT_EQ(expectedStatisticsType, statisticsType);
}

TEST(StatisticsTypePlaylistHandlerTest,
     LoadStatisticsType_StatisticsTypeNotInXML_ShouldNotModifyStatisticsType)
{
  auto statisticsType = stats::StatisticsTypeBuilder(0, "SomeTestType")
                            .withValueDataOptions({})
                            .withVectorDataOptions({})
                            .build();

  const auto document =
      loadAndCheckXMLDocument("<playlistItemStatisticsFile>\n"
                              " <statType1 gridStyle=\"#0c0ff0 1.25 1\" renderGrid=\"1\" "
                              "scaleGridToZoom=\"1\">SomeTestType</statType1>\n"
                              "</playlistItemStatisticsFile>\n");

  StatisticsTypePlaylistHandler::tryToLoadFromPlaylist(statisticsType, document.documentElement());

  const auto expectedStatisticsType = stats::StatisticsTypeBuilder(0, "SomeTestType")
                                          .withValueDataOptions({})
                                          .withVectorDataOptions({})
                                          .build();
  EXPECT_EQ(expectedStatisticsType, statisticsType);
}

TEST(StatisticsTypePlaylistHandlerTest,
     LoadStatisticsType_RenderAndAlphaFactorInXML_ShouldBeCorrectlySetInStatisticsType)
{
  auto statisticsType = stats::StatisticsTypeBuilder(1, "SomeTestType").build();

  const auto document = loadAndCheckXMLDocument(
      "<playlistItemStatisticsFile>\n"
      " <statType1 alphaFactor=\"98\" render=\"1\">SomeTestType</statType1>\n"
      "</playlistItemStatisticsFile>\n");

  StatisticsTypePlaylistHandler::tryToLoadFromPlaylist(statisticsType, document.documentElement());

  EXPECT_TRUE(statisticsType.render);
  EXPECT_EQ(statisticsType.alphaFactor, 98);

  EXPECT_FALSE(statisticsType.valueDataOptions);
  EXPECT_FALSE(statisticsType.vectorDataOptions);
  EXPECT_EQ(statisticsType.gridOptions, StatisticsType::GridOptions());

  EXPECT_FALSE(anyValueModified(statisticsType));
}

TEST(StatisticsTypePlaylistHandlerTest,
     LoadStatisticsType_ValueDataOptionsInXML_ShouldBeCorrectlySetInStatisticsType)
{
  auto statisticsType = stats::StatisticsTypeBuilder(1, "SomeTestType").build();

  const auto document = loadAndCheckXMLDocument(
      "<playlistItemStatisticsFile>\n"
      " <statType1 colorMapperPredefinedType=\"Jet\" colorMapperRange=\"0|255\" "
      "colorMapperType=\"Predefined\" renderValueData=\"0\" scaleValueToBlockSize=\"0\">"
      "SomeTestType</statType1>\n"
      "</playlistItemStatisticsFile>\n");

  StatisticsTypePlaylistHandler::tryToLoadFromPlaylist(statisticsType, document.documentElement());

  EXPECT_EQ(statisticsType.valueDataOptions->render, false);
  EXPECT_EQ(statisticsType.valueDataOptions->scaleToBlockSize, false);
  EXPECT_EQ(statisticsType.valueDataOptions->colorMapper,
            color::ColorMapper({0, 255}, color::PredefinedType::Jet));

  EXPECT_FALSE(statisticsType.vectorDataOptions);
  EXPECT_EQ(statisticsType.gridOptions, StatisticsType::GridOptions());

  EXPECT_FALSE(anyValueModified(statisticsType));
}

TEST(StatisticsTypePlaylistHandlerTest,
     LoadStatisticsType_VectorDataOptionsInXML_ShouldBeCorrectlySetInStatisticsType)
{
  auto statisticsType = stats::StatisticsTypeBuilder(1, "SomeTestType").build();

  const auto document = loadAndCheckXMLDocument(
      "<playlistItemStatisticsFile>\n"
      " <statType1 mapVectorToColor=\"1\" renderVectorData=\"0\" renderVectorDataValues=\"0\" "
      "renderarrowHead=\"1\" scaleVectorToZoom=\"1\" vectorScale=\"4\" vectorStyle=\"#80164c 2 3\">"
      "SomeTestType</statType1>\n"
      "</playlistItemStatisticsFile>\n");

  StatisticsTypePlaylistHandler::tryToLoadFromPlaylist(statisticsType, document.documentElement());

  EXPECT_EQ(statisticsType.vectorDataOptions->render, false);
  EXPECT_EQ(statisticsType.vectorDataOptions->renderDataValues, false);
  EXPECT_EQ(statisticsType.vectorDataOptions->scaleToZoom, true);
  EXPECT_EQ(statisticsType.vectorDataOptions->style,
            LineDrawStyle({Color(128, 22, 76), 2.0, Pattern::DashDot}));
  EXPECT_EQ(statisticsType.vectorDataOptions->scale, 4);
  EXPECT_EQ(statisticsType.vectorDataOptions->mapToColor, true);
  EXPECT_EQ(statisticsType.vectorDataOptions->arrowHead, ArrowHead::circle);

  EXPECT_FALSE(statisticsType.valueDataOptions);
  EXPECT_EQ(statisticsType.gridOptions, StatisticsType::GridOptions());

  EXPECT_FALSE(anyValueModified(statisticsType));
}

TEST(StatisticsTypePlaylistHandlerTest,
     LoadStatisticsType_GridOptionsInXML_ShouldBeCorrectlySetInStatisticsType)
{
  auto statisticsType = stats::StatisticsTypeBuilder(1, "SomeTestType").build();

  const auto document = loadAndCheckXMLDocument(
      "<playlistItemStatisticsFile>\n"
      " <statType1 gridStyle=\"#0c0ff0 1.25 1\" renderGrid=\"1\" scaleGridToZoom=\"1\">"
      "SomeTestType</statType1>\n"
      "</playlistItemStatisticsFile>\n");

  StatisticsTypePlaylistHandler::tryToLoadFromPlaylist(statisticsType, document.documentElement());

  EXPECT_EQ(statisticsType.gridOptions.render, true);
  EXPECT_EQ(statisticsType.gridOptions.style,
            LineDrawStyle({Color(12, 15, 240), 1.25, Pattern::Dash}));
  EXPECT_EQ(statisticsType.gridOptions.scaleToZoom, true);

  EXPECT_FALSE(statisticsType.valueDataOptions);
  EXPECT_FALSE(statisticsType.vectorDataOptions);

  EXPECT_FALSE(anyValueModified(statisticsType));
}

} // namespace stats::test
