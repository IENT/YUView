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

#include "playlistItemVideo.h"

class playlistItemDifference :
  public playlistItemVideo
{
public:
  playlistItemDifference();
  ~playlistItemDifference();

  // Overload from playlistItem. Save the difference item to playlist.
  virtual void savePlaylist(QDomDocument &doc, QDomElement &root, QDir playlistDir) {}

  // The difference item accepts drops of items that provide video
  virtual bool acceptDrops(playlistItem *draggingItem);

  virtual QString getInfoTitel() { return "Difference Info"; };
  virtual QList<infoItem> getInfoList();

  virtual QString getPropertiesTitle() { return "Difference Properties"; }

  // Overload from playlistItemVideo. 
  virtual qint64 getNumberFrames() Q_DECL_OVERRIDE;

  // Overload from playlistItemVideo. We add some specific drawing functionality if the two
  // children are not comparable.
  virtual void drawFrame(QPainter *painter, int frameIdx, double zoomFactor) Q_DECL_OVERRIDE;

  // The children of this item might have changed. If yes, update the properties of this item
  // and emit the signalItemChanged(true).
  void updateChildren();

  // Get the values of the two input items and the difference
  virtual ValuePairList getPixelValues(QPoint pixelPos);

  virtual bool isCaching() Q_DECL_OVERRIDE {return false;}
  
public slots:
  // TODO: this does not do anything yet
  virtual void startCaching(indexRange range) {}
  virtual void stopCaching() {}
  virtual void removeFromCache(indexRange range) {};
  
protected:

  // Overloaded from playlistItemVideo. Actually load the given frame.
  virtual void loadFrame(int frameIdx) Q_DECL_OVERRIDE;
  
private:

  // Overload from playlistItem. Create a properties widget custom to the playlistItemDifference
  // and set propertiesWidget to point to it.
  virtual void createPropertiesWidget() Q_DECL_OVERRIDE;

  // Pointers to the two sources that we are going to calculate the difference from
  playlistItemVideo *inputVideo[2];

  // The conversion function can provide some information to show.
  QList<infoItem> conversionInfoList;
};

#endif
