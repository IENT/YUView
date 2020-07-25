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

  unsigned int getNrStreams() const override
  {
    return 3;
  }

  StreamParameter getStreamParameter(unsigned streamIndex) const override
  {
    StreamParameter streamParameter;
    if (streamIndex == 0)
    {
      streamParameter.xRange = {0, 99};
      streamParameter.yRange = {0, 500};
      streamParameter.plotParameters.append({PlotType::Bar, unsigned(barData.size())});
    }
    else if (streamIndex == 1)
    {
      streamParameter.xRange = {0, 99};
      streamParameter.yRange = {0, 1000};
      streamParameter.plotParameters.append({PlotType::Line, unsigned(graphData.size())});
    }
    else if (streamIndex == 2)
    {
      streamParameter.xRange = {-1, -1};
      streamParameter.yRange = {300, 99};
      streamParameter.plotParameters.append({PlotType::ConstValue, 1});
    }
    return streamParameter;
  }

  Point getPlotPoint(unsigned streamIndex, unsigned plotIndex, unsigned pointIndex) const override
  {
    if (streamIndex == 0 && plotIndex == 0)
      return {double(pointIndex), double(barData[pointIndex])};
    if (streamIndex == 1 && plotIndex == 0)
      return graphData[pointIndex];
    return {};
  }

  QString getPointInfo(unsigned streamIndex, unsigned plotIndex, unsigned pointIndex) const override
  {
    Q_UNUSED(plotIndex);
    auto point = this->getPlotPoint(streamIndex, 0, pointIndex);
    return QString("<h4>Stream %1</h4>"
                   "<table width=\"100%\">"
                   "<tr><td>PTS:</td><td align=\"right\">%2</td></tr>"
                   "<tr><td>DTS:</td><td align=\"right\">%2</td></tr>"
                   "<tr><td>Bitrate:</td><td align=\"right\">%3</td></tr>"
                   "<tr><td>Type:</td><td align=\"right\">%4</td></tr>"
                   "</table>"
                   ).arg(streamIndex).arg(point.x).arg(point.y).arg("Inter");
  }

private:
  QList<int> barData;
  QList<Point> graphData;
};
