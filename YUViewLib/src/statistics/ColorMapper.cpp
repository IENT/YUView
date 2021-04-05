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

#include "common/typedef.h"

#include <QStringList>
#include <random>

namespace stats
{

// All types that are supported by the getColor() function.
QStringList ColorMapper::supportedComplexTypes = QStringList() << "jet"
                                                               << "heat"
                                                               << "hsv"
                                                               << "shuffle"
                                                               << "hot"
                                                               << "cool"
                                                               << "spring"
                                                               << "summer"
                                                               << "autumn"
                                                               << "winter"
                                                               << "gray"
                                                               << "bone"
                                                               << "copper"
                                                               << "pink"
                                                               << "lines"
                                                               << "col3_gblr"
                                                               << "col3_gwr"
                                                               << "col3_bblr"
                                                               << "col3_bwr"
                                                               << "col3_bblg"
                                                               << "col3_bwg";

// Setup an invalid (uninitialized color mapper)
ColorMapper::ColorMapper()
{
  rangeMin      = 0;
  rangeMax      = 0;
  colorMapOther = {};
  mappingType   = MappingType::none;
}

// Setup a color mapper with a gradient
ColorMapper::ColorMapper(int min, const Color &colMin, int max, const Color &colMax)
{
  rangeMin      = min;
  rangeMax      = max;
  minColor      = colMin;
  maxColor      = colMax;
  colorMapOther = {};
  mappingType   = MappingType::gradient;
}

ColorMapper::ColorMapper(const QString &rangeName, int min, int max)
{
  if (supportedComplexTypes.contains(rangeName))
  {
    rangeMin    = min;
    rangeMax    = max;
    complexType = rangeName;
    mappingType = MappingType::complex;
  }
  else
  {
    rangeMin    = 0;
    rangeMax    = 0;
    mappingType = MappingType::none;
  }
  colorMapOther = {};
}

Color ColorMapper::getColor(int value) const
{
  if (mappingType == MappingType::map)
  {
    if (colorMap.count(value) > 0)
      return colorMap.at(value);
    else
      return colorMapOther;
  }
  else
  {
    return getColor(float(value));
  }
}

Color ColorMapper::getColor(float value) const
{
  if (mappingType == MappingType::map)
    // Round and use the integer value to get the value from the map
    return getColor(int(value + 0.5));

  // clamp the value to [min max]
  if (value > rangeMax)
    value = rangeMax;
  if (value < rangeMin)
    value = rangeMin;

  if (mappingType == MappingType::gradient)
  {
    // The value scaled from 0 to 1 within the range (rangeMin ... rangeMax)
    float valScaled = (value - rangeMin) / (rangeMax - rangeMin);

    unsigned char retR =
        minColor.R() +
        (unsigned char)(floor(valScaled * (float)(maxColor.R() - minColor.R()) + 0.5f));
    unsigned char retG =
        minColor.G() +
        (unsigned char)(floor(valScaled * (float)(maxColor.G() - minColor.G()) + 0.5f));
    unsigned char retB =
        minColor.B() +
        (unsigned char)(floor(valScaled * (float)(maxColor.B() - minColor.B()) + 0.5f));
    unsigned char retA =
        minColor.A() +
        (unsigned char)(floor(valScaled * (float)(maxColor.A() - minColor.A()) + 0.5f));

    return Color(retR, retG, retB, retA);
  }
  else if (mappingType == MappingType::complex)
  {
    float x = (value - rangeMin) / (rangeMax - rangeMin);
    float r = 1, g = 1, b = 1, a = 1;

    if (complexType == "jet")
    {
      if ((x >= 3.0 / 8.0) && (x < 5.0 / 8.0))
        r = (4.0f * x - 3.0f / 2.0f);
      else if ((x >= 5.0 / 8.0) && (x < 7.0 / 8.0))
        r = 1.0;
      else if (x >= 7.0 / 8.0)
        r = (-4.0f * x + 9.0f / 2.0f);
      else
        r = 0.0f;
      if ((x >= 1.0 / 8.0) && (x < 3.0 / 8.0))
        g = (4.0f * x - 1.0f / 2.0f);
      else if ((x >= 3.0 / 8.0) && (x < 5.0 / 8.0))
        g = 1.0;
      else if ((x >= 5.0 / 8.0) && (x < 7.0 / 8.0))
        g = (-4.0f * x + 7.0f / 2.0f);
      else
        g = 0.0f;
      if (x < 1.0 / 8.0)
        b = (4.0f * x + 1.0f / 2.0f);
      else if ((x >= 1.0 / 8.0) && (x < 3.0 / 8.0))
        b = 1.0f;
      else if ((x >= 3.0 / 8.0) & (x < 5.0 / 8.0))
        b = (-4.0f * x + 5.0f / 2.0f);
      else
        b = 0.0f;
    }
    else if (complexType == "heat")
    {
      r = 1;
      g = 0;
      b = 0;
      a = x;
    }
    else if (complexType == "hsv")
    {
      // h = x, s = 1, v = 1
      if (x >= 1.0)
        x = 0.0;
      x       = x * 6.0f;
      int   I = (int)x; /* should be in the range 0..5 */
      float F = x - I;  /* fractional part */

      float N = (1.0f - 1.0f * F);
      float K = (1.0f - 1.0f * (1 - F));

      if (I == 0)
      {
        r = 1;
        g = K;
        b = 0;
      }
      if (I == 1)
      {
        r = N;
        g = 1.0;
        b = 0;
      }
      if (I == 2)
      {
        r = 0;
        g = 1.0;
        b = K;
      }
      if (I == 3)
      {
        r = 0;
        g = N;
        b = 1.0;
      }
      if (I == 4)
      {
        r = K;
        g = 0;
        b = 1.0;
      }
      if (I == 5)
      {
        r = 1.0;
        g = 0;
        b = N;
      }
    }
    else if (complexType == "shuffle")
    {
      int rangeSize = rangeMax - rangeMin + 1;
      // randomly remap the x value, but always with the same random seed
      unsigned         seed = 42;
      std::vector<int> randomMap;
      for (int val = 0; val < rangeSize; ++val)
      {
        randomMap.push_back(val);
      }
      shuffle(randomMap.begin(), randomMap.end(), std::default_random_engine(seed));

      int   valueInt    = clip(int(value - rangeMin), rangeMin, rangeMax);
      float rem         = value - valueInt;
      float valueMapped = randomMap[valueInt] + rem;

      float x = valueMapped / (rangeMax - rangeMin);

      // h = x, s = 1, v = 1
      if (x >= 1.0)
        x = 0.0;
      x       = x * 6.0f;
      int   I = (int)x; /* should be in the range 0..5 */
      float F = x - I;  /* fractional part */

      float N = (1.0f - 1.0f * F);
      float K = (1.0f - 1.0f * (1 - F));

      if (I == 0)
      {
        r = 1;
        g = K;
        b = 0;
      }
      if (I == 1)
      {
        r = N;
        g = 1.0;
        b = 0;
      }
      if (I == 2)
      {
        r = 0;
        g = 1.0;
        b = K;
      }
      if (I == 3)
      {
        r = 0;
        g = N;
        b = 1.0;
      }
      if (I == 4)
      {
        r = K;
        g = 0;
        b = 1.0;
      }
      if (I == 5)
      {
        r = 1.0;
        g = 0;
        b = N;
      }
    }
    else if (complexType == "hot")
    {
      if (x < 2.0 / 5.0)
        r = (5.0f / 2.0f * x);
      else
        r = 1.0f;
      if ((x >= 2.0 / 5.0) && (x < 4.0 / 5.0))
        g = (5.0f / 2.0f * x - 1);
      else if (x >= 4.0 / 5.0)
        g = 1.0f;
      else
        g = 0.0f;
      if (x >= 4.0 / 5.0)
        b = (5.0f * x - 4.0f);
      else
        b = 0.0f;
    }
    else if (complexType == "cool")
    {
      r = x;
      g = 1.0f - x;
      b = 1.0;
    }
    else if (complexType == "spring")
    {
      r = 1.0f;
      g = x;
      b = 1.0f - x;
    }
    else if (complexType == "summer")
    {
      r = x;
      g = 0.5f + x / 2.0f;
      b = 0.4f;
    }
    else if (complexType == "autumn")
    {
      r = 1.0;
      g = x;
      b = 0.0;
    }
    else if (complexType == "winter")
    {
      r = 0;
      g = x;
      b = 1.0f - x / 2.0f;
    }
    else if (complexType == "gray")
    {
      r = x;
      g = x;
      b = x;
    }
    else if (complexType == "bone")
    {
      if (x < 3.0 / 4.0)
        r = (7.0f / 8.0f * x);
      else if (x >= 3.0 / 4.0)
        r = (11.0f / 8.0f * x - 3.0f / 8.0f);
      if (x < 3.0 / 8.0)
        g = (7.0f / 8.0f * x);
      else if ((x >= 3.0 / 8.0) && (x < 3.0 / 4.0))
        g = (29.0f / 24.0f * x - 1.0f / 8.0f);
      else if (x >= 3.0 / 4.0)
        g = (7.0f / 8.0f * x + 1.0f / 8.0f);
      if (x < 3.0 / 8.0)
        b = (29.0f / 24.0f * x);
      else if (x >= 3.0 / 8.0)
        b = (7.0f / 8.0f * x + 1.0f / 8.0f);
    }
    else if (complexType == "copper")
    {
      if (x < 4.0 / 5.0)
        r = (5.0f / 4.0f * x);
      else
        r = 1.0f;
      g = 4.0f / 5.0f * x;
      b = 1.0f / 2.0f * x;
    }
    else if (complexType == "pink")
    {
      if (x < 3.0 / 8.0)
        r = (14.0f / 9.0f * x);
      else if (x >= 3.0 / 8.0)
        r = (2.0f / 3.0f * x + 1.0f / 3.0f);
      if (x < 3.0 / 8.0)
        g = (2.0f / 3.0f * x);
      else if ((x >= 3.0 / 8.0) && (x < 3.0 / 4.0))
        g = (14.0f / 9.0f * x - 1.0f / 3.0f);
      else if (x >= 3.0 / 4.0)
        g = (2.0f / 3.0f * x + 1.0f / 3.0f);
      if (x < 3.0 / 4.0)
        b = (2.0f / 3.0f * x);
      else if (x >= 3.0 / 4.0)
        b = (2.0f * x - 1.0f);
    }
    else if (complexType == "lines")
    {
      if (x >= 1.0)
        x = 0.0f;
      x     = x * 7.0f;
      int I = (int)x;

      if (I == 0)
      {
        r = 0.0;
        g = 0.0;
        b = 1.0;
      }
      if (I == 1)
      {
        r = 0.0;
        g = 0.5;
        b = 0.0;
      }
      if (I == 2)
      {
        r = 1.0;
        g = 0.0;
        b = 0.0;
      }
      if (I == 3)
      {
        r = 0.0;
        g = 0.75;
        b = 0.75;
      }
      if (I == 4)
      {
        r = 0.75;
        g = 0.0;
        b = 0.75;
      }
      if (I == 5)
      {
        r = 0.75;
        g = 0.75;
        b = 0.0;
      }
      if (I == 6)
      {
        r = 0.25;
        g = 0.25;
        b = 0.25;
      }
    }
    else if (complexType == "col3_gblr")
    {
      // 3 colors: green, black, red
      r = (x < 0.5) ? 0.0 : (x - 0.5) * 2;
      g = (x < 0.5) ? (0.5 - x) * 2 : 0.0;
      b = 0.0;
    }
    else if (complexType == "col3_gwr")
    {
      // 3 colors: green, white, red
      r = (x < 0.5) ? x * 2 : 1.0;
      g = (x < 0.5) ? 1.0 : (1 - x) * 2;
      b = (x < 0.5) ? x * 2 : (1 - x) * 2;
    }
    else if (complexType == "col3_bblr")
    {
      // 3 colors: blue,  black, red
      r = (x < 0.5) ? 0.0 : (x - 0.5) * 2;
      g = 0.0;
      b = (x < 0.5) ? 1.0 - 2 * x : 0.0;
    }
    else if (complexType == "col3_bwr")
    {
      // 3 colors: blue,  white, red
      r = (x < 0.5) ? x * 2 : 1.0;
      g = (x < 0.5) ? x * 2 : (1 - x) * 2;
      b = (x < 0.5) ? 1.0 : (1 - x) * 2;
    }
    else if (complexType == "col3_bblg")
    {
      // 3 colors: blue,  black, green
      r = 0.0;
      g = (x < 0.5) ? 0.0 : (x - 0.5) * 2;
      b = (x < 0.5) ? 1.0 - 2 * x : 0.0;
    }
    else if (complexType == "col3_bwg")
    {
      // 3 colors: blue,  white, green
      r = (x < 0.5) ? x * 2 : (1 - x) * 2;
      g = (x < 0.5) ? x * 2 : 1.0;
      b = (x < 0.5) ? 1.0 : (1 - x) * 2;
    }

    unsigned char retR = (unsigned char)(floor(r * 255.0f + 0.5f));
    unsigned char retG = (unsigned char)(floor(g * 255.0f + 0.5f));
    unsigned char retB = (unsigned char)(floor(b * 255.0f + 0.5f));
    unsigned char retA = (unsigned char)(floor(a * 255.0f + 0.5f));

    return Color(retR, retG, retB, retA);
  }

  return {};
}

int ColorMapper::getMinVal() const
{
  if (mappingType == MappingType::gradient || mappingType == MappingType::complex)
    return rangeMin;
  else if (mappingType == MappingType::map && !colorMap.empty())
    return colorMap.begin()->first;

  return 0;
}

int ColorMapper::getMaxVal() const
{
  if (mappingType == MappingType::gradient || mappingType == MappingType::complex)
    return rangeMax;
  else if (mappingType == MappingType::map && !colorMap.empty())
    return colorMap.rbegin()->first;

  return 0;
}

int ColorMapper::getID() const
{
  if (mappingType == MappingType::gradient)
    return 0;
  else if (mappingType == MappingType::map)
    return 1;
  else if (mappingType == MappingType::complex)
    return supportedComplexTypes.indexOf(complexType) + 2;

  // Invalid type
  return -1;
}

unsigned ColorMapper::mappingTypeToUInt(MappingType mappingType)
{
  auto m = std::map<MappingType, unsigned>({{MappingType::gradient, 0},
                                            {MappingType::map, 1},
                                            {MappingType::complex, 2},
                                            {MappingType::none, 3}});
  return m[mappingType];
}

bool ColorMapper::operator!=(const ColorMapper &other) const
{
  if (mappingType != other.mappingType)
    return true;
  if (mappingType == MappingType::gradient)
    return rangeMin != other.rangeMin || rangeMax != other.rangeMax || minColor != other.minColor ||
           maxColor != other.maxColor;
  if (mappingType == MappingType::map)
    return colorMap != other.colorMap;
  if (mappingType == MappingType::complex)
    return rangeMin != other.rangeMin || rangeMax != other.rangeMax ||
           complexType != other.complexType;
  return false;
}

} // namespace stats
