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

#include "plotModel.h"

PlotModel::PlotModel()
{
  this->connect(&this->eventSubsampler, &EventSubsampler::subsampledEvent, this, &PlotModel::dataChanged);
}

std::optional<unsigned> PlotModel::getPointIndex(unsigned streamIndex, unsigned plotIndex, QPointF point) const
{
  const auto streamParam = this->getStreamParameter(streamIndex);
  if (plotIndex >= unsigned(streamParam.plotParameters.size()))
    return {};
  const auto plotParam = this->getStreamParameter(streamIndex).plotParameters[plotIndex];
  if (plotParam.type == PlotModel::PlotType::Line)
  {
    // For lines we go segment by segmen (always considering two points).
    // In case of lines that go straight up/down, also consider 10% of the left and right segment to being this point.
    // For a line, we return the index of the end point of the segment.
    if (plotParam.nrpoints <= 1)
      return {};

    auto findLineSegmentAtPos = [&](double x) -> std::optional<unsigned>
    {
      auto prevPointX = this->getPlotPoint(streamIndex, plotIndex, 0).x;
      for (unsigned pointIndex = 1; pointIndex < plotParam.nrpoints; pointIndex++)
      {
        const auto pointX = this->getPlotPoint(streamIndex, plotIndex, pointIndex).x;
        if (x > prevPointX && x <= pointX)
          return pointIndex;
        prevPointX = pointX;
      }
      return {};
    };

    const auto lineAtPos = findLineSegmentAtPos(point.x());

    if (!lineAtPos)
    {
      // Special case: We did not find a segment at the position. Check if the position is
      // left of / right of all the data. If the first/last line segment is going straight down/up
      // return that as the selected one.
      const auto firstPointX = this->getPlotPoint(streamIndex, plotIndex, 0).x;
      if (point.x() < firstPointX && firstPointX == this->getPlotPoint(streamIndex, plotIndex, 1).x)
        return 0;
      const auto lastPointX = this->getPlotPoint(streamIndex, plotIndex, plotParam.nrpoints-1).x;
      if (point.x() > lastPointX && lastPointX == this->getPlotPoint(streamIndex, plotIndex, plotParam.nrpoints-2).x)
        return plotParam.nrpoints - 2;
      return {};
    }

    // We are on a "normal" segment. Check if the connecting segments left/right are going straight up/down.
    const auto startX = this->getPlotPoint(streamIndex, plotIndex, *lineAtPos - 1).x;
    const auto endX = this->getPlotPoint(streamIndex, plotIndex, *lineAtPos).x;
    const auto lengthLine = endX - startX;
    if (*lineAtPos >= 2)
    {
      // Check left
      const auto prevStartX = this->getPlotPoint(streamIndex, plotIndex, *lineAtPos - 2).x;
      if (startX == prevStartX && point.x() >= startX)
      {
        const auto relativeX = point.x() - startX;
        if (relativeX / lengthLine < 0.1)
          return *lineAtPos - 1;
      }
    }
    if (*lineAtPos < plotParam.nrpoints - 2)
    {
      // Check right
      const auto nextEndX = this->getPlotPoint(streamIndex, plotIndex, *lineAtPos + 1).x;
      if (endX == nextEndX && point.x() <= endX)
      {
        const auto relativeX = endX - point.x();
        if (relativeX / lengthLine < 0.1)
          return *lineAtPos + 1;
      }
    }
    return lineAtPos;
  }
  else
  {
    for (unsigned pointIndex = 0; pointIndex < plotParam.nrpoints; pointIndex++)
    {
      const auto barPoint = this->getPlotPoint(streamIndex, plotIndex, pointIndex);
      if (point.x() > barPoint.x - barPoint.width / 2 && point.x() <= barPoint.x + barPoint.width / 2)
        return pointIndex;
    }
  }
  return {};
}
