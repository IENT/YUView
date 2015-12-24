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

#ifndef PLAYLISTITEM_H
#define PLAYLISTITEM_H

#include <QTreeWidgetItem>
#include "typedef.h"

class playlistItem : public QTreeWidgetItem
{
public:
  /*playlistItem(QString itemNameOrFileName, QTreeWidget* parent = 0);
  playlistItem(QString itemNameOrFileName, QTreeWidgetItem* parent = 0);*/
  playlistItem(QString itemNameOrFileName);
  ~playlistItem();
 
  // Return the info title and info list to be shown in the fileInfo groupBox.
  // Get these from the display object.
  virtual QString getInfoTitel() = 0;
  virtual QList<infoItem> getInfoList() = 0;

  // ----- Video ----

  // Does the playlist item prvode video?
  virtual bool providesVideo() { return false; }
  // What is the sampling rate of this playlist item?
  virtual int sampling() { return 0; }
  virtual int frameRate() { return 0; }

  // ------ Statistics ----

  // Does the playlistItem provide statistics?
  virtual bool provideStatistics() { return false; }


  // Does the playlist item currently accept drops of the given item?
  virtual bool acceptDrops(playlistItem *draggingItem) { return false; }

protected:
  QString name;
};

#endif // PLAYLISTITEM_H
