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

#ifndef PLAYLISTITEMSTATISTICSFILE_H
#define PLAYLISTITEMSTATISTICSFILE_H

#include <QBasicTimer>
#include <QFuture>
#include "fileSource.h"
#include "playlistItem.h"
#include "statisticHandler.h"

class playlistItemStatisticsFile : public playlistItem
{
  Q_OBJECT

public:

  /*
  */
  playlistItemStatisticsFile(const QString &itemNameOrFileName);
  virtual ~playlistItemStatisticsFile();

  virtual void savePlaylist(QDomElement &root, const QDir &playlistDir) const Q_DECL_OVERRIDE;

  virtual QSize getSize() const Q_DECL_OVERRIDE { return statSource.statFrameSize; }
  
  // Return the info title and info list to be shown in the fileInfo groupBox.
  virtual infoData getInfo() const Q_DECL_OVERRIDE;

  /* Get the title of the properties panel. The child class has to overload this.
   * This can be different depending on the type of playlistItem.
   * For example a playlistItemYUVFile will return "YUV File properties".
  */
  virtual QString getPropertiesTitle() const Q_DECL_OVERRIDE { return "Statistics File Properties"; }

  // ------ Statistics ----

  // Does the playlistItem provide statistics? If yes, the following functions can be
  // used to access it

  virtual void drawItem(QPainter *painter, int frameIdx, double zoomFactor, bool drawRawData) Q_DECL_OVERRIDE;

  // Do we need to load the statistics first?
  virtual itemLoadingState needsLoading(int frameIdx, bool loadRawdata) Q_DECL_OVERRIDE { Q_UNUSED(loadRawdata); return statSource.needsLoading(getFrameIdxInternal(frameIdx)); }
  // Load the statistics for the given frame
  virtual void loadFrame(int frameIdx, bool playback, bool loadRawdata, bool emitSignals=true) Q_DECL_OVERRIDE;
  // Are statistics currently being loaded?
  virtual bool isLoading() const Q_DECL_OVERRIDE { return isStatisticsLoading; }

  // Override from playlistItem. Return the statistics values under the given pixel position.
  virtual ValuePairListSets getPixelValues(const QPoint &pixelPos, int frameIdx) Q_DECL_OVERRIDE { Q_UNUSED(frameIdx); return ValuePairListSets("Stats",statSource.getValuesAt(pixelPos)); }

  // A statistics file source of course provides statistics
  virtual bool              providesStatistics() const Q_DECL_OVERRIDE { return true; }
  virtual statisticHandler *getStatisticsHandler() Q_DECL_OVERRIDE { return &statSource; }

  // ----- Detection of source/file change events -----
  virtual bool isSourceChanged()  Q_DECL_OVERRIDE { return file.isFileChanged(); }
  virtual void updateSettings()   Q_DECL_OVERRIDE { file.updateFileWatchSetting(); statSource.updateSettings(); }

protected:
  virtual indexRange getStartEndFrameLimits() const Q_DECL_OVERRIDE { return indexRange(0, maxPOC); }

  // Overload from playlistItem. Create a properties widget custom to the statistics item
  // and set propertiesWidget to point to it.
  virtual void createPropertiesWidget() Q_DECL_OVERRIDE;

  // The statistics source
  statisticHandler statSource;

  // Is the loadFrame function currently loading?
  bool isStatisticsLoading;

  QFuture<void> backgroundParserFuture;
  double backgroundParserProgress;
  bool cancelBackgroundParser;
  // A timer is used to frequently update the status of the background process (every second)
  QBasicTimer timer;
  virtual void timerEvent(QTimerEvent *event) Q_DECL_OVERRIDE; // Overloaded from QObject. Called when the timer fires.

  // Set if the file is sorted by POC and the types are 'random' within this POC (true)
  // or if the file is sorted by typeID and the POC is 'random'
  bool fileSortedByPOC;
  // If not -1, this gives the POC in which the parser noticed a block that was outside of the "frame"
  int  blockOutsideOfFrame_idx;
  // The maximum POC number in the file (as far as we know)
  int maxPOC;

  // If an error occurred while parsing, this error text will be set and can be shown
  QString parsingError;

  fileSource file;

  int currentDrawnFrameIdx;
};

#endif // PLAYLISTITEMSTATISTICSFILE_H
