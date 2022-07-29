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

#include "DataPerType.h"
#include "StatisticsType.h"

#include <map>
#include <mutex>
#include <vector>

namespace stats
{

using StatisticsTypes = std::vector<StatisticsType>;
using TypeID          = int;
using DataPerTypeMap  = std::map<TypeID, DataPerType>;

struct FrameDataCache
{
  std::map<TypeID, DataPerType> data{};
  std::optional<int>            frameIndex;
};

class StatisticsData
{
public:
  ItemLoadingState       needsLoading(int frameIndex) const;
  std::vector<int>       getTypesThatNeedLoading(int frameIndex, BufferSelection buffer) const;
  QStringPairList        getValuesAt(const QPoint &pos) const;
  const StatisticsTypes &getStatisticsTypes() { return this->types; }
  bool                   hasDataForTypeID(int typeID) const;

  void add(BufferSelection buffer, TypeID typeID, BlockWithValue &&blockWithValue);
  void add(BufferSelection buffer, TypeID typeID, BlockWithVector &&BlockWithVector);

  void clear();
  void setFrameSize(Size size) { this->frameSize = size; }
  void setFrameIndex(int frameIndex, BufferSelection buffer);
  void setStatisticsTypes(const StatisticsTypes &&types);

  void savePlaylist(YUViewDomElement &root) const;
  void loadPlaylist(const YUViewDomElement &root);

private:
  FrameDataCache dataCacheMain{};
  FrameDataCache dataCacheDoubleBuffer{};

  mutable std::mutex accessMutex{};

  Size            frameSize{};
  StatisticsTypes types{};
};

} // namespace stats
