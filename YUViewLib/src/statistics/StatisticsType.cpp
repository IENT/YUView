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

bool LineDrawStyle::operator==(const LineDrawStyle &other) const
{
  return color == other.color && width == other.width && pattern == other.pattern;
}

bool LineDrawStyle::operator!=(const LineDrawStyle &other) const
{
  return !(*this == other);
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

bool StatisticsType::ValueDataOptions::wasModified() const
{
  return this->render.wasModified() ||           //
         this->scaleToBlockSize.wasModified() || //
         this->colorMapper.wasModified();
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
  this->render.setUnmodified();
  this->alphaFactor.setUnmodified();
  if (this->valueDataOptions)
  {
    this->valueDataOptions->render.setUnmodified();
    this->valueDataOptions->scaleToBlockSize.setUnmodified();
    this->valueDataOptions->colorMapper.setUnmodified();
  }

  this->init.vectorDataOptions = this->vectorDataOptions;
  this->init.gridOptions       = this->gridOptions;
}

} // namespace stats
