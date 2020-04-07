/*  This file is part of YUView - The YUV player with advanced analytics toolset
*   <https://github.com/IENT/YUView>
*   Copyright (C) 2015  Institut f�r Nachrichtentechnik, RWTH Aachen University, GERMANY
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

#include "plotModel.h"

#include <QList>
#include <random>

class DummyPlotModel : public PlotModel
{
public:
  DummyPlotModel() : PlotModel()
  {
    std::random_device rd;
    std::mt19937 gen(rd());
    
    std::uniform_int_distribution<> dis500(0, 500);
    for (size_t i = 0; i < 100; i++)
      this->barData.append(int(dis500(gen)));
    std::uniform_int_distribution<> dis100(0, 100);
    std::uniform_int_distribution<> dis1000(0, 1000);
    for (size_t i = 0; i < 10; i++)
      this->graphData.append({double(dis100(gen)), double(dis1000(gen))});
  }

  unsigned int getNrPlots() const override
  { 
    return 3; 
  }

  PlotParameter getPlotParameter(unsigned plotIndex) const override
  {
    if (plotIndex == 0)
      return {PlotType::Bar, {0, 100}, {0, 500}};
    if (plotIndex == 1)
      return {PlotType::Line, {0, 100}, {0, 1000}};
    if (plotIndex == 2)
      return {PlotType::ConstValue, {-1, -1}, {300, 300}};
    return {};
  }

  unsigned int getNrPlotPoints(unsigned plotIndex) const override
  {
    if (plotIndex == 0)
      return barData.size();
    if (plotIndex == 1)
      return graphData.size();
    return 0;
  }

  Point getPlotPoint(unsigned plotIndex, unsigned pointIndex) const override
  {
    if (plotIndex == 0)
      return {double(pointIndex), double(barData[pointIndex])};
    if (plotIndex == 1)
      return graphData[pointIndex];
    return {};
  }

private:
  QList<int> barData;
  QList<Point> graphData;
};
