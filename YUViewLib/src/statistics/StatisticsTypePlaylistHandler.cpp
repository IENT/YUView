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

#include "StatisticsTypePlaylistHandler.h"

namespace stats
{

namespace
{

// Get a string with all values of the QPen
QString convertPenToString(const LineDrawStyle &style)
{
  auto colorHex  = QString::fromStdString(style.color.toHex());
  auto patternIt = std::find(AllPatterns.begin(), AllPatterns.end(), style.pattern);
  if (patternIt == AllPatterns.end())
    return {};
  auto patternIdx = std::distance(AllPatterns.begin(), patternIt);
  return QString("%1 %2 %3").arg(colorHex).arg(style.width).arg(patternIdx);
}

// The inverse functio to get a QPen from the string
LineDrawStyle convertStringToPen(const QString &str)
{
  LineDrawStyle style;
  QStringList   split = str.split(" ");
  if (split.length() == 3)
  {
    style.color     = Color(split[0].toStdString());
    style.width     = split[1].toDouble();
    auto patternIdx = split[2].toInt();
    if (patternIdx >= 0 && unsigned(patternIdx) < AllPatterns.size())
      style.pattern = AllPatterns[patternIdx];
  }
  return style;
}

void addModifiedValuesToElement(YUViewDomElement                                      &element,
                                const std::optional<StatisticsType::ValueDataOptions> &options)
{
  if (!options)
    return;

  if (options->render.wasModified())
    element.setAttribute("renderValueData", options->render);
  if (options->scaleToBlockSize.wasModified())
    element.setAttribute("scaleValueToBlockSize", options->render);
  if (options->colorMapper.wasModified())
    options->colorMapper->savePlaylist(element);
}

std::vector<StatisticsType::ArrowHead> AllArrowHeads = {StatisticsType::ArrowHead::arrow,
                                                        StatisticsType::ArrowHead::circle,
                                                        StatisticsType::ArrowHead::none};

void addModifiedValuesToElement(YUViewDomElement                                       &element,
                                const std::optional<StatisticsType::VectorDataOptions> &options,
                                const std::optional<StatisticsType::VectorDataOptions> &initOptions)
{
  if (!options)
    return;

  if (!initOptions || options->render != initOptions->render)
    element.setAttribute("renderVectorData", options->render);
  if (!initOptions || options->renderDataValues != initOptions->renderDataValues)
    element.setAttribute("renderVectorDataValues", options->renderDataValues);
  if (!initOptions || options->scaleToZoom != initOptions->scaleToZoom)
    element.setAttribute("scaleVectorToZoom", options->scaleToZoom);
  if (!initOptions || options->style != initOptions->style)
    element.setAttribute("vectorStyle", convertPenToString(options->style));
  if (!initOptions || options->scale != initOptions->scale)
    element.setAttribute("vectorScale", options->scale);
  if (!initOptions || options->mapToColor != initOptions->mapToColor)
    element.setAttribute("mapVectorToColor", options->mapToColor);
  if (!initOptions || options->arrowHead != initOptions->arrowHead)
  {
    if (const auto index = vectorIndexOf(AllArrowHeads, options->arrowHead))
      element.setAttribute("renderarrowHead", static_cast<int>(*index));
  }
}

void addModifiedValuesToElement(YUViewDomElement                  &element,
                                const StatisticsType::GridOptions &options,
                                const StatisticsType::GridOptions &initOptions)
{
  if (options.render != initOptions.render)
    element.setAttribute("renderGrid", options.render);
  if (options.style != initOptions.style)
    element.setAttribute("gridStyle", convertPenToString(options.style));
  if (options.scaleToZoom != initOptions.scaleToZoom)
    element.setAttribute("scaleGridToZoom", options.scaleToZoom);
}

} // namespace

void StatisticsTypePlaylistHandler::saveToPlaylist(const StatisticsType &type,
                                                   YUViewDomElement     &root)
{
  bool allValuesIdenticalToInitialValues =
      (!type.render.wasModified() &&                                        //
       !type.alphaFactor.wasModified() &&                                   //
       (!type.valueDataOptions || !type.valueDataOptions->wasModified()) && //
       type.init.vectorDataOptions == type.vectorDataOptions &&             //
       type.init.gridOptions == type.gridOptions);

  if (allValuesIdenticalToInitialValues)
    return;

  YUViewDomElement newChild =
      root.ownerDocument().createElement(QString("statType%1").arg(type.typeID));
  newChild.appendChild(root.ownerDocument().createTextNode(QString::fromStdString(type.typeName)));

  // Append only the parameters that changed
  if (type.render.wasModified())
    newChild.setAttribute("render", type.render);
  if (type.alphaFactor.wasModified())
    newChild.setAttribute("alphaFactor", type.alphaFactor);

  addModifiedValuesToElement(newChild, type.valueDataOptions);
  addModifiedValuesToElement(newChild, type.vectorDataOptions, type.init.vectorDataOptions);
  addModifiedValuesToElement(newChild, type.gridOptions, type.init.gridOptions);

  root.appendChild(newChild);
}

void StatisticsTypePlaylistHandler::tryToLoadFromPlaylist(StatisticsType         &type,
                                                          const YUViewDomElement &root)
{
  const auto [name, attributes] =
      root.findChildValueWithAttributes(QString("statType%1").arg(type.typeID));

  if (name.toStdString() != type.typeName)
    // The name of this type with the right ID and the name in the playlist don't match?...
    return;

  // Parse and set all the attributes that are in the playlist
  for (const auto &[name, value] : attributes)
  {
    if (name == "render")
      type.render = (value != "0");
    else if (name == "alphaFactor")
      type.alphaFactor = value.toInt();
    else if (name == "renderValueData" || name == "scaleValueToBlockSize" ||
             name == "colorMapperType")
    {
      if (!type.valueDataOptions)
        type.valueDataOptions = StatisticsType::ValueDataOptions();

      if (name == "renderValueData")
        type.valueDataOptions->render = (value != "0");
      else if (name == "scaleValueToBlockSize")
        type.valueDataOptions->scaleToBlockSize = (value != "0");
      else if (name == "colorMapperType")
        type.valueDataOptions->colorMapper->loadPlaylist(attributes);
    }
    else if (name == "renderVectorData" || name == "renderVectorDataValues" ||
             name == "scaleVectorToZoom" || name == "vectorStyle" || name == "vectorScale" ||
             name == "mapVectorToColor" || name == "renderarrowHead")
    {
      if (!type.vectorDataOptions)
        type.vectorDataOptions = StatisticsType::VectorDataOptions();

      if (name == "renderVectorData")
        type.vectorDataOptions->render = (value != "0");
      else if (name == "renderVectorDataValues")
        type.vectorDataOptions->renderDataValues = (value != "0");
      else if (name == "scaleVectorToZoom")
        type.vectorDataOptions->scaleToZoom = (value != "0");
      else if (name == "vectorStyle")
        type.vectorDataOptions->style = convertStringToPen(value);
      else if (name == "vectorScale")
        type.vectorDataOptions->scale = value.toInt();
      else if (name == "mapVectorToColor")
        type.vectorDataOptions->mapToColor = (value != "0");
      else if (name == "renderarrowHead")
      {
        const auto idx = value.toInt();
        if (idx >= 0 && unsigned(idx) < AllArrowHeads.size())
          type.vectorDataOptions->arrowHead = AllArrowHeads.at(idx);
      }
    }
    else if (name == "renderGrid")
      type.gridOptions.render = (value != "0");
    else if (name == "gridPen")
      type.gridOptions.style = convertStringToPen(value);
    else if (name == "scaleGridToZoom")
      type.gridOptions.scaleToZoom = (value != "0");
  }

  type.saveInitialState();
}

} // namespace stats
