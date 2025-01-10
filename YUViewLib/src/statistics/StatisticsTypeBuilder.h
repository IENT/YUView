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

#include <statistics/StatisticsType.h>

namespace stats
{

class StatisticsTypeBuilder
{
public:
  StatisticsTypeBuilder &withTypeID(const int typeID)
  {
    this->statisticsType.typeID = typeID;
    return *this;
  }

  StatisticsTypeBuilder &withTypeName(const std::string &typeName)
  {
    this->statisticsType.typeName = typeName;
    return *this;
  }

  StatisticsTypeBuilder &withDescription(const std::string &description)
  {
    this->statisticsType.description = description;
    return *this;
  }

  StatisticsTypeBuilder &withRender(const bool render)
  {
    this->statisticsType.render = render;
    return *this;
  }

  StatisticsTypeBuilder withAlphaFactor(int alphaFactor)
  {
    this->statisticsType.alphaFactor = alphaFactor;
    return *this;
  }

  StatisticsTypeBuilder
  withValueDataOptions(const StatisticsType::ValueDataOptions &valueDataOptions)
  {
    this->statisticsType.valueDataOptions = valueDataOptions;
    return *this;
  }

  StatisticsTypeBuilder
  withVectorDataOptions(const StatisticsType::VectorDataOptions &vectorDataOptions)
  {
    this->statisticsType.vectorDataOptions = vectorDataOptions;
    return *this;
  }

  StatisticsTypeBuilder withGridOptions(const StatisticsType::GridOptions &gridOptions)
  {
    this->statisticsType.gridOptions = gridOptions;
    return *this;
  }

  StatisticsTypeBuilder &withMappingValues(const std::vector<std::string> &mappingValues)
  {
    this->statisticsType.setMappingValues(mappingValues);
    return *this;
  }

  StatisticsTypeBuilder &withMappingValues(const std::initializer_list<const char *> &mappingValues)
  {
    std::vector<std::string> values;
    for (const auto &value : mappingValues)
      values.push_back(value);

    this->statisticsType.setMappingValues(values);
    return *this;
  }

  StatisticsType build()
  {
    this->statisticsType.setInitialState();
    return this->statisticsType;
  }

private:
  StatisticsType statisticsType;
};

} // namespace stats
