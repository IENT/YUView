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

#include "CustomColorMapStorage.h"

#include <common/Typedef.h>

#include <QSettings>

namespace stats::color
{

namespace
{

ColorMap parseColorMap(const QString &mapString)
{
  ColorMap colorMap;
  for (auto entry : mapString.split("|"))
  {
    auto indexAndColor = entry.split(",");
    if (indexAndColor.size() == 2)
    {
      auto index      = indexAndColor.at(0).toInt();
      auto color      = Color(indexAndColor.at(1).toStdString());
      colorMap[index] = color;
    }
  }
  return colorMap;
}

QString toString(const ColorMap &colorMap)
{
  QString str;
  for (const auto &color : colorMap)
    str += QString("%1,%2|").arg(color.first).arg(QString::fromStdString(color.second.toHex()));
  if (str.size() > 0 && str.endsWith("|"))
    str.chop(1);
  return str;
}

std::vector<CustomColorMap> loadFromSettings()
{
  QSettings                   settings;
  std::vector<CustomColorMap> colorMaps;

  auto size = settings.beginReadArray("CustomColorMaps");
  for (int i = 0; i < size; ++i)
  {
    settings.setArrayIndex(i);

    CustomColorMap customColorMap;
    customColorMap.name     = settings.value("name").toString();
    customColorMap.colorMap = parseColorMap(settings.value("colorMap").toString());
    customColorMap.other    = Color(settings.value("otherColor").toString().toStdString());
    colorMaps.push_back(customColorMap);
  }
  settings.endArray();
  return colorMaps;
}

void storeToSettings(const std::vector<CustomColorMap> &colorMaps)
{
  QSettings settings;
  settings.beginWriteArray("CustomColorMaps");
  for (size_t i = 0; i < colorMaps.size(); ++i)
  {
    settings.setArrayIndex(int(i));
    settings.setValue("name", colorMaps.at(i).name);
    settings.setValue("colorMap", toString(colorMaps.at(i).colorMap));
    settings.setValue("otherColor", QString::fromStdString(colorMaps.at(i).other.toHex()));
  }
  settings.endArray();
}

} // namespace

CustomColorMapStorage::CustomColorMapStorage()
{
  this->customColorMaps = loadFromSettings();
}

const std::vector<CustomColorMap> &CustomColorMapStorage::getCustomColorMaps() const
{
  return this->customColorMaps;
}

CustomColorMap CustomColorMapStorage::at(size_t index) const
{
  if (index >= this->customColorMaps.size())
    return {};
  return this->customColorMaps.at(index);
}

std::optional<size_t> CustomColorMapStorage::indexOfColorMap(const ColorMap &colorMap,
                                                             const Color &   other) const
{
  auto colorMapString = toString(colorMap);
  for (size_t i = 0; i < this->customColorMaps.size(); i++)
  {
    auto mapEntryAsString = toString(this->customColorMaps.at(i).colorMap);
    if (colorMapString == mapEntryAsString && this->customColorMaps.at(i).other == other)
      return i;
  }
  return {};
}

bool CustomColorMapStorage::contains(const QString &name) const
{
  auto it = std::find_if(this->customColorMaps.begin(),
                         this->customColorMaps.end(),
                         [&name](const CustomColorMap &map) { return map.name == name; });
  return it != this->customColorMaps.end();
}

size_t CustomColorMapStorage::saveAndGetIndex(const CustomColorMap &customColormap)
{
  for (size_t i = 0; i < this->customColorMaps.size(); ++i)
  {
    if (this->customColorMaps.at(i).name == customColormap.name)
    {
      this->customColorMaps[i] = customColormap;
      storeToSettings(this->customColorMaps);
      return i;
    }
  }

  this->customColorMaps.push_back(customColormap);
  storeToSettings(this->customColorMaps);
  return this->customColorMaps.size() - 1;
}

void CustomColorMapStorage::remove(const QString &name)
{
  auto it = std::find_if(this->customColorMaps.begin(),
                         this->customColorMaps.end(),
                         [&name](const CustomColorMap &map) { return map.name == name; });
  if (it == this->customColorMaps.end())
    return;

  this->customColorMaps.erase(it);
  storeToSettings(this->customColorMaps);
}

} // namespace stats::color
