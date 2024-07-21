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

#include <common/Color.h>
#include <common/NewEnumMapper.h>
#include <common/Typedef.h>
#include <common/YUViewDomElement.h>

#include <map>

namespace stats::color
{

/* This class knows how to map values to color.
 * There are 3 types of mapping:
 * 1: gradient - We use a min and max value (rangeMin, rangeMax) and two colors (minColor,
 *    maxColor). getColor(rangeMin) will return minColor. getColor(rangeMax) will return maxColor.
 *    In between, we perform linear interpolation. //
 * 2: map - We use a fixed map to map values (int) to color. The values are stored in colorMap.
 * 3: complex - We use a specific complex color gradient for values from rangeMin to rangeMax.
 *    They are similar to the ones used in MATLAB. The are set by name. supportedComplexTypes has a
 *    list of all supported types.
 */

enum class PredefinedType
{
  Jet,
  Heat,
  Hsv,
  Shuffle,
  Hot,
  Cool,
  Spring,
  Summer,
  Autumn,
  Winter,
  Gray,
  Bone,
  Copper,
  Pink,
  Lines,
  Col3_gblr,
  Col3_gwr,
  Col3_bblr,
  Col3_bwr,
  Col3_bblg,
  Col3_bwg
};

constexpr NewEnumMapper<PredefinedType, 21>
    PredefinedTypeMapper(std::make_pair(PredefinedType::Jet, "Jet"sv),
                         std::make_pair(PredefinedType::Heat, "Heat"sv),
                         std::make_pair(PredefinedType::Hsv, "Hsv"sv),
                         std::make_pair(PredefinedType::Shuffle, "Shuffle"sv),
                         std::make_pair(PredefinedType::Hot, "Hot"sv),
                         std::make_pair(PredefinedType::Cool, "Cool"sv),
                         std::make_pair(PredefinedType::Spring, "Spring"sv),
                         std::make_pair(PredefinedType::Summer, "Summer"sv),
                         std::make_pair(PredefinedType::Autumn, "Autumn"sv),
                         std::make_pair(PredefinedType::Winter, "Winter"sv),
                         std::make_pair(PredefinedType::Gray, "Gray"sv),
                         std::make_pair(PredefinedType::Bone, "Bone"sv),
                         std::make_pair(PredefinedType::Copper, "Copper"sv),
                         std::make_pair(PredefinedType::Pink, "Pink"sv),
                         std::make_pair(PredefinedType::Lines, "Lines"sv),
                         std::make_pair(PredefinedType::Col3_gblr, "Col3_gblr"sv),
                         std::make_pair(PredefinedType::Col3_gwr, "Col3_gwr"sv),
                         std::make_pair(PredefinedType::Col3_bblr, "Col3_bblr"sv),
                         std::make_pair(PredefinedType::Col3_bwr, "Col3_bwr"sv),
                         std::make_pair(PredefinedType::Col3_bblg, "Col3_bblg"sv),
                         std::make_pair(PredefinedType::Col3_bwg, "Col3_bwg"sv));

enum class MappingType
{
  Gradient,
  Map,
  Predefined
};

constexpr NewEnumMapper<MappingType, 3>
    MappingTypeMapper(std::make_pair(MappingType::Gradient, "Gradient"sv),
                      std::make_pair(MappingType::Map, "Map"sv),
                      std::make_pair(MappingType::Predefined, "Predefined"sv));

class ColorMapper
{
public:
  ColorMapper() = default;
  ColorMapper(Range<int> valueRange, Color gradientColorStart, Color gradientColorEnd);
  ColorMapper(const ColorMap &colorMap, Color other);
  ColorMapper(Range<int> valueRange, PredefinedType predefinedType);
  ColorMapper(Range<int> valueRange, std::string predefinedTypeName);

  Color getColor(int value) const;
  Color getColor(double value) const;

  void savePlaylist(YUViewDomElement &root) const;
  void loadPlaylist(const QStringPairList &attributes);

  // Two colorMappers are identical if they will return the same color when asked for any value.
  // When changing the type of one of the mappers, this might not be true anymore.
  bool operator!=(const ColorMapper &other) const;

  MappingType mappingType{MappingType::Predefined};

  Range<int>     valueRange{};
  Color          gradientColorStart{0, 0, 0};
  Color          gradientColorEnd{0, 0, 255};
  ColorMap       colorMap;
  Color          colorMapOther{};
  PredefinedType predefinedType{PredefinedType::Jet};
};

} // namespace stats::color
