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

#include "playlistItemContainer.h"
#include "typedef.h"
#include "ui_playlistItemOverlay.h"

class playlistItemOverlay : public playlistItemContainer
{
  Q_OBJECT

public:
  playlistItemOverlay();

  virtual infoData getInfo() const Q_DECL_OVERRIDE;

  virtual QString getPropertiesTitle() const Q_DECL_OVERRIDE { return "Overlay Properties"; }

  // Overload from playlistItemVideo.
  virtual QSize getSize() const Q_DECL_OVERRIDE;

  // Overload from playlistItemVideo. We add some specific drawing functionality if the two
  // children are not comparable.
  virtual void drawItem(QPainter *painter, int frameIdx, double zoomFactor, bool drawRawData) Q_DECL_OVERRIDE;

  // The children of this item might have changed. If yes, update the properties of this item
  // and emit the signalItemChanged(true).
  void updateChildItems() Q_DECL_OVERRIDE { childLlistUpdateRequired = true; updateChildList(); emit signalItemChanged(true, false); }

  // The overlay item itself does not need to load anything. We just pass all of these to the child items.
  virtual itemLoadingState needsLoading(int frameIdx, bool loadRawData) Q_DECL_OVERRIDE;
  virtual void loadFrame(int frameIdx, bool playing, bool loadRawdata) Q_DECL_OVERRIDE;
  virtual bool isLoading() const Q_DECL_OVERRIDE;

  // Overload from playlistItem. Save the playlist item to playlist.
  virtual void savePlaylist(QDomElement &root, const QDir &playlistDir) const Q_DECL_OVERRIDE;
  // Create a new playlistItemOverlay from the playlist file entry. Return nullptr if parsing failed.
  static playlistItemOverlay *newPlaylistItemOverlay(const QDomElementYUView &stringElement, const QString &filePath);

  virtual ValuePairListSets getPixelValues(const QPoint &pixelPos, int frameIdx) Q_DECL_OVERRIDE;

private slots:
  void controlChanged(int idx);
  void childChanged(bool redraw, bool cacheChanged);

private:

  // Overload from playlistItem. Create a properties widget custom to the playlistItemOverlay
  // and set propertiesWidget to point to it.
  virtual void createPropertiesWidget() Q_DECL_OVERRIDE;

  SafeUi<Ui::playlistItemOverlay_Widget> ui;
  QLayout *createOverlayControls();

  int alignmentMode;
  QPoint manualAlignment;

  // The layout of the child items
  QRect boundingRect;
  QList<QRect> childItems;

  // Update the child item layout and this item's bounding QRect. If checkNumber is true the values
  // will be updated only if the number of items in childItems and childCount() disagree (if new items
  // were added to the overlay)
  void updateLayout(bool checkNumber=true);

  QSpacerItem *vSpacer;

};

#endif // PLAYLISTITEMOVERLAY_H
