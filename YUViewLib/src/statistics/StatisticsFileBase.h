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

#include "filesource/FileSource.h"
#include "statistics/StatisticsData.h"

#include <QObject>

namespace stats
{

/* Abstract base class that prvides features which are common to all parsers
 */
class StatisticsFileBase : public QObject
{
  Q_OBJECT

public:
  StatisticsFileBase(const QString &filename);
  virtual ~StatisticsFileBase();

  // Parse the whole file and get the positions where a new POC/type starts and save them. Later we
  // can then seek to these positions to load data. Usually this is called in a seperate thread.
  virtual void readFrameAndTypePositionsFromFile(std::atomic_bool &breakFunction) = 0;

  // Load the statistics for "poc/type" from file and put it into the handlers cache.
  virtual void loadStatisticData(StatisticsData &statisticsData, int poc, int typeID) = 0;

  operator bool() const { return !this->error; };

  // -1 if it could not be parser from the file
  virtual double getFramerate() const { return -1; }

  int getMaxPoc() const { return this->maxPOC; }

  bool isFileChanged() { return this->file.isFileChanged(); }
  void updateSettings() { this->file.updateFileWatchSetting(); }

  infoData getInfo() const;

signals:
  // When readFrameAndTypePositionsFromFile is running it will emit whenever new data for this POC
  // is available. If this POC is currently drawn we can then update the view and show the
  // statistics.
  void readPOCType(int newPoc, int typeID);
  void readPOC(int newPoc);

protected:
  FileSource file;

  // Set if the file is sorted by POC and the types are 'random' within this POC (true)
  // or if the file is sorted by typeID and the POC is 'random'
  bool fileSortedByPOC{};
  // The maximum POC number in the file (as far as we know)
  int maxPOC{};
  // The POC in which the parser noticed a block that was outside of the "frame" or -1 if none was
  // found.
  int blockOutsideOfFramePOC{-1};

  bool    error{false};
  QString errorMessage{};

  double parsingProgress{};
  bool   abortParsingDestroy{};
};

} // namespace stats