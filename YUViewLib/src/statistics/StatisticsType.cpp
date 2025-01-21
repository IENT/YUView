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

void addModifiedValuesToElement(YUViewDomElement                                      &element,
                                const std::optional<StatisticsType::ValueDataOptions> &options,
                                const std::optional<StatisticsType::ValueDataOptions> &initOptions)
{
  if (!options)
    return;

  if (!initOptions || options->render != initOptions->render)
    element.setAttribute("renderValueData", options->render);
  if (!initOptions || options->scaleToBlockSize != initOptions->scaleToBlockSize)
    element.setAttribute("scaleValueToBlockSize", options->render);
  if (!initOptions || options->colorMapper != initOptions->colorMapper)
    options->colorMapper.savePlaylist(element);
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

bool LineDrawStyle::operator==(const LineDrawStyle &other) const
{
  return color == other.color && width == other.width && pattern == other.pattern;
}

bool LineDrawStyle::operator!=(const LineDrawStyle &other) const
{
  return !(*this == other);
}

/* Save all the settings of the statistics type that have changed from the initial state
 */
void StatisticsType::saveToPlaylist(YUViewDomElement &root) const
{
  bool allValuesIdenticalToInitialValues = (init.render == this->render &&                       //
                                            init.alphaFactor == this->alphaFactor &&             //
                                            init.valueDataOptions == this->valueDataOptions &&   //
                                            init.vectorDataOptions == this->vectorDataOptions && //
                                            init.gridOptions == this->gridOptions);

  if (allValuesIdenticalToInitialValues)
    return;

  YUViewDomElement newChild = root.ownerDocument().createElement(QString("statType%1").arg(typeID));
  newChild.appendChild(root.ownerDocument().createTextNode(QString::fromStdString(typeName)));

  // Append only the parameters that changed
  if (init.render != render)
    newChild.setAttribute("render", render);
  if (init.alphaFactor != alphaFactor)
    newChild.setAttribute("alphaFactor", alphaFactor);

  addModifiedValuesToElement(newChild, this->valueDataOptions, this->init.valueDataOptions);
  addModifiedValuesToElement(newChild, this->vectorDataOptions, this->init.vectorDataOptions);
  addModifiedValuesToElement(newChild, this->gridOptions, this->init.gridOptions);

  root.appendChild(newChild);
}

void StatisticsType::tryToLoadFromPlaylist(const YUViewDomElement &root)
{
  const auto [name, attributes] =
      root.findChildValueWithAttributes(QString("statType%1").arg(this->typeID));

  if (name.toStdString() != this->typeName)
    // The name of this type with the right ID and the name in the playlist don't match?...
    return;

  // Parse and set all the attributes that are in the playlist
  for (const auto [name, value] : attributes)
  {
    if (name == "render")
      this->render = (value != "0");
    else if (name == "alphaFactor")
      this->alphaFactor = value.toInt();
    else if (name == "renderValueData" || name == "scaleValueToBlockSize" ||
             name == "colorMapperType")
    {
      if (!this->valueDataOptions)
        this->valueDataOptions = ValueDataOptions();

      if (name == "renderValueData")
        this->valueDataOptions->render = (value != "0");
      else if (name == "scaleValueToBlockSize")
        this->valueDataOptions->scaleToBlockSize = (value != "0");
      else if (name == "colorMapperType")
        this->valueDataOptions->colorMapper.loadPlaylist(attributes);
    }
    else if (name == "renderVectorData" || name == "renderVectorDataValues" ||
             name == "scaleVectorToZoom" || name == "vectorStyle" || name == "vectorScale" ||
             name == "mapVectorToColor" || name == "renderarrowHead")
    {
      if (!this->vectorDataOptions)
        this->vectorDataOptions = VectorDataOptions();

      if (name == "renderVectorData")
        this->vectorDataOptions->render = (value != "0");
      else if (name == "renderVectorDataValues")
        this->vectorDataOptions->renderDataValues = (value != "0");
      else if (name == "scaleVectorToZoom")
        this->vectorDataOptions->scaleToZoom = (value != "0");
      else if (name == "vectorStyle")
        this->vectorDataOptions->style = convertStringToPen(value);
      else if (name == "vectorScale")
        this->vectorDataOptions->scale = value.toInt();
      else if (name == "mapVectorToColor")
        this->vectorDataOptions->mapToColor = (value != "0");
      else if (name == "renderarrowHead")
      {
        const auto idx = value.toInt();
        if (idx >= 0 && unsigned(idx) < AllArrowHeads.size())
          this->vectorDataOptions->arrowHead = AllArrowHeads.at(idx);
      }
    }
    else if (name == "renderGrid")
      this->gridOptions.render = (value != "0");
    else if (name == "gridPen")
      this->gridOptions.style = convertStringToPen(value);
    else if (name == "scaleGridToZoom")
      this->gridOptions.scaleToZoom = (value != "0");
  }

  this->saveInitialState();
}

std::string StatisticsType::getValueText(const int val) const
{
  if (this->valuesToText.contains(val))
    return this->valuesToText.at(val) + " (" + std::to_string(val) + ")";

  return std::to_string(val);
}

void StatisticsType::setMappingValues(std::vector<std::string> values)
{
  // We assume linear increasing typed IDs
  for (int i = 0; i < int(values.size()); i++)
    this->valuesToText[i] = values[i];
}

bool StatisticsType::ValueDataOptions::operator==(const ValueDataOptions &rhs) const
{
  return this->render == rhs.render &&                     //
         this->scaleToBlockSize == rhs.scaleToBlockSize && //
         this->colorMapper == rhs.colorMapper;
}

bool StatisticsType::VectorDataOptions::operator==(const VectorDataOptions &rhs) const
{
  return this->render == rhs.render &&                     //
         this->renderDataValues == rhs.renderDataValues && //
         this->scaleToZoom == rhs.scaleToZoom &&           //
         this->style == rhs.style &&                       //
         this->scale == rhs.scale &&                       //
         this->mapToColor == rhs.mapToColor &&             //
         this->arrowHead == rhs.arrowHead;
}

bool StatisticsType::GridOptions::operator==(const GridOptions &rhs) const
{
  return this->render == rhs.render && //
         this->style == rhs.style &&   //
         this->scaleToZoom == rhs.scaleToZoom;
}

StatisticsType::StatisticsType(int typeId, std::string typeName)
    : typeID(typeId), typeName(std::move(typeName))
{
}

void StatisticsType::saveInitialState()
{
  this->init.render      = this->render;
  this->init.alphaFactor = this->alphaFactor;

  this->init.valueDataOptions  = this->valueDataOptions;
  this->init.vectorDataOptions = this->vectorDataOptions;
  this->init.gridOptions       = this->gridOptions;
}

} // namespace stats
