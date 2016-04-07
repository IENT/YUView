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

#ifndef PLAYLISTITEMOVERLAY_H
#define PLAYLISTITEMOVERLAY_H

#include "playlistItem.h"
#include "ui_playlistItemOverlay.h"

class playlistItemOverlay :
  public playlistItem
{
  Q_OBJECT

public:
  playlistItemOverlay();
  ~playlistItemOverlay() {};

  // The difference item accepts drops of items that provide video
  virtual bool acceptDrops(playlistItem *draggingItem) Q_DECL_OVERRIDE { return true; }

  // The difference item is indexed by frame
  virtual bool isIndexedByFrame() Q_DECL_OVERRIDE { return true; }
  virtual indexRange getFrameIndexRange() { return (getFirstChildPlaylistItem() == NULL) ? indexRange(-1,-1) : getFirstChildPlaylistItem()->getFrameIndexRange(); }

  virtual QString getInfoTitel() Q_DECL_OVERRIDE { return "Overlay Info"; };
  virtual QList<infoItem> getInfoList() Q_DECL_OVERRIDE;

  virtual QString getPropertiesTitle() Q_DECL_OVERRIDE { return "Overlay Properties"; }

  // Overload from playlistItemVideo. 
  virtual double getFrameRate() Q_DECL_OVERRIDE { return (getFirstChildPlaylistItem() == NULL) ? 0 : getFirstChildPlaylistItem()->getFrameRate(); }
  // TODO: The size depends on the number of child items and their positioning
  virtual QSize  getSize()      Q_DECL_OVERRIDE { return (getFirstChildPlaylistItem() == NULL) ? QSize() : getFirstChildPlaylistItem()->getSize(); }
  virtual int    getSampling()  Q_DECL_OVERRIDE { return (getFirstChildPlaylistItem() == NULL) ? 1 : getFirstChildPlaylistItem()->getSampling(); }

  // Overload from playlistItemVideo. We add some specific drawing functionality if the two
  // children are not comparable.
  virtual void drawItem(QPainter *painter, int frameIdx, double zoomFactor) Q_DECL_OVERRIDE;
  
  virtual bool isCaching() Q_DECL_OVERRIDE {return false;}

  // Overload from playlistItem. Save the playlist item to playlist.
  virtual void savePlaylist(QDomElement &root, QDir playlistDir) Q_DECL_OVERRIDE;
  // Create a new playlistItemDifference from the playlist file entry. Return NULL if parsing failed.
  static playlistItemOverlay *newPlaylistItemDifference(QDomElementYUV stringElement, QString filePath);

  virtual ValuePairList getPixelValues(QPoint pixelPos) Q_DECL_OVERRIDE;
  
protected:

private slots:
  void alignmentModeChanged(int idx);
  void manualAlignmentVerChanged(int val);
  void manualAlignmentHorChanged(int val);
  
private:

  // Overload from playlistItem. Create a properties widget custom to the playlistItemDifference
  // and set propertiesWidget to point to it.
  virtual void createPropertiesWidget() Q_DECL_OVERRIDE;

  // Return the first child item (as playlistItem) or NULL if there is no child.
  playlistItem *getFirstChildPlaylistItem();

  Ui_playlistItemOverlay_Widget ui;

  int alignmentMode;
  int manualAlignment[2];
};

#endif // PLAYLISTITEMOVERLAY_H
