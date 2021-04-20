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

#include "StatisticsData.h"

#include "common/functions.h"

// Activate this if you want to know when what is loaded.
#define STATISTICS_DEBUG_LOADING 0
#if STATISTICS_DEBUG_LOADING && !NDEBUG
#include <QDebug>
#define DEBUG_STATDATA(fmt) qDebug() << fmt
#else
#define DEBUG_STATDATA(fmt) ((void)0)
#endif

namespace stats
{

// code for checking if a point is inside a polygon. adapted from
// https://stackoverflow.com/questions/217578/how-can-i-determine-whether-a-2d-point-is-within-a-polygon
bool doesLineIntersectWithHorizontalLine(const Line side, const Point pt)
{
  bool isSideHorizontal = side.p1.y == side.p2.y;
  if(isSideHorizontal) return false; // collinear with line defined by pt, cannot intersect

  const float l1x1 = side.p1.x;
  const float l1y1 = side.p1.y;
  const float l1x2 = side.p2.x;
  const float l1y2 = side.p2.y;
  const float ptx = pt.x;
  const float pty = pt.y;

  // horizontal line from starting at pt.

  // checking for horizontal intersection, since simpler. do not need
  // to use linear eq system for it
  // y of horizontal line has to be between that of the lines pts
  // else can not intersect
  bool isIntersectionPossible = (l1y1 <= pty && l1y2 >= pty) || (l1y1 >= pty && l1y2 <= pty);
  if(!isIntersectionPossible) return false;

  float d1;
  float a1, b1, c1;

  // Convert vector 1 to a line (line 1) of infinite length.
  // We want the line in linear equation standard form: A*x + B*y + C = 0
  // See: http://en.wikipedia.org/wiki/Linear_equation
  a1 = l1y2 - l1y1;
  b1 = l1x1 - l1x2;
  c1 = (l1x2 * l1y1) - (l1x1 * l1y2);

  // Every point (x,y), that solves the equation above, is on the line,
  // every point that does not solve it, is not. The equation will have a
  // positive result if it is on one side of the line and a negative one
  // if is on the other side of it. We insert (x1,y1) and (x2,y2) of vector
  // 2 into the equation above.
  d1 = (a1 * ptx) + (b1 * pty) + c1;
  // we only actually need the signs of d1 and d2
  bool d1IsPositiv = d1 > 0;
  // since end-point (l2x2, l2y2) of the semi-infinte line is at infinity,
  // its sign is given by sign of a1
  bool d2IsPositiv = a1 > 0;

  bool doLinesIntersect = d1IsPositiv != d2IsPositiv;

  return doLinesIntersect;
}

bool polygonContainsPoint(const stats::Polygon &polygon, const Point &pt)
{
  if(polygon.empty()) return false;

  unsigned numPts = polygon.size();
  // Test the ray against all sides
  unsigned intersections = 0;
  for(unsigned i = 0; i < numPts - 1; i++)
  {
    Line side = Line(polygon.at(i), polygon.at(i+1));
    // Test if current side intersects with ray.
    if(doesLineIntersectWithHorizontalLine(side, pt))
    {
      intersections++;
    }
  }
  // close polygon
  if( polygon.front() != polygon.back())
  {
    Line side = Line(polygon.front(), polygon.back());
    // Test if current side intersects with ray.
    if(doesLineIntersectWithHorizontalLine(side, pt))
    {
      intersections++;
    }
  }

  if ((intersections & 1) == 1)
  {
    // Inside of polygon
    return true;
  }
  else
  {
    // Outside of polygon
    return false;
  }
}

FrameTypeData StatisticsData::getFrameTypeData(int typeID)
{
  if (this->frameCache.count(typeID) == 0)
    return {};

  return this->frameCache[typeID];
}

itemLoadingState StatisticsData::needsLoading(int frameIndex) const
{
  if (frameIndex != this->frameIdx)
  {
    // New frame, but do we even render any statistics?
    for (auto &t : this->statsTypes)
      if (t.render)
      {
        // At least one statistic type is drawn. We need to load it.
        DEBUG_STATDATA("StatisticsData::needsLoading new frameIndex " << frameIdx
                                                                      << " LoadingNeeded");
        return LoadingNeeded;
      }
  }

  std::unique_lock<std::mutex> lock(this->accessMutex);
  // Check all the statistics. Do some need loading?
  for (auto it = this->statsTypes.rbegin(); it != this->statsTypes.rend(); it++)
  {
    // If the statistics for this frame index were not loaded yet but will be rendered, load them
    // now.
    if (it->render && this->frameCache.count(it->typeID) == 0)
    {
      // Return that loading is needed before we can render the statitics.
      DEBUG_STATDATA("StatisticsData::needsLoading type " << it->typeID << " LoadingNeeded");
      return LoadingNeeded;
    }
  }

  // Everything needed for drawing is loaded
  DEBUG_STATDATA("StatisticsData::needsLoading " << frameIdx << " LoadingNotNeeded");
  return LoadingNotNeeded;
}

std::vector<int> StatisticsData::getTypesThatNeedLoading(int frameIndex) const
{
  std::vector<int> typesToLoad;
  auto             loadAll = this->frameIdx != frameIndex;
  for (const auto &statsType : this->statsTypes)
  {
    if (statsType.render && (loadAll || this->frameCache.count(statsType.typeID) == 0))
      typesToLoad.push_back(statsType.typeID);
  }

  DEBUG_STATDATA("StatisticsData::getTypesThatNeedLoading "
                 << QString::fromStdString(to_string(typesToLoad)));
  return typesToLoad;
}

// return raw(!) value of front-most, active statistic item at given position
// Info is always read from the current buffer. So these values are only valid if a draw event
// occurred first.
QStringPairList StatisticsData::getValuesAt(const QPoint &pos) const
{
  QStringPairList valueList;

  for (auto it = this->statsTypes.rbegin(); it != this->statsTypes.rend(); it++)
  {
    if (!it->renderGrid)
      continue;

    if (it->typeID == INT_INVALID || this->frameCache.count(it->typeID) == 0)
      // no active statistics data
      continue;

    // Get all value data entries
    bool foundStats = false;
    for (const auto &valueItem : this->frameCache.at(it->typeID).valueData)
    {
      auto rect = QRect(valueItem.pos[0], valueItem.pos[1], valueItem.size[0], valueItem.size[1]);
      if (rect.contains(pos))
      {
        int  value  = valueItem.value;
        auto valTxt = it->getValueTxt(value);
        if (valTxt.isEmpty() && it->scaleValueToBlockSize)
          valTxt = QString("%1").arg(float(value) / (valueItem.size[0] * valueItem.size[1]));

        valueList.append(QStringPair(it->typeName, valTxt));
        foundStats = true;
      }
    }

    for (const auto &vectorItem : this->frameCache.at(it->typeID).vectorData)
    {
      auto rect =
          QRect(vectorItem.pos[0], vectorItem.pos[1], vectorItem.size[0], vectorItem.size[1]);
      if (rect.contains(pos))
      {
        float vectorValue1, vectorValue2;
        if (vectorItem.isLine)
        {
          vectorValue1 =
              float(vectorItem.point[1].first - vectorItem.point[0].first) / it->vectorScale;
          vectorValue2 =
              float(vectorItem.point[1].second - vectorItem.point[0].second) / it->vectorScale;
        }
        else
        {
          vectorValue1 = float(vectorItem.point[0].first / it->vectorScale);
          vectorValue2 = float(vectorItem.point[0].second / it->vectorScale);
        }
        valueList.append(
            QStringPair(QString("%1[x]").arg(it->typeName), QString::number(vectorValue1)));
        valueList.append(
            QStringPair(QString("%1[y]").arg(it->typeName), QString::number(vectorValue2)));
        foundStats = true;
      }
    }

    for (const auto &affineTFItem : this->frameCache.at(it->typeID).affineTFData)
    {
      const auto rect = QRect(
          affineTFItem.pos[0], affineTFItem.pos[1], affineTFItem.size[0], affineTFItem.size[1]);
      if (rect.contains(pos))
      {
        for( unsigned i=0; i<3; i++)
        {
          float vectorValue1, vectorValue2;
          vectorValue1 = float(affineTFItem.point[i].first / it->vectorScale);
          vectorValue2 = float(affineTFItem.point[i].second / it->vectorScale);
          valueList.append(
              QStringPair(QString("%1_%2[x]").arg(it->typeName).arg(i), QString::number(vectorValue1)));
          valueList.append(
              QStringPair(QString("%1_%2[y]").arg(it->typeName).arg(i), QString::number(vectorValue2)));
        }
        foundStats = true;
      }
    }

    for (const auto &valueItem : this->frameCache.at(it->typeID).polygonValueData)
    {
      if (!valueItem.corners.size()) continue;      
      if (stats::polygonContainsPoint(valueItem.corners, Point(pos.x(), pos.y())))
      {
        int  value  = valueItem.value;
        auto valTxt = it->getValueTxt(value);
        valueList.append(QStringPair(it->typeName, valTxt));
        foundStats = true;
      }
    }

    for (const auto &polygonVectorItem : this->frameCache.at(it->typeID).polygonVectorData)
    {
      if (!polygonVectorItem.corners.size()) continue;
      if (stats::polygonContainsPoint(polygonVectorItem.corners, Point(pos.x(), pos.y())))
      {
        if (it->renderVectorData)
        {
          float vx, vy;
          // The length of the vector
          vx = (float)polygonVectorItem.point.first / it->vectorScale;
          vy = (float)polygonVectorItem.point.second / it->vectorScale;
          valueList.append(
              QStringPair(QString("%1[x]").arg(it->typeName), QString::number(vx)));
          valueList.append(
              QStringPair(QString("%1[y]").arg(it->typeName), QString::number(vy)));
          foundStats = true;
        }
      }
    }

    if (!foundStats)
      valueList.append(QStringPair(it->typeName, "-"));
  }

  return valueList;
}

void StatisticsData::clear()
{
  this->frameCache.clear();
  this->frameIdx  = -1;
  this->frameSize = {};
  this->statsTypes.clear();
}

void StatisticsData::setFrameIndex(int frameIndex)
{
  std::unique_lock<std::mutex> lock(this->accessMutex);
  if (this->frameIdx != frameIndex)
  {
    DEBUG_STATDATA("StatisticsData::getTypesThatNeedLoading New frame index set "
                   << this->frameIdx << "->" << frameIndex);
    this->frameCache.clear();
    this->frameIdx = frameIndex;
  }
}

void StatisticsData::addStatType(const StatisticsType &type)
{
  if (type.typeID == -1)
  {
    // stat source does not have type ids. need to auto assign an id for this type
    // check if type not already in list
    int maxTypeID = 0;
    for (auto it = this->statsTypes.begin(); it != this->statsTypes.end(); it++)
    {
      if (it->typeName == type.typeName)
        return;
      if (it->typeID > maxTypeID)
        maxTypeID = it->typeID;
    }

    auto newType   = type;
    newType.typeID = maxTypeID + 1;
    this->statsTypes.push_back(newType);
  }
  else
    this->statsTypes.push_back(type);
}

void StatisticsData::savePlaylist(YUViewDomElement &root) const
{
  for (const auto &type : this->statsTypes)
    type.savePlaylist(root);
}

void StatisticsData::loadPlaylist(const YUViewDomElement &root)
{
  for (auto &type : this->statsTypes)
    type.loadPlaylist(root);
}

} // namespace stats
