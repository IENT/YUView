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

#include <QBasicTimer>
#include <QFuture>
#include <memory>

#include "playlistItem.h"
#include "statistics/StatisticsFileBase.h"

class playlistItemStatisticsFile : public playlistItem
{
  Q_OBJECT

public:
  enum class OpenMode
  {
    CSVFile,
    VTMBMSFile,
    Extension
  };

  playlistItemStatisticsFile(const QString &itemNameOrFileName,
                             OpenMode       openMode = OpenMode::Extension);
  virtual ~playlistItemStatisticsFile();

  virtual void savePlaylist(QDomElement &root, const QDir &playlistDir) const override;

  virtual QSize getSize() const override;

  // Return the info title and info list to be shown in the fileInfo groupBox.
  virtual infoData getInfo() const override;

  virtual void
  drawItem(QPainter *painter, int frameIdx, double zoomFactor, bool drawRawData) override;

  static playlistItemStatisticsFile *
  newplaylistItemStatisticsFile(const YUViewDomElement &root,
                                const QString &         playlistFilePath,
                                OpenMode                openMode = OpenMode::Extension);

  virtual void reloadItemSource() override;

  // ------ Statistics ----

  // Do we need to load the statistics first?
  virtual itemLoadingState needsLoading(int frameIdx, bool loadRawdata) override;

  // Load the statistics for the given frame. Emit signalItemChanged(true,false) when done. Always
  // called from a thread.
  virtual void
  loadFrame(int frameIdx, bool playback, bool loadRawdata, bool emitSignals = true) override;
  // Are statistics currently being loaded?
  virtual bool isLoading() const override { return isStatisticsLoading; }

  // Override from playlistItem. Return the statistics values under the given pixel position.
  virtual ValuePairListSets getPixelValues(const QPoint &pixelPos, int frameIdx) override;

  // A statistics file source of course provides statistics
  virtual stats::StatisticUIHandler *getStatisticsUIHandler() override
  {
    return &this->statisticsUIHandler;
  }

  // ----- Detection of source/file change events -----
  virtual bool isSourceChanged() override;
  virtual void updateSettings() override;

  static void getSupportedFileExtensions(QStringList &allExtensions, QStringList &filters);

protected slots:
  void onPOCTypeParsed(int poc, int typeID);
  void onPOCParsed(int poc);

protected:
  // Overload from playlistItem. Create a properties widget custom to the statistics item
  // and set propertiesWidget to point to it.
  virtual void createPropertiesWidget() override;

  void openStatisticsFile();

  stats::StatisticUIHandler statisticsUIHandler;
  stats::StatisticsData     statisticsData;

  std::unique_ptr<stats::StatisticsFileBase> file;
  OpenMode                                   openMode;

  // Is the loadFrame function currently loading?
  bool isStatisticsLoading;

  QFuture<void>    backgroundParserFuture;
  std::atomic_bool breakBackgroundAtomic;

  // A timer is used to frequently update the status of the background process (every second)
  QBasicTimer timer;
  virtual void
  timerEvent(QTimerEvent *event) override; // Overloaded from QObject. Called when the timer fires.

  int currentDrawnFrameIdx;
};
