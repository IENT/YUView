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

#pragma once

#include "Common.h"

#include <statistics/ColorMapper.h>
#include <statistics/StatisticsType.h>

namespace stats
{

namespace color
{

void PrintTo(const ColorMap &colorMap, std::ostream *os)
{
  *os << "ColorMapper: {";
  for (const auto &[value, color] : colorMap)
  {
    *os << "Value " << value << " ";
    PrintTo(color, os);
    *os << " ";
  }
  *os << "}";
}

void PrintTo(const ColorMapper &colorMapper, std::ostream *os)
{
  *os << "ColorMapper: {";

  *os << "MappingType " << MappingTypeMapper.getName(colorMapper.mappingType);
  *os << " valueRange ";
  PrintTo(colorMapper.valueRange, os);
  *os << " gradientColorStart ";
  PrintTo(colorMapper.gradientColorStart, os);
  *os << " gradientColorEnd ";
  PrintTo(colorMapper.gradientColorEnd, os);
  *os << " colorMap ";
  PrintTo(colorMapper.colorMap, os);
  *os << " colorMapOther ";
  PrintTo(colorMapper.colorMapOther, os);
  *os << " predefinedType " << PredefinedTypeMapper.getName(colorMapper.predefinedType);

  *os << "}";
}

} // namespace color

void PrintTo(const LineDrawStyle &style, std::ostream *os)
{
  *os << "LineDrawStyle: {";

  *os << "color ";
  PrintTo(style.color, os);
  *os << " width " << style.width;
  *os << " pattern " << PatternMapper.getName(style.pattern);

  *os << "}";
}

void PrintTo(const StatisticsType::ValueDataOptions &options, std::ostream *os)
{
  *os << "ValueDataOptions: {";

  *os << "Render ";
  PrintTo(options.render, os);
  *os << " ScaleToBlockSize ";
  PrintTo(options.scaleToBlockSize, os);
  *os << " ColorMapper ";
  PrintTo(options.colorMapper, os);

  *os << "}";
}

void PrintTo(const StatisticsType::VectorDataOptions &options, std::ostream *os)
{
  *os << "VectorDataOptions: {";

  *os << "render ";
  PrintTo(options.render, os);
  *os << " renderDataValues ";
  PrintTo(options.renderDataValues, os);
  *os << " scaleToZoom ";
  PrintTo(options.scaleToZoom, os);
  *os << " style ";
  PrintTo(options.style, os);
  *os << " scale ";
  PrintTo(options.scale, os);
  *os << " mapToColor ";
  PrintTo(options.mapToColor, os);
  *os << " arrowHead " << ArrowHeadMapper.getName(options.arrowHead);

  *os << "}";
}

void PrintTo(const StatisticsType::GridOptions &options, std::ostream *os)
{
  *os << "GridOptions: {";

  *os << "render ";
  PrintTo(options.render, os);
  *os << " style ";
  PrintTo(options.style, os);
  *os << " scaleToZoom ";
  PrintTo(options.scaleToZoom, os);

  *os << "}";
}

void PrintTo(const StatisticsType &type, std::ostream *os)
{
  *os << "StatisticsType: {";
  *os << "TypeID " << type.getTypeID();
  *os << " Name " << type.getTypeName();
  *os << " Description " << type.getDescription();

  *os << " Render ";
  PrintTo(type.render, os);
  *os << " AlphaFactor ";
  PrintTo(type.alphaFactor, os);

  if (type.valueDataOptions)
    PrintTo(type.valueDataOptions.value(), os);
  if (type.vectorDataOptions)
    PrintTo(type.vectorDataOptions.value(), os);
  PrintTo(type.gridOptions, os);

  *os << "}";
}

} // namespace stats
