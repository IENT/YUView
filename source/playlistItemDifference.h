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

#include "playlistitemIndexed.h"
#include "videoHandlerDifference.h"

class playlistItemDifference :
  public playlistItemIndexed
{
public:
  playlistItemDifference();
  ~playlistItemDifference() {};

  // The difference item accepts drops of items that provide video
  virtual bool acceptDrops(playlistItem *draggingItem) Q_DECL_OVERRIDE;
  
  virtual QString getInfoTitel() Q_DECL_OVERRIDE { return "Difference Info"; };
  virtual QList<infoItem> getInfoList() Q_DECL_OVERRIDE;

  virtual QString getPropertiesTitle() Q_DECL_OVERRIDE { return "Difference Properties"; }

  // Overload from playlistItemIndexed
  virtual indexRange getstartEndFrameLimits() Q_DECL_OVERRIDE;

  // Overload from playlistItemVideo. 
  virtual QSize getSize() Q_DECL_OVERRIDE;
  
  // Overload from playlistItemVideo. We add some specific drawing functionality if the two
  // children are not comparable.
  virtual void drawItem(QPainter *painter, int frameIdx, double zoomFactor) Q_DECL_OVERRIDE;

  // The children of this item might have changed. If yes, update the properties of this item
  // and emit the signalItemChanged(true, false).
  void updateChildItems() Q_DECL_OVERRIDE;
  
  // Overload from playlistItem. Save the playlist item to playlist.
  virtual void savePlaylist(QDomElement &root, QDir playlistDir) Q_DECL_OVERRIDE;
  // Create a new playlistItemDifference from the playlist file entry. Return NULL if parsing failed.
  static playlistItemDifference *newPlaylistItemDifference(QDomElementYUV stringElement);

  // Get the pixel values from A, B and the difference.
  virtual ValuePairListSets getPixelValues(QPoint pixelPos);
  
protected:
  
private:

  // Overload from playlistItem. Create a properties widget custom to the playlistItemDifference
  // and set propertiesWidget to point to it.
  virtual void createPropertiesWidget() Q_DECL_OVERRIDE;

  videoHandlerDifference difference;
};

#endif
