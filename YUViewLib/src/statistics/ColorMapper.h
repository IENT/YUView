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

#include "common/Color.h"

#include <QString>
#include <map>

namespace stats
{

/* This class knows how to map values to color.
 * There are 3 types of mapping:
 * 1: gradient - We use a min and max value (rangeMin, rangeMax) and two colors (minColor,
 * maxColor). getColor(rangeMin) will return minColor. getColor(rangeMax) will return maxColor. In
 * between, we perform linear interpolation. 2: map      - We use a fixed map to map values (int) to
 * color. The values are stored in colorMap. 3: complex  - We use a specific complex color gradient
 * for values from rangeMin to rangeMax. They are similar to the ones used in MATLAB. The are set by
 * name. supportedComplexTypes has a list of all supported types.
 */
class ColorMapper
{
public:
  ColorMapper() = default;
  ColorMapper(int min, const Color &colMin, int max, const Color &colMax);
  ColorMapper(const QString &rangeName, int min, int max);

  Color getColor(int value) const;
  Color getColor(float value) const;
  int   getMinVal() const;
  int   getMaxVal() const;

  // ID: 0:colorMapperGradient, 1:colorMapperMap, 2+:ColorMapperComplex
  int getID() const;

  int                  rangeMin{0};
  int                  rangeMax{0};
  Color                minColor{};
  Color                maxColor{};
  std::map<int, Color> colorMap;        // Each int is mapped to a specific color
  Color                colorMapOther{}; // All other values are mapped to this color
  QString              complexType{};

  // Two colorMappers are identical if they will return the same color when asked for any value.
  // When changing the type of one of the mappers, this might not be true anymore.
  bool operator!=(const ColorMapper &other) const;

  enum class MappingType
  {
    gradient,
    map,
    complex,
    none
  };
  static unsigned mappingTypeToUInt(MappingType mappingType);

  MappingType        mappingType{MappingType::none};
  static QStringList supportedComplexTypes;
};

} // namespace stats