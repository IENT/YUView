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

  // The overlay item accepts drops of items that provide video
  virtual bool acceptDrops(playlistItem *draggingItem) Q_DECL_OVERRIDE { return true; }

  // The overlay item is indexed by frame
  virtual bool isIndexedByFrame() Q_DECL_OVERRIDE { return true; }
  virtual indexRange getFrameIndexRange() { return (getFirstChildPlaylistItem() == NULL) ? indexRange(-1,-1) : getFirstChildPlaylistItem()->getFrameIndexRange(); }

  virtual QString getInfoTitel() Q_DECL_OVERRIDE { return "Overlay Info"; };
  virtual QList<infoItem> getInfoList() Q_DECL_OVERRIDE;

  virtual QString getPropertiesTitle() Q_DECL_OVERRIDE { return "Overlay Properties"; }

  // Overload from playlistItemVideo. 
  virtual double getFrameRate() Q_DECL_OVERRIDE { return (getFirstChildPlaylistItem() == NULL) ? 0 : getFirstChildPlaylistItem()->getFrameRate(); }
  virtual QSize  getSize()      Q_DECL_OVERRIDE { return boundingRect.size(); }
  virtual int    getSampling()  Q_DECL_OVERRIDE { return (getFirstChildPlaylistItem() == NULL) ? 1 : getFirstChildPlaylistItem()->getSampling(); }

  // Overload from playlistItemVideo. We add some specific drawing functionality if the two
  // children are not comparable.
  virtual void drawItem(QPainter *painter, int frameIdx, double zoomFactor) Q_DECL_OVERRIDE;

  // The children of this item might have changed. If yes, update the properties of this item
  // and emit the signalItemChanged(true).
  void updateChildItems() Q_DECL_OVERRIDE { childLlistUpdateRequired = true; emit signalItemChanged(true, false); }
  // An item will be deleted. Disconnect the signals/slots of this item
  virtual void itemAboutToBeDeleter(playlistItem *item) Q_DECL_OVERRIDE;
  
  // Overload from playlistItem. Save the playlist item to playlist.
  virtual void savePlaylist(QDomElement &root, QDir playlistDir) Q_DECL_OVERRIDE;
  // Create a new playlistItemOverlay from the playlist file entry. Return NULL if parsing failed.
  static playlistItemOverlay *newPlaylistItemOverlay(QDomElementYUV stringElement, QString filePath);

  virtual ValuePairListSets getPixelValues(QPoint pixelPos) Q_DECL_OVERRIDE;
  
protected:

private slots:
  void controlChanged(int idx);
  void childChanged(bool redraw);
    
private:

  // Overload from playlistItem. Create a properties widget custom to the playlistItemOverlay
  // and set propertiesWidget to point to it.
  virtual void createPropertiesWidget() Q_DECL_OVERRIDE;

  // Return the first child item (as playlistItem) or NULL if there is no child.
  playlistItem *getFirstChildPlaylistItem();

  Ui_playlistItemOverlay_Widget ui;

  int alignmentMode;
  QPoint manualAlignment;

  // The layout of the child items
  Rect boundingRect;
  QList<Rect> childItems;

  // Update the child item layout and this item's bounding rect. If checkNumber is true the values
  // will be updated only if the number of items in childItems and childCount() disagree (if new items
  // were added to the overlay)
  void updateLayout(bool checkNumber=true);

  // We keep a list of pointers to our child items so we can disconnect their signals if they are removed.
  void updateChildList();
  QList<playlistItem*> childList;
  bool childLlistUpdateRequired;

};

#endif // PLAYLISTITEMOVERLAY_H
