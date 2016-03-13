/*  YUView - YUV player with advanced analytics toolset
*   Copyright (C) 2015  Institut für Nachrichtentechnik
*                       RWTH Aachen University, GERMANY
*
*   YUView is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   YUView is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with YUView.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef PLAYLISTITEMSTATISTICSFILE_H
#define PLAYLISTITEMSTATISTICSFILE_H

#include <QTreeWidgetItem>
#include <QDomElement>
#include <QDir>
#include "typedef.h"
#include <assert.h>
#include <QFuture>

#include "playlistItem.h"
#include "fileSource.h"
#include "statisticSource.h"

class playlistItemStatisticsFile :
  public playlistItem
{
  Q_OBJECT

public:

  /*
  */
  playlistItemStatisticsFile(QString itemNameOrFileName);
  virtual ~playlistItemStatisticsFile();

  virtual void savePlaylist(QDomElement &root, QDir playlistDir) Q_DECL_OVERRIDE { Q_UNUSED(root); Q_UNUSED(playlistDir); };

  // A statistics file has a fixed number of frames
  virtual bool isIndexedByFrame() Q_DECL_OVERRIDE { return true; }
  virtual indexRange getFrameIndexRange() Q_DECL_OVERRIDE { return statSource.startEndFrame; }

  // Return the info title and info list to be shown in the fileInfo groupBox.
  virtual QString getInfoTitel() Q_DECL_OVERRIDE { return "Statistics File info"; }
  virtual QList<infoItem> getInfoList() Q_DECL_OVERRIDE;

  /* Get the title of the properties panel. The child class has to overload this.
   * This can be different depending on the type of playlistItem.
   * For example a playlistItemYUVFile will return "YUV File properties".
  */
  virtual QString getPropertiesTitle() Q_DECL_OVERRIDE { return "Statistics File Properties"; }

  // ------ Statistics ----

  // Does the playlistItem provide statistics? If yes, the following functions can be
  // used to access it
  virtual bool isCaching() Q_DECL_OVERRIDE {return false;}

  virtual void drawItem(QPainter *painter, int frameIdx, double zoomFactor) Q_DECL_OVERRIDE;

protected:
  // Overload from playlistItem. Create a properties widget custom to the statistics item
  // and set propertiesWidget to point to it.
  virtual void createPropertiesWidget() Q_DECL_OVERRIDE;

private:

  // The statistics source
  statisticSource statSource;

  //! Scan the header: What types are saved in this file?
  void readHeaderFromFile();
  //! Load the statistics with frameIdx/type from file and put it into the cache.
  //! If the statistics file is in an interleaved format (types are mixed within one POC) this function also parses
  //! types which were not requested by the given 'type'.
  void loadStatisticToCache(int frameIdx, int type);

  QStringList parseCSVLine(QString line, char delimiter);

  // A list of file positions where each POC/type starts
  QMap<int, QMap<int, qint64> > pocTypeStartList;

  qint64 nrFrames;

  // --------------- background parsing ---------------

  //! Parser the whole file and get the positions where a new POC/type starts. Save this position in p_pocTypeStartList.
  //! This is performed in the background using the QFuture.
  void readFrameAndTypePositionsFromFile();
  QFuture<void> backgroundParserFuture;
  double backgroundParserProgress;
  bool cancelBackgroundParser;
  // A timer is used to frequently update the status of the background process (every second)
  int timerId; // If we call QObject::startTimer(...) we have to remember the ID so we can kill it later.
  virtual void timerEvent(QTimerEvent * event) Q_DECL_OVERRIDE; // Overloaded from QObject. Called when the timer fires.

  // Set if the file is sorted by POC and the types are 'random' within this POC (true)
  // or if the file is sorted by typeID and the POC is 'random'
  bool fileSortedByPOC;
  // If not -1, this gives the POC in which the parser noticed a block that was outside of the "frame"
  int  blockOutsideOfFrame_idx;

  // If an error occured while parsing, this error text will be set and can be shown
  QString parsingError;

  fileSource file;

private slots:
  void updateStatSource(bool bRedraw) { emit signalItemChanged(bRedraw); }

};

#endif // PLAYLISTITEMSTATISTICSFILE_H
