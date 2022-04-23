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

#include "ColorMapper.h"

#include <common/Typedef.h>

#include <QStringList>
#include <random>

namespace stats::color
{

namespace
{

struct RGBA
{
  double r{1.0};
  double g{1.0};
  double b{1.0};
  double a{1.0};
};

RGBA applyIMapping(int i, double k, double n)
{
  if (i == 0)
    return RGBA({1.0, k, 0.0, 1.0});
  if (i == 1)
    return RGBA({n, 1.0, 0.0, 1.0});
  if (i == 2)
    return RGBA({0.0, 1.0, k, 1.0});
  if (i == 3)
    return RGBA({0.0, n, 1.0, 1.0});
  if (i == 4)
    return RGBA({k, 0.0, 1.0, 1.0});
  if (i == 5)
    return RGBA({1.0, 0.0, n, 1.0});
  return {};
}

std::string rangeToString(const Range<int> range)
{
  return std::to_string(range.min) + "|" + std::to_string(range.max);
}

Range<int> rangeFromString(std::string rangeText)
{
  auto seperator = rangeText.find("|");
  if (seperator == std::string::npos)
    return {};

  auto firstStr  = rangeText.substr(0, seperator);
  auto secondStr = rangeText.substr(seperator + 1);

  try
  {
    auto first  = std::stoi(firstStr);
    auto second = std::stoi(secondStr);
    return {first, second};
  }
  catch (const std::exception)
  {
    return {};
  }
}

} // namespace

ColorMapper::ColorMapper(Range<int> valueRange, Color gradientColorStart, Color gradientColorEnd)
{
  this->mappingType        = MappingType::Gradient;
  this->valueRange         = valueRange;
  this->gradientColorStart = gradientColorStart;
  this->gradientColorEnd   = gradientColorEnd;
}

ColorMapper::ColorMapper(const ColorMap &colorMap, Color other)
{
  this->mappingType   = MappingType::Map;
  this->colorMap      = colorMap;
  this->colorMapOther = other;
}

ColorMapper::ColorMapper(Range<int> valueRange, PredefinedType predefinedType)
{
  this->mappingType    = MappingType::Predefined;
  this->valueRange     = valueRange;
  this->predefinedType = predefinedType;
}

ColorMapper::ColorMapper(Range<int> valueRange, std::string predefinedTypeName)
{
  this->mappingType = MappingType::Predefined;
  this->valueRange  = valueRange;
  if (auto type = PredefinedTypeMapper.getValue(predefinedTypeName))
    this->predefinedType = *type;
}

Color ColorMapper::getColor(int value) const
{
  if (this->mappingType == MappingType::Map)
  {
    if (this->colorMap.count(value) > 0)
      return this->colorMap.at(value);
    else
      return this->colorMapOther;
  }
  else
    return this->getColor(double(value));
}

Color ColorMapper::getColor(double value) const
{
  if (this->mappingType == MappingType::Map)
    return this->getColor(int(value + 0.5));

  value           = functions::clip(value, this->valueRange);
  auto rangeWidth = double(this->valueRange.max) - double(this->valueRange.min);

  if (mappingType == MappingType::Gradient)
  {
    // The value scaled from 0 to 1 within the range (rangeMin ... rangeMax)
    auto valScaled = (value - this->valueRange.min) / rangeWidth;

    auto interpolate = [&valScaled](int start, int end) {
      auto range       = end - start;
      auto rangeScaled = std::floor(valScaled * double(range) + 0.5);
      return start + int(rangeScaled);
    };
    auto retR = interpolate(this->gradientColorStart.R(), this->gradientColorEnd.R());
    auto retG = interpolate(this->gradientColorStart.G(), this->gradientColorEnd.G());
    auto retB = interpolate(this->gradientColorStart.B(), this->gradientColorEnd.B());
    auto retA = interpolate(this->gradientColorStart.A(), this->gradientColorEnd.A());

    return Color(retR, retG, retB, retA);
  }
  else if (mappingType == MappingType::Predefined)
  {
    auto x = (value - this->valueRange.min) / rangeWidth;

    RGBA rgba{};

    if (this->predefinedType == PredefinedType::Jet)
    {
      if ((x >= 3.0 / 8.0) && (x < 5.0 / 8.0))
        rgba.r = (4.0 * x - 3.0 / 2.0);
      else if ((x >= 5.0 / 8.0) && (x < 7.0 / 8.0))
        rgba.r = 1.0;
      else if (x >= 7.0 / 8.0)
        rgba.r = (-4.0 * x + 9.0 / 2.0);
      else
        rgba.r = 0.0;
      if ((x >= 1.0 / 8.0) && (x < 3.0 / 8.0))
        rgba.g = (4.0 * x - 1.0 / 2.0);
      else if ((x >= 3.0 / 8.0) && (x < 5.0 / 8.0))
        rgba.g = 1.0;
      else if ((x >= 5.0 / 8.0) && (x < 7.0 / 8.0))
        rgba.g = (-4.0 * x + 7.0 / 2.0);
      else
        rgba.g = 0.0;
      if (x < 1.0 / 8.0)
        rgba.b = (4.0 * x + 1.0 / 2.0);
      else if ((x >= 1.0 / 8.0) && (x < 3.0 / 8.0))
        rgba.b = 1.0;
      else if ((x >= 3.0 / 8.0) & (x < 5.0 / 8.0))
        rgba.b = (-4.0 * x + 5.0 / 2.0);
      else
        rgba.b = 0.0;
    }
    else if (this->predefinedType == PredefinedType::Heat)
      rgba = RGBA({1.0, 0.0, 0.0, x});
    else if (this->predefinedType == PredefinedType::Hsv)
    {
      // h = x, s = 1, v = 1
      if (x >= 1.0)
        x = 0.0;
      x      = x * 6.0;
      auto i = (int)x; /* should be in the range 0..5 */
      auto f = x - i;  /* fractional part */

      auto n = (1.0 - 1.0 * f);
      auto k = (1.0 - 1.0 * (1 - f));

      rgba = applyIMapping(i, k, n);
    }
    else if (this->predefinedType == PredefinedType::Shuffle)
    {
      // randomly remap the x value, but always with the same random seed
      unsigned         seed = 42;
      std::vector<int> randomMap;
      for (int val = 0; val <= rangeWidth; ++val)
        randomMap.push_back(val);
      std::shuffle(randomMap.begin(), randomMap.end(), std::default_random_engine(seed));

      auto valueInt    = functions::clip(int(value) - this->valueRange.min, this->valueRange);
      auto remainder   = value - valueInt;
      auto valueMapped = randomMap[valueInt] + remainder;
      auto x           = valueMapped / rangeWidth;

      // h = x, s = 1, v = 1
      if (x >= 1.0)
        x = 0.0;
      x      = x * 6.0;
      auto i = (int)x; /* should be in the range 0..5 */
      auto f = x - i;  /* fractional part */

      auto n = (1.0 - 1.0 * f);
      auto k = (1.0 - 1.0 * (1 - f));

      rgba = applyIMapping(i, k, n);
    }
    else if (this->predefinedType == PredefinedType::Hot)
    {
      if (x < 2.0 / 5.0)
        rgba.r = (5.0 / 2.0 * x);
      else
        rgba.r = 1.0;
      if ((x >= 2.0 / 5.0) && (x < 4.0 / 5.0))
        rgba.g = (5.0 / 2.0 * x - 1);
      else if (x >= 4.0 / 5.0)
        rgba.g = 1.0;
      else
        rgba.g = 0.0;
      if (x >= 4.0 / 5.0)
        rgba.b = (5.0 * x - 4.0);
      else
        rgba.b = 0.0;
    }
    else if (this->predefinedType == PredefinedType::Cool)
      rgba = RGBA({x, 1.0 - x, 1.0, 1.0});
    else if (this->predefinedType == PredefinedType::Spring)
      rgba = RGBA({1.0, x, 1.0 - x, 1.0});
    else if (this->predefinedType == PredefinedType::Summer)
      rgba = RGBA({x, 0.5 + x / 2.0, 0.4, 1.0});
    else if (this->predefinedType == PredefinedType::Autumn)
      rgba = RGBA({1.0, x, 0.0, 1.0});
    else if (this->predefinedType == PredefinedType::Winter)
      rgba = RGBA({0.0, x, 1.0 - x / 2.0, 1.0});
    else if (this->predefinedType == PredefinedType::Gray)
      rgba = RGBA({x, x, x, 1.0});
    else if (this->predefinedType == PredefinedType::Bone)
    {
      if (x < 3.0 / 4.0)
        rgba.r = (7.0 / 8.0 * x);
      else if (x >= 3.0 / 4.0)
        rgba.r = (11.0 / 8.0 * x - 3.0 / 8.0);
      if (x < 3.0 / 8.0)
        rgba.g = (7.0 / 8.0 * x);
      else if ((x >= 3.0 / 8.0) && (x < 3.0 / 4.0))
        rgba.g = (29.0 / 24.0 * x - 1.0 / 8.0);
      else if (x >= 3.0 / 4.0)
        rgba.g = (7.0 / 8.0 * x + 1.0 / 8.0);
      if (x < 3.0 / 8.0)
        rgba.b = (29.0 / 24.0 * x);
      else if (x >= 3.0 / 8.0)
        rgba.b = (7.0 / 8.0 * x + 1.0 / 8.0);
    }
    else if (this->predefinedType == PredefinedType::Copper)
    {
      if (x < 4.0 / 5.0)
        rgba.r = 5.0 / 4.0 * x;
      else
        rgba.r = 1.0;
      rgba.g = 4.0 / 5.0 * x;
      rgba.b = 1.0 / 2.0 * x;
    }
    else if (this->predefinedType == PredefinedType::Pink)
    {
      if (x < 3.0 / 8.0)
        rgba.r = (14.0 / 9.0 * x);
      else if (x >= 3.0 / 8.0)
        rgba.r = (2.0 / 3.0 * x + 1.0 / 3.0);
      if (x < 3.0 / 8.0)
        rgba.g = (2.0 / 3.0 * x);
      else if ((x >= 3.0 / 8.0) && (x < 3.0 / 4.0))
        rgba.g = (14.0 / 9.0 * x - 1.0 / 3.0);
      else if (x >= 3.0 / 4.0)
        rgba.g = (2.0 / 3.0 * x + 1.0 / 3.0);
      if (x < 3.0 / 4.0)
        rgba.b = (2.0 / 3.0 * x);
      else if (x >= 3.0 / 4.0)
        rgba.b = (2.0 * x - 1.0);
    }
    else if (this->predefinedType == PredefinedType::Lines)
    {
      if (x >= 1.0)
        x = 0.0;
      x      = x * 7.0;
      auto i = (int)x;

      if (i == 0)
        rgba = RGBA({0.0, 0.0, 1.0, 1.0});
      if (i == 1)
        rgba = RGBA({0.0, 0.5, 0.0, 1.0});
      if (i == 2)
        rgba = RGBA({1.0, 0.0, 0.0, 1.0});
      if (i == 3)
        rgba = RGBA({0.0, 0.75, 0.75, 1.0});
      if (i == 4)
        rgba = RGBA({0.75, 0.0, 0.75, 1.0});
      if (i == 5)
        rgba = RGBA({0.75, 0.75, 0.0, 1.0});
      if (i == 6)
        rgba = RGBA({0.25, 0.25, 0.25, 1.0});
    }
    else if (this->predefinedType == PredefinedType::Col3_gblr)
    {
      // 3 colors: green, black, red
      rgba.r = (x < 0.5) ? 0.0 : (x - 0.5) * 2;
      rgba.g = (x < 0.5) ? (0.5 - x) * 2 : 0.0;
      rgba.b = 0.0;
    }
    else if (this->predefinedType == PredefinedType::Col3_gwr)
    {
      // 3 colors: green, white, red
      rgba.r = (x < 0.5) ? x * 2 : 1.0;
      rgba.g = (x < 0.5) ? 1.0 : (1 - x) * 2;
      rgba.b = (x < 0.5) ? x * 2 : (1 - x) * 2;
    }
    else if (this->predefinedType == PredefinedType::Col3_bblr)
    {
      // 3 colors: blue,  black, red
      rgba.r = (x < 0.5) ? 0.0 : (x - 0.5) * 2;
      rgba.g = 0.0;
      rgba.b = (x < 0.5) ? 1.0 - 2 * x : 0.0;
    }
    else if (this->predefinedType == PredefinedType::Col3_bwr)
    {
      // 3 colors: blue,  white, red
      rgba.r = (x < 0.5) ? x * 2 : 1.0;
      rgba.g = (x < 0.5) ? x * 2 : (1 - x) * 2;
      rgba.b = (x < 0.5) ? 1.0 : (1 - x) * 2;
    }
    else if (this->predefinedType == PredefinedType::Col3_bblg)
    {
      // 3 colors: blue,  black, green
      rgba.r = 0.0;
      rgba.g = (x < 0.5) ? 0.0 : (x - 0.5) * 2;
      rgba.b = (x < 0.5) ? 1.0 - 2 * x : 0.0;
    }
    else if (this->predefinedType == PredefinedType::Col3_bwg)
    {
      // 3 colors: blue,  white, green
      rgba.r = (x < 0.5) ? x * 2 : (1 - x) * 2;
      rgba.g = (x < 0.5) ? x * 2 : 1.0;
      rgba.b = (x < 0.5) ? 1.0 : (1 - x) * 2;
    }

    auto retR = functions::clip(int(std::floor(rgba.r * 255.0 + 0.5)), 0, 255);
    auto retG = functions::clip(int(std::floor(rgba.g * 255.0 + 0.5)), 0, 255);
    auto retB = functions::clip(int(std::floor(rgba.b * 255.0 + 0.5)), 0, 255);
    auto retA = functions::clip(int(std::floor(rgba.a * 255.0 + 0.5)), 0, 255);

    return Color(retR, retG, retB, retA);
  }

  return {};
}

void ColorMapper::savePlaylist(YUViewDomElement &root) const
{
  root.setAttribute("colorMapperType", MappingTypeMapper.getName(this->mappingType));
  if (this->mappingType == MappingType::Gradient)
  {
    root.setAttribute("colorMapperGradientStart", this->gradientColorStart.toHex());
    root.setAttribute("colorMapperGradientEnd", this->gradientColorEnd.toHex());
  }
  if (this->mappingType == MappingType::Gradient || this->mappingType == MappingType::Predefined)
    root.setAttribute("colorMapperRange", rangeToString(this->valueRange));
  if (this->mappingType == MappingType::Predefined)
    root.setAttribute("colorMapperPredefinedType",
                      PredefinedTypeMapper.getName(this->predefinedType));
  if (this->mappingType == MappingType::Map)
    for (auto &[key, value] : this->colorMap)
      root.setAttribute("colorMapperMapValue" + std::to_string(key), value.toHex());
}

void ColorMapper::loadPlaylist(const QStringPairList &attributes)
{
  for (auto attribute : attributes)
  {
    if (attribute.first == "colorMapperType")
    {
      bool ok    = false;
      auto value = attribute.second.toInt(&ok);
      if (ok)
      {
        if (auto type = MappingTypeMapper.at(value))
          this->mappingType = *type;
      }
      else if (auto type = MappingTypeMapper.getValue(attribute.second.toStdString()))
        this->mappingType = *type;
    }
    else if (attribute.first == "colorMapperMinColor" ||
             attribute.first == "colorMapperGradientStart")
      this->gradientColorStart = Color(attribute.second.toStdString());
    else if (attribute.first == "colorMapperMaxColor" ||
             attribute.first == "colorMapperGradientEnd")
      this->gradientColorEnd = Color(attribute.second.toStdString());
    else if (attribute.first == "colorMapperRangeMin")
      this->valueRange.min = attribute.second.toInt();
    else if (attribute.first == "colorMapperRangeMax")
      this->valueRange.max = attribute.second.toInt();
    else if (attribute.first == "colorMapperRange")
      this->valueRange = rangeFromString(attribute.second.toStdString());
    else if (attribute.first == "colorMapperComplexType" ||
             attribute.first == "colorMapperPredefinedType")
    {
      if (auto type = PredefinedTypeMapper.getValue(attribute.second.toStdString()))
        this->predefinedType = *type;
    }
    else if (attribute.first.startsWith("colorMapperMapValue"))
    {
      auto key            = attribute.first.mid(19).toInt();
      auto value          = Color(attribute.second.toStdString());
      this->colorMap[key] = value;
    }
  }
}

bool ColorMapper::operator!=(const ColorMapper &other) const
{
  if (this->mappingType != other.mappingType)
    return true;
  if (this->mappingType == MappingType::Gradient)
    return this->valueRange != other.valueRange ||
           this->gradientColorStart != other.gradientColorStart ||
           this->gradientColorEnd != other.gradientColorEnd;
  if (this->mappingType == MappingType::Map)
    return this->colorMap != other.colorMap;
  if (this->mappingType == MappingType::Predefined)
    return this->valueRange != other.valueRange || this->predefinedType != other.predefinedType;
  return false;
}

} // namespace stats::color
