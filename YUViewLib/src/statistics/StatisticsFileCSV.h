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

#include "StatisticsFileBase.h"

namespace stats
{

/* Abstract base class that prvides features which are common to all parsers
 */
class StatisticsFileCSV : public StatisticsFileBase
{
public:
  StatisticsFileCSV(const QString &filename, StatisticsData &statisticsData);
  virtual ~StatisticsFileCSV() = default;

  // -1 if it could not be parser from the file
  double getFramerate() const override { return this->framerate; }

  // Parse the whole file and get the positions where a new POC/type starts and save them. Later we
  // can then seek to these positions to load data. Usually this is called in a seperate thread.
  void readFrameAndTypePositionsFromFile(std::atomic_bool &breakFunction) override;

  // Load the statistics for "poc/type" from file and return the data.
  stats::DataPerType loadFrameStatisticsData(int poc, int typeID) override;

protected:
  //! Scan the header: What types are saved in this file?
  void readHeaderFromFile(StatisticsData &statisticsData);

  double framerate{-1};

  // File positions pocTypeFileposMap[poc][typeID]
  using TypeFileposMap = std::map<int, uint64_t>;
  std::map<int, TypeFileposMap> pocTypeFileposMap;

  struct OtherTypeCache
  {
    int                   poc{-1};
    stats::DataPerTypeMap data;
  };
  OtherTypeCache otherTypeCache;
};

} // namespace stats