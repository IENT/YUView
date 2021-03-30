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

#include <QPainterPath>

namespace stats
{

// Activate this if you want to know when what is loaded.
#define STATISTICS_DEBUG_LOADING 0
#if STATISTICS_DEBUG_LOADING && !NDEBUG
#define DEBUG_STAT qDebug
#else
#define DEBUG_STAT(fmt, ...) ((void)0)
#endif

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
        DEBUG_STAT("StatisticsData::needsLoading %d LoadingNeeded", frameIdx);
        return LoadingNeeded;
      }
  }

  std::unique_lock<std::mutex> lock(this->accessMutex);
  // Check all the statistics. Do some need loading?
  for (auto it = this->statsTypes.rbegin(); it != this->statsTypes.rend(); it++)
  {
    // If the statistics for this frame index were not loaded yet but will be rendered, load them
    // now.
    auto typeIdx = it->typeID;
    if (it->render)
    {
      if (this->frameCache.count(typeIdx) > 0)
      {
        // Return that loading is needed before we can render the statitics.
        DEBUG_STAT("StatisticsData::needsLoading %d LoadingNeeded", frameIdx);
        return LoadingNeeded;
      }
    }
  }

  // Everything needed for drawing is loaded
  DEBUG_STAT("StatisticsData::needsLoading %d LoadingNotNeeded", frameIdx);
  return LoadingNotNeeded;
}

std::vector<int> StatisticsData::getTypesThatNeedLoading() const
{
  std::vector<int> typesToLoad;
  for (const auto &statsType : this->statsTypes)
  {
    if (statsType.render && this->frameCache.count(statsType.typeID) == 0)
      typesToLoad.push_back(statsType.typeID);
  }
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

    if (it->typeID == INT_INVALID) // no active statistics
      continue;

    const auto typeName = this->statsTypes.at(it->typeID).typeName;

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

        valueList.append(QStringPair(typeName, valTxt));
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
              (float)(vectorItem.point[1].x() - vectorItem.point[0].x()) / it->vectorScale;
          vectorValue2 =
              (float)(vectorItem.point[1].y() - vectorItem.point[0].y()) / it->vectorScale;
        }
        else
        {
          vectorValue1 = (float)vectorItem.point[0].x() / it->vectorScale;
          vectorValue2 = (float)vectorItem.point[0].y() / it->vectorScale;
        }
        valueList.append(
            QStringPair(QString("%1[x]").arg(typeName), QString::number(vectorValue1)));
        valueList.append(
            QStringPair(QString("%1[y]").arg(typeName), QString::number(vectorValue2)));
        foundStats = true;
      }
    }

    if (!foundStats)
      valueList.append(QStringPair(typeName, "-"));
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
    DEBUG_STAT("StatisticsData::getTypesThatNeedLoading New frame index set %d -> %d",
               this->frameIdx,
               frameIndex);
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

    auto newType = type;
    newType.typeID         = maxTypeID + 1;
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