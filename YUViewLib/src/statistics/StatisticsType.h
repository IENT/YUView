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

#include <common/Typedef.h>
#include <common/YUViewDomElement.h>

#include "ColorMapper.h"

namespace stats
{

enum class Pattern
{
  Solid,
  Dash,
  Dot,
  DashDot,
  DashDotDot
};

const std::vector<Pattern> AllPatterns = {
    Pattern::Solid, Pattern::Dash, Pattern::Dot, Pattern::DashDot, Pattern::DashDotDot};

struct LineDrawStyle
{
  Color   color{};
  double  width{0.25};
  Pattern pattern{Pattern::Solid};

  bool operator!=(const LineDrawStyle &other) const
  {
    return color != other.color || width != other.width || pattern != other.pattern;
  }
};

/* This class defines a type of statistic to render. Each statistics type entry defines the name and
 * and ID of a statistic. It also defines what type of data should be drawn for this type and how it
 * should be drawn.
 */
class StatisticsType
{
public:
  StatisticsType() = default;

  // Save all the values that the user could change. When saving to playlist we can save only the
  // changed values to playlist.
  void setInitialState();

  // Load/Save status of statistics from playlist file
  void savePlaylist(YUViewDomElement &root) const;
  void loadPlaylist(const YUViewDomElement &root);

  // Every statistics type has an ID, a name and possibly a description
  int         typeID{};
  std::string typeName{};
  std::string description{};

  // Get the value text (from the value map (if there is an entry))
  std::string getValueText(const int val) const;

  void        setMappingValues(std::vector<std::string> values);
  std::string getMappedValue(const int typeID) const;

  // Is this statistics type rendered and what is the alpha value?
  // These are corresponding to the controls in the properties panel
  bool render{};
  int  alphaFactor{50};

  struct ValueDataOptions
  {
    bool               render{true};
    bool               scaleToBlockSize{};
    color::ColorMapper colorMapper{};
  };

  std::optional<ValueDataOptions> valueDataOptions;

  enum class ArrowHead
  {
    arrow,
    circle,
    none
  };

  struct VectorDataOptions
  {
    bool          render{true};
    bool          renderDataValues{true};
    bool          scaleToZoom{};
    LineDrawStyle style{};
    int           scale{};
    bool          mapToColor{};
    ArrowHead     arrowHead{};
  };

  std::optional<VectorDataOptions> vectorDataOptions;

  struct GridOptions
  {
    bool          render{};
    LineDrawStyle style{};
    bool          scaleToZoom{};
  };

  GridOptions gridOptions;

private:
  std::map<int, std::string> valuesToText;

  // Backup values for setDefaultState()
  struct initialState
  {
    bool render;
    int  alphaFactor;

    std::optional<ValueDataOptions>  valueDataOptions;
    std::optional<VectorDataOptions> vectorDataOptions;
    GridOptions                      gridOptions;
  };
  initialState init;
};

} // namespace stats
