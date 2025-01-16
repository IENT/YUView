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

#include "StatisticsType.h"

#include <common/Functions.h>

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

std::vector<StatisticsType::ArrowHead> AllArrowHeads = {StatisticsType::ArrowHead::arrow,
                                                        StatisticsType::ArrowHead::circle,
                                                        StatisticsType::ArrowHead::none};

} // namespace

bool LineDrawStyle::operator==(const LineDrawStyle &other) const
{
  return color == other.color && width == other.width && pattern == other.pattern;
}

void StatisticsType::setInitialState()
{
  this->init.render      = this->render;
  this->init.alphaFactor = this->alphaFactor;

  this->init.valueDataOptions  = this->valueDataOptions;
  this->init.vectorDataOptions = this->vectorDataOptions;
  this->init.gridOptions       = this->gridOptions;
}

/* Save all the settings of the statistics type that have changed from the initial state
 */
void StatisticsType::savePlaylist(YUViewDomElement &root) const
{
  // bool statChanged =
  //     (init.render != render || init.alphaFactor != alphaFactor ||
  //      init.renderValueData != renderValueData ||
  //      init.scaleValueToBlockSize != scaleValueToBlockSize || init.colorMapper != colorMapper ||
  //      init.renderVectorData != renderVectorData || init.scaleVectorToZoom != scaleVectorToZoom
  //      || init.vectorStyle != vectorStyle || init.vectorScale != vectorScale ||
  //      init.mapVectorToColor != mapVectorToColor || init.arrowHead != arrowHead ||
  //      init.renderGrid != renderGrid || init.gridStyle != gridStyle ||
  //      init.scaleGridToZoom != scaleGridToZoom);

  // if (!statChanged)
  //   return;

  // YUViewDomElement newChild =
  // root.ownerDocument().createElement(QString("statType%1").arg(typeID));
  // newChild.appendChild(root.ownerDocument().createTextNode(QString::fromStdString(typeName)));

  // // Append only the parameters that changed
  // if (init.render != render)
  //   newChild.setAttribute("render", render);
  // if (init.alphaFactor != alphaFactor)
  //   newChild.setAttribute("alphaFactor", alphaFactor);
  // if (init.renderValueData != renderValueData)
  //   newChild.setAttribute("renderValueData", renderValueData);
  // if (init.scaleValueToBlockSize != scaleValueToBlockSize)
  //   newChild.setAttribute("scaleValueToBlockSize", scaleValueToBlockSize);
  // if (init.colorMapper != this->colorMapper)
  //   this->colorMapper.savePlaylist(newChild);
  // if (init.renderVectorData != renderVectorData)
  //   newChild.setAttribute("renderVectorData", renderVectorData);
  // if (init.scaleVectorToZoom != scaleVectorToZoom)
  //   newChild.setAttribute("scaleVectorToZoom", scaleVectorToZoom);
  // if (init.vectorStyle != vectorStyle)
  //   newChild.setAttribute("vectorStyle", convertPenToString(vectorStyle));
  // if (init.vectorScale != vectorScale)
  //   newChild.setAttribute("vectorScale", vectorScale);
  // if (init.mapVectorToColor != mapVectorToColor)
  //   newChild.setAttribute("mapVectorToColor", mapVectorToColor);
  // if (init.arrowHead != arrowHead)
  // {
  //   if (const auto index = vectorIndexOf(stats::AllArrowHeads, arrowHead))
  //     newChild.setAttribute("renderarrowHead", static_cast<int>(*index));
  // }
  // if (init.renderGrid != renderGrid)
  //   newChild.setAttribute("renderGrid", renderGrid);
  // if (init.gridStyle != gridStyle)
  //   newChild.setAttribute("gridStyle", convertPenToString(gridStyle));
  // if (init.scaleGridToZoom != scaleGridToZoom)
  //   newChild.setAttribute("scaleGridToZoom", scaleGridToZoom);

  // root.appendChild(newChild);
}

void StatisticsType::loadPlaylist(const YUViewDomElement &root)
{
  // auto [name, attributes] = root.findChildValueWithAttributes(QString("statType%1").arg(typeID));

  // if (name.toStdString() != this->typeName)
  //   // The name of this type with the right ID and the name in the playlist don't match?...
  //   return;

  // // Parse and set all the attributes that are in the playlist
  // for (int i = 0; i < attributes.length(); i++)
  // {
  //   if (attributes[i].first == "render")
  //     render = (attributes[i].second != "0");
  //   else if (attributes[i].first == "alphaFactor")
  //     alphaFactor = attributes[i].second.toInt();
  //   else if (attributes[i].first == "renderValueData")
  //     renderValueData = (attributes[i].second != "0");
  //   else if (attributes[i].first == "scaleValueToBlockSize")
  //     scaleValueToBlockSize = (attributes[i].second != "0");
  //   else if (attributes[i].first == "renderVectorData")
  //     renderVectorData = (attributes[i].second != "0");
  //   else if (attributes[i].first == "scaleVectorToZoom")
  //     scaleVectorToZoom = (attributes[i].second != "0");
  //   else if (attributes[i].first == "vectorPen")
  //     vectorStyle = convertStringToPen(attributes[i].second);
  //   else if (attributes[i].first == "vectorScale")
  //     vectorScale = attributes[i].second.toInt();
  //   else if (attributes[i].first == "mapVectorToColor")
  //     mapVectorToColor = (attributes[i].second != "0");
  //   else if (attributes[i].first == "renderarrowHead")
  //   {
  //     auto idx = attributes[i].second.toInt();
  //     if (idx >= 0 && unsigned(idx) < AllArrowHeads.size())
  //       arrowHead = AllArrowHeads[idx];
  //   }
  //   else if (attributes[i].first == "renderGrid")
  //     renderGrid = (attributes[i].second != "0");
  //   else if (attributes[i].first == "gridPen")
  //     gridStyle = convertStringToPen(attributes[i].second);
  //   else if (attributes[i].first == "scaleGridToZoom")
  //     scaleGridToZoom = (attributes[i].second != "0");
  // }

  // this->colorMapper.loadPlaylist(attributes);
}

// If the internal valueMap can map the value to text, text and value will be returned.
// Otherwise just the value as QString will be returned.
std::string StatisticsType::getValueText(const int val) const
{
  if (this->valuesToText.count(val) > 0)
  {
    // A text for this value van be shown.
    return this->valuesToText.at(val) + " (" + std::to_string(val) + ")";
  }
  return std::to_string(val);
}

void StatisticsType::setMappingValues(std::vector<std::string> values)
{
  // We assume linear increasing typed IDs
  for (int i = 0; i < int(values.size()); i++)
    this->valuesToText[i] = values[i];
}

std::string StatisticsType::getMappedValue(const int typeID) const
{
  if (this->valuesToText.count(typeID) == 0)
    return {};

  return this->valuesToText.at(typeID);
}

} // namespace stats
