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

namespace stats
{

StatisticsType::StatisticsType(int typeID, const QString &typeName)
    : typeID(typeID), typeName(typeName)
{
  // Default, do not render, alpha 50%
  render      = false;
  alphaFactor = 50;

  // For this constructor, we don't know if there is value or vector data.
  // Set one of these to true if you want to render something.
  hasValueData    = false;
  hasVectorData   = false;
  hasAffineTFData = false;

  // Default values for drawing value data
  renderValueData       = false;
  scaleValueToBlockSize = false;

  // Default values for drawing vectors
  renderVectorData       = false;
  renderVectorDataValues = true;
  vectorScale            = 1;
  vectorPen              = QPen(QBrush(QColor(Qt::black)), 1.0, Qt::SolidLine);
  scaleVectorToZoom      = false;
  mapVectorToColor       = false;
  arrowHead              = arrow;

  // Default values for drawing grids
  renderGrid      = true;
  scaleGridToZoom = false;
  gridPen         = QPen(QBrush(QColor(Qt::black)), 0.25, Qt::SolidLine);

  // Default: no polygon shape, use Rect
  isPolygon = false;
}

StatisticsType::StatisticsType(int typeID, const QString &typeName, int vectorScaling)
    : StatisticsType(typeID, typeName)
{
  this->hasVectorData    = true;
  this->renderVectorData = true;
  this->vectorScale      = vectorScaling;

  this->setInitialState();
}

// Convenience constructor for a statistics type with block data and a named color map
StatisticsType::StatisticsType(int            typeID,
                               const QString &typeName,
                               const QString &defaultColorRangeName,
                               int            rangeMin,
                               int            rangeMax,
                               bool           hasAndRenderVectorData)
    : StatisticsType(typeID, typeName)
{
  // There is value data. Set up a color mapper.
  this->hasValueData    = true;
  this->renderValueData = true;
  this->colorMapper     = ColorMapper(defaultColorRangeName, rangeMin, rangeMax);

  this->hasVectorData    = hasAndRenderVectorData;
  this->renderVectorData = hasAndRenderVectorData;

  this->setInitialState();
}

// Convenience constructor for a statistics type with block data and a color gradient based color
// mapping
StatisticsType::StatisticsType(int            typeID,
                               const QString &typeName,
                               int            cRangeMin,
                               const Color &  cRangeMinColor,
                               int            cRangeMax,
                               const Color &  cRangeMaxColor,
                               bool           hasAndRenderVectorData)
    : StatisticsType(typeID, typeName)
{
  // There is value data. Set up a color mapper.
  hasValueData    = true;
  renderValueData = true;
  colorMapper     = ColorMapper(cRangeMin, cRangeMinColor, cRangeMax, cRangeMaxColor);

  hasVectorData    = hasAndRenderVectorData;
  renderVectorData = hasAndRenderVectorData;

  setInitialState();
}

void StatisticsType::setInitialState()
{
  init.render      = render;
  init.alphaFactor = alphaFactor;

  init.renderValueData       = renderValueData;
  init.scaleValueToBlockSize = scaleValueToBlockSize;
  init.colorMapper           = colorMapper;

  init.renderVectorData  = renderVectorData;
  init.scaleVectorToZoom = scaleVectorToZoom;
  init.vectorPen         = vectorPen;
  init.vectorScale       = vectorScale;
  init.mapVectorToColor  = mapVectorToColor;
  init.arrowHead         = arrowHead;

  init.renderGrid      = renderGrid;
  init.gridPen         = gridPen;
  init.scaleGridToZoom = scaleGridToZoom;
}

// Get a string with all values of the QPen
QString convertPenToString(const QPen &pen)
{
  return QString("%1 %2 %3").arg(pen.color().name()).arg(pen.widthF()).arg(pen.style());
}
// The inverse functio to get a QPen from the string
QPen convertStringToPen(const QString &str)
{
  QPen        pen;
  QStringList split = str.split(" ");
  if (split.length() == 3)
  {
    pen.setColor(QColor(split[0]));
    pen.setWidthF(split[1].toFloat());
    pen.setStyle(Qt::PenStyle(split[2].toInt()));
  }
  return pen;
}

/* Save all the settings of the statistics type that have changed from the initial state
 */
void StatisticsType::savePlaylist(YUViewDomElement &root) const
{
  bool statChanged =
      (init.render != render || init.alphaFactor != alphaFactor ||
       init.renderValueData != renderValueData ||
       init.scaleValueToBlockSize != scaleValueToBlockSize || init.colorMapper != colorMapper ||
       init.renderVectorData != renderVectorData || init.scaleVectorToZoom != scaleVectorToZoom ||
       init.vectorPen != vectorPen || init.vectorScale != vectorScale ||
       init.mapVectorToColor != mapVectorToColor || init.arrowHead != arrowHead ||
       init.renderGrid != renderGrid || init.gridPen != gridPen ||
       init.scaleGridToZoom != scaleGridToZoom);

  if (!statChanged)
    return;

  // Create a new node
  auto newChild = root.ownerDocument().createElement(QString("statType%1").arg(typeID));
  newChild.appendChild(root.ownerDocument().createTextNode(typeName));

  // Append only the parameters that changed
  if (init.render != render)
    newChild.setAttribute("render", render);
  if (init.alphaFactor != alphaFactor)
    newChild.setAttribute("alphaFactor", alphaFactor);
  if (init.renderValueData != renderValueData)
    newChild.setAttribute("renderValueData", renderValueData);
  if (init.scaleValueToBlockSize != scaleValueToBlockSize)
    newChild.setAttribute("scaleValueToBlockSize", scaleValueToBlockSize);
  if (init.colorMapper != colorMapper)
  {
    if (init.colorMapper.mappingType != colorMapper.mappingType)
      newChild.setAttribute("colorMapperType",
                            ColorMapper::mappingTypeToUInt(colorMapper.mappingType));
    if (colorMapper.mappingType == ColorMapper::MappingType::gradient)
    {
      if (init.colorMapper.minColor != colorMapper.minColor)
        newChild.setAttribute("colorMapperMinColor",
                              QString::fromStdString(colorMapper.minColor.toHex()));
      if (init.colorMapper.maxColor != colorMapper.maxColor)
        newChild.setAttribute("colorMapperMaxColor",
                              QString::fromStdString(colorMapper.maxColor.toHex()));
    }
    if (colorMapper.mappingType == ColorMapper::MappingType::gradient ||
        colorMapper.mappingType == ColorMapper::MappingType::complex)
    {
      if (init.colorMapper.rangeMin != colorMapper.rangeMin)
        newChild.setAttribute("colorMapperRangeMin", colorMapper.rangeMin);
      if (init.colorMapper.rangeMax != colorMapper.rangeMax)
        newChild.setAttribute("colorMapperRangeMax", colorMapper.rangeMax);
    }
    if (colorMapper.mappingType == ColorMapper::MappingType::map)
    {
      if (init.colorMapper.colorMap != colorMapper.colorMap)
      {
        // Append the whole color map
        for (auto &[key, value] : colorMapper.colorMap)
          newChild.setAttribute(QString("colorMapperMapValue%1").arg(key),
                                QString::fromStdString(value.toHex()));
      }
    }
  }

  if (init.renderVectorData != renderVectorData)
    newChild.setAttribute("renderVectorData", renderVectorData);
  if (init.scaleVectorToZoom != scaleVectorToZoom)
    newChild.setAttribute("scaleVectorToZoom", scaleVectorToZoom);
  if (init.vectorPen != vectorPen)
    newChild.setAttribute("vectorPen", convertPenToString(vectorPen));
  if (init.vectorScale != vectorScale)
    newChild.setAttribute("vectorScale", vectorScale);
  if (init.mapVectorToColor != mapVectorToColor)
    newChild.setAttribute("mapVectorToColor", mapVectorToColor);
  if (init.arrowHead != arrowHead)
    newChild.setAttribute("renderarrowHead", arrowHead);
  if (init.renderGrid != renderGrid)
    newChild.setAttribute("renderGrid", renderGrid);
  if (init.gridPen != gridPen)
    newChild.setAttribute("gridPen", convertPenToString(gridPen));
  if (init.scaleGridToZoom != scaleGridToZoom)
    newChild.setAttribute("scaleGridToZoom", scaleGridToZoom);

  root.appendChild(newChild);
}

void StatisticsType::loadPlaylist(const YUViewDomElement &root)
{
  QStringPairList attributes;
  auto            statItemName = root.findChildValue(QString("statType%1").arg(typeID), attributes);

  if (statItemName != typeName)
    // The name of this type with the right ID and the name in the playlist don't match?...
    return;

  // Parse and set all the attributes that are in the playlist
  for (int i = 0; i < attributes.length(); i++)
  {
    if (attributes[i].first == "render")
      render = (attributes[i].second != "0");
    else if (attributes[i].first == "alphaFactor")
      alphaFactor = attributes[i].second.toInt();
    else if (attributes[i].first == "renderValueData")
      renderValueData = (attributes[i].second != "0");
    else if (attributes[i].first == "scaleValueToBlockSize")
      scaleValueToBlockSize = (attributes[i].second != "0");
    else if (attributes[i].first == "colorMapperType")
      colorMapper.mappingType = ColorMapper::MappingType(attributes[i].second.toInt());
    else if (attributes[i].first == "colorMapperMinColor")
      colorMapper.minColor = Color(attributes[i].second.toStdString());
    else if (attributes[i].first == "colorMapperMaxColor")
      colorMapper.maxColor = Color(attributes[i].second.toStdString());
    else if (attributes[i].first == "colorMapperRangeMin")
      colorMapper.rangeMin = attributes[i].second.toInt();
    else if (attributes[i].first == "colorMapperRangeMax")
      colorMapper.rangeMax = attributes[i].second.toInt();
    else if (attributes[i].first.startsWith("colorMapperMapValue"))
    {
      auto key                  = attributes[i].first.mid(19).toInt();
      auto value                = Color(attributes[i].second.toStdString());
      colorMapper.colorMap[key] = value;
    }
    else if (attributes[i].first == "renderVectorData")
      renderVectorData = (attributes[i].second != "0");
    else if (attributes[i].first == "scaleVectorToZoom")
      scaleVectorToZoom = (attributes[i].second != "0");
    else if (attributes[i].first == "vectorPen")
      vectorPen = convertStringToPen(attributes[i].second);
    else if (attributes[i].first == "vectorScale")
      vectorScale = attributes[i].second.toInt();
    else if (attributes[i].first == "mapVectorToColor")
      mapVectorToColor = (attributes[i].second != "0");
    else if (attributes[i].first == "renderarrowHead")
      arrowHead = arrowHead_t(attributes[i].second.toInt());
    else if (attributes[i].first == "renderGrid")
      renderGrid = (attributes[i].second != "0");
    else if (attributes[i].first == "gridPen")
      gridPen = convertStringToPen(attributes[i].second);
    else if (attributes[i].first == "scaleGridToZoom")
      scaleGridToZoom = (attributes[i].second != "0");
  }
}

// If the internal valueMap can map the value to text, text and value will be returned.
// Otherwise just the value as QString will be returned.
QString StatisticsType::getValueTxt(int val) const
{
  if (valMap.count(val) > 0)
  {
    // A text for this value van be shown.
    return QString("%1 (%2)").arg(valMap.at(val)).arg(val);
  }
  return QString("%1").arg(val);
}

void StatisticsType::setMappingValues(std::vector<QString> values)
{
  // We assume linear increasing typed IDs
  for (int i = 0; i < values.size(); i++)
    this->valMap[i] = values[i];
}

QString StatisticsType::getMappedValue(int typeID) const
{
  if (this->valMap.count(typeID) == 0)
    return {};

  return this->valMap.at(typeID);
}

} // namespace stats