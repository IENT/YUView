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

#include <common/Functions.h>

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

namespace
{

// code for checking if a point is inside a polygon. adapted from
// https://stackoverflow.com/questions/217578/how-can-i-determine-whether-a-2d-point-is-within-a-polygon
bool doesLineIntersectWithHorizontalLine(const Line side, const Point pt)
{
  bool isSideHorizontal = side.p1.y == side.p2.y;
  if (isSideHorizontal)
    return false; // collinear with line defined by pt, cannot intersect

  const float l1x1 = side.p1.x;
  const float l1y1 = side.p1.y;
  const float l1x2 = side.p2.x;
  const float l1y2 = side.p2.y;
  const float ptx  = pt.x;
  const float pty  = pt.y;

  // horizontal line from starting at pt.

  // checking for horizontal intersection, since simpler. do not need
  // to use linear eq system for it
  // y of horizontal line has to be between that of the lines pts
  // else can not intersect
  bool isIntersectionPossible = (l1y1 < pty && l1y2 >= pty) || (l1y1 >= pty && l1y2 < pty);
  if (!isIntersectionPossible)
    return false;

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
  if (polygon.empty())
    return false;

  auto numPts = polygon.size();
  // Test the ray against all sides
  unsigned intersections = 0;
  for (size_t i = 0; i < numPts - 1; i++)
  {
    Line side = Line(polygon.at(i), polygon.at(i + 1));
    // Test if current side intersects with ray.
    if (doesLineIntersectWithHorizontalLine(side, pt))
      intersections++;
  }
  // close polygon
  if (polygon.front() != polygon.back())
  {
    auto side = Line(polygon.front(), polygon.back());
    // Test if current side intersects with ray.
    if (doesLineIntersectWithHorizontalLine(side, pt))
      intersections++;
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

bool anyStatsNeedLoading(const StatisticsTypes &types, const DataPerTypeMap &dataMap)
{
  for (auto &type : types)
  {
    if (type.render && dataMap.count(type.typeID) == 0)
    {
      DEBUG_STATDATA("StatisticsData::checkIfAnyStatsNeedLoading type " << type.typeID
                                                                        << " needs loading");
      return true;
    }
  }
  return false;
}

bool anyStatsAreRendered(const StatisticsTypes &types)
{
  return std::any_of<StatisticsType>(
      types.begin(), types.end(), [](const StatisticsType &type) { return type.render; });
}

} // namespace

ItemLoadingState StatisticsData::needsLoading(int frameIndex) const
{
  std::unique_lock<std::mutex> lock(this->accessMutex);

  if (!anyStatsAreRendered(this->types))
  {
    DEBUG_STATDATA("StatisticsData::needsLoading " << frameIndex
                                                   << " nothing rendered - LoadingNotNeeded");
    return ItemLoadingState::LoadingNotNeeded;
  }

  if (frameIndex != this->dataCacheMain.frameIndex)
  {
    DEBUG_STATDATA("StatisticsData::needsLoading " << frameIndex << " new frame - LoadingNeeded");
    return ItemLoadingState::LoadingNeeded;
  }

  if (anyStatsNeedLoading(this->types, this->dataCacheMain.data))
  {
    DEBUG_STATDATA("StatisticsData::needsLoading " << frameIndex << " LoadingNeeded");
    return ItemLoadingState::LoadingNeeded;
  }

  if (frameIndex + 1 != this->dataCacheDoubleBuffer.frameIndex)
  {
    DEBUG_STATDATA("StatisticsData::needsLoading "
                   << frameIndex << " new frame for double buffer - LoadingNeededDoubleBuffer");
    return ItemLoadingState::LoadingNeededDoubleBuffer;
  }

  if (anyStatsNeedLoading(this->types, this->dataCacheDoubleBuffer.data))
  {
    DEBUG_STATDATA("StatisticsData::needsLoading " << frameIndex << " LoadingNeededDoubleBuffer");
    return ItemLoadingState::LoadingNeededDoubleBuffer;
  }

  DEBUG_STATDATA("StatisticsData::needsLoading " << frameIndex << " LoadingNotNeeded");
  return ItemLoadingState::LoadingNotNeeded;
}

std::vector<int> StatisticsData::getTypesThatNeedLoading(int             frameIndex,
                                                         BufferSelection buffer) const
{
  std::vector<int> typesToLoad;

  auto  loadedFrameIndex = (buffer == BufferSelection::DoubleBuffer)
                               ? this->dataCacheDoubleBuffer.frameIndex
                               : this->dataCacheMain.frameIndex;
  auto  loadAll          = loadedFrameIndex != frameIndex;
  auto &dataCache =
      (buffer == BufferSelection::DoubleBuffer) ? this->dataCacheDoubleBuffer : this->dataCacheMain;

  for (const auto &type : this->types)
  {
    if (type.render && (loadAll || dataCache.data.count(type.typeID) == 0))
      typesToLoad.push_back(type.typeID);
  }

  DEBUG_STATDATA("StatisticsData::getTypesThatNeedLoading "
                 << ((buffer == BufferSelection::DoubleBuffer) ? "doubleBuffer" : "primaryBuffer")
                 << QString::fromStdString(to_string(typesToLoad)));
  return typesToLoad;
}

// return raw(!) value of front-most, active statistic item at given position
// Info is always read from the current buffer. So these values are only valid if a draw event
// occurred first.
QStringPairList StatisticsData::getValuesAt(const QPoint &qPoint) const
{
  QStringPairList valueList;
  const Point     point(qPoint.x(), qPoint.y());

  std::unique_lock<std::mutex> lock(this->accessMutex);

  for (auto it = this->types.rbegin(); it != this->types.rend(); it++)
  {
    if (!it->renderGrid)
      continue;

    if (it->typeID == INT_INVALID || this->dataCacheMain.data.count(it->typeID) == 0)
      // no active statistics data
      continue;

    const auto &typeData = this->dataCacheMain.data.at(it->typeID);

    bool foundStats = false;
    for (const auto &blockWithValue : typeData.valueData)
    {
      if (blockWithValue.contains(point))
      {
        int  value  = blockWithValue.value;
        auto valTxt = it->getValueTxt(value);
        if (valTxt.isEmpty() && it->scaleValueToBlockSize)
          valTxt = QString("%1").arg(float(value) / blockWithValue.area());

        valueList.append(QStringPair(it->typeName, valTxt));
        foundStats = true;
      }
    }

    for (const auto &blockWithVector : typeData.vectorData)
    {
      if (blockWithVector.contains(point))
      {
        auto vectorValue1 = float(blockWithVector.vector.x / it->vectorScale);
        auto vectorValue2 = float(blockWithVector.vector.y / it->vectorScale);

        valueList.append(
            QStringPair(QString("%1[x]").arg(it->typeName), QString::number(vectorValue1)));
        valueList.append(
            QStringPair(QString("%1[y]").arg(it->typeName), QString::number(vectorValue2)));
        foundStats = true;
      }
    }

    for (const auto &blockWithLine : typeData.lineData)
    {
      if (blockWithLine.contains(point))
      {
        auto lineValue1 =
            float(blockWithLine.line.p2.x - blockWithLine.line.p1.x) / it->vectorScale;
        auto lineValue2 =
            float(blockWithLine.line.p2.y - blockWithLine.line.p1.y) / it->vectorScale;

        valueList.append(
            QStringPair(QString("%1[x]").arg(it->typeName), QString::number(lineValue1)));
        valueList.append(
            QStringPair(QString("%1[y]").arg(it->typeName), QString::number(lineValue2)));
        foundStats = true;
      }
    }

    for (const auto &blockWithAffineTF : typeData.affineTFData)
    {
      if (blockWithAffineTF.contains(point))
      {
        for (int i = 0; i < 3; i++)
        {
          auto xScaled = float(blockWithAffineTF.point[i].x / it->vectorScale);
          auto yScaled = float(blockWithAffineTF.point[i].y / it->vectorScale);
          valueList.append(
              QStringPair(QString("%1_%2[x]").arg(it->typeName).arg(i), QString::number(xScaled)));
          valueList.append(
              QStringPair(QString("%1_%2[y]").arg(it->typeName).arg(i), QString::number(yScaled)));
        }
        foundStats = true;
      }
    }

    for (const auto &polygonWithValue : typeData.polygonValueData)
    {
      if (polygonWithValue.size() < 3)
        continue;
      if (polygonContainsPoint(polygonWithValue, point))
      {
        auto valTxt = it->getValueTxt(polygonWithValue.value);
        valueList.append(QStringPair(it->typeName, valTxt));
        foundStats = true;
      }
    }

    for (const auto &polygonWithVector : typeData.polygonVectorData)
    {
      if (polygonWithVector.size() < 3)
        continue;
      if (polygonContainsPoint(polygonWithVector, point))
      {
        if (it->renderVectorData)
        {
          // The length of the vector
          auto xScaled = (float)polygonWithVector.vector.x / it->vectorScale;
          auto yScaled = (float)polygonWithVector.vector.y / it->vectorScale;
          valueList.append(
              QStringPair(QString("%1[x]").arg(it->typeName), QString::number(xScaled)));
          valueList.append(
              QStringPair(QString("%1[y]").arg(it->typeName), QString::number(yScaled)));
          foundStats = true;
        }
      }
    }

    if (!foundStats)
      valueList.append(QStringPair(it->typeName, "-"));
  }

  return valueList;
}

bool StatisticsData::hasDataForTypeID(int typeID) const
{
  std::unique_lock<std::mutex> lock(this->accessMutex);
  return this->dataCacheMain.data.count(typeID) > 0;
}

void StatisticsData::add(BufferSelection buffer, TypeID typeID, BlockWithValue &&blockWithValue)
{
  if (buffer == BufferSelection::Primary)
    this->dataCacheMain.data[typeID].valueData.push_back(std::move(blockWithValue));
  if (buffer == BufferSelection::DoubleBuffer)
    this->dataCacheDoubleBuffer.data[typeID].valueData.push_back(std::move(blockWithValue));
}

void StatisticsData::add(BufferSelection buffer, TypeID typeID, BlockWithVector &&blockWithVector)
{
  if (buffer == BufferSelection::Primary)
    this->dataCacheMain.data[typeID].vectorData.push_back(std::move(blockWithVector));
  if (buffer == BufferSelection::DoubleBuffer)
    this->dataCacheDoubleBuffer.data[typeID].vectorData.push_back(std::move(blockWithVector));
}

void StatisticsData::clear()
{
  this->dataCacheMain.data.clear();
  this->dataCacheDoubleBuffer.data.clear();
  this->dataCacheMain.frameIndex         = {};
  this->dataCacheDoubleBuffer.frameIndex = {};
  this->frameSize                        = {};
  this->types.clear();
}

void StatisticsData::setFrameIndex(int frameIndex, BufferSelection buffer)
{
  std::unique_lock<std::mutex> lock(this->accessMutex);

  if (buffer == BufferSelection::DoubleBuffer &&
      this->dataCacheDoubleBuffer.frameIndex != frameIndex)
  {
    DEBUG_STATDATA("StatisticsData::setFrameIndex New frame index set double buffer "
                   << this->dataCacheDoubleBuffer.frameIndex.value_or(-1) << "->" << frameIndex);
    this->dataCacheDoubleBuffer.data.clear();
    this->dataCacheDoubleBuffer.frameIndex = frameIndex;
  }
  if (buffer == BufferSelection::Primary && this->dataCacheMain.frameIndex != frameIndex)
  {
    DEBUG_STATDATA("StatisticsData::setFrameIndex New frame index set "
                   << this->dataCacheMain.frameIndex.value_or(-1) << "->" << frameIndex);
    this->dataCacheMain.data.clear();
    this->dataCacheMain.frameIndex = frameIndex;
  }
}

void StatisticsData::addStatType(const StatisticsType &type)
{
  std::unique_lock<std::mutex> lock(this->accessMutex);

  if (type.typeID == -1)
  {
    // stat source does not have type ids. need to auto assign an id for this type
    // check if type not already in list
    int maxTypeID = 0;
    for (auto it = this->types.begin(); it != this->types.end(); it++)
    {
      if (it->typeName == type.typeName)
        return;
      if (it->typeID > maxTypeID)
        maxTypeID = it->typeID;
    }

    auto newType   = type;
    newType.typeID = maxTypeID + 1;
    this->types.push_back(newType);
  }
  else
    this->types.push_back(type);
}

void StatisticsData::savePlaylist(YUViewDomElement &root) const
{
  for (const auto &type : this->types)
    type.savePlaylist(root);
}

void StatisticsData::loadPlaylist(const YUViewDomElement &root)
{
  for (auto &type : this->types)
    type.loadPlaylist(root);
}

} // namespace stats
