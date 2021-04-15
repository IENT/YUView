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

#include "FrameTypeData.h"
#include "StatisticsType.h"

#include <QSize>
#include <QPolygon>
#include <map>
#include <mutex>
#include <vector>

namespace stats
{

using StatisticsTypesVec = std::vector<StatisticsType>;

QPolygon convertToQPolygon(const stats::Polygon &poly);

class StatisticsData
{
public:
  StatisticsData() = default;

  FrameTypeData       getFrameTypeData(int typeId);
  Size                getFrameSize() const { return this->frameSize; }
  int                 getFrameIndex() const { return this->frameIdx; }
  itemLoadingState    needsLoading(int frameIndex) const;
  std::vector<int>    getTypesThatNeedLoading(int frameIndex) const;
  QStringPairList     getValuesAt(const QPoint &pos) const;
  StatisticsTypesVec &getStatisticsTypes() { return this->statsTypes; }
  bool                hasDataForTypeID(int typeID) { return this->frameCache.count(typeID) > 0; }
  void                eraseDataForTypeID(int typeID) { this->frameCache.erase(typeID); }

  void clear();
  void setFrameSize(Size size) { this->frameSize = size; }
  void setFrameIndex(int frameIndex);
  void addStatType(const StatisticsType &type);

  void savePlaylist(YUViewDomElement &root) const;
  void loadPlaylist(const YUViewDomElement &root);

  FrameTypeData &operator[](int typeID) { return this->frameCache[typeID]; }
  FrameTypeData &at(int typeID) { return this->frameCache[typeID]; }

  mutable std::mutex accessMutex;

private:
  // cache of the statistics for the current POC [statsTypeID]
  std::map<int, FrameTypeData> frameCache;
  int                          frameIdx{-1};

  Size frameSize;

  StatisticsTypesVec statsTypes;
};

} // namespace stats
