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

#ifndef PLAYLISTITEMDIFFERENCE_H
#define PLAYLISTITEMDIFFERENCE_H

#include "playlistItem.h"

class playlistItemDifference :
  public playlistItem
{
public:
  playlistItemDifference();
  ~playlistItemDifference();

  // Overload from playlistItem. Save the difference item to playlist.
  virtual void savePlaylist(QDomDocument &doc, QDomElement &root, QDir playlistDir) {}

  bool isIndexedByFrame() { return true; }

  // The difference item can provide the difference of two items that provide video
  virtual bool providesVideo() { return true; }

  // The difference item accepts drops of items that provide video
  virtual bool acceptDrops(playlistItem *draggingItem);

  virtual QString getInfoTitel() { return "Difference Info"; };
  virtual QList<infoItem> getInfoList();

  virtual QString getPropertiesTitle() { return "Difference Properties"; }

  virtual bool isCaching() Q_DECL_OVERRIDE {return false;}
public slots:
  // TODO: this does not do anything yet
  virtual void startCaching(indexRange range) {}
  virtual void stopCaching() {}


protected:

};

#endif
