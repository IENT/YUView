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

#include <common/Typedef.h>

#include "playlistItemContainer.h"
#include "ui_playlistItemOverlay.h"

#include <QGridLayout>

enum class OverlayLayoutMode
{
  Overlay,
  Arange,
  Custom
};

class playlistItemOverlay : public playlistItemContainer
{
  Q_OBJECT

public:
  playlistItemOverlay();

  virtual InfoData getInfo() const override;

  // Overload from playlistItemVideo.
  virtual Size getSize() const override;

  // Overload from playlistItemVideo. We add some specific drawing functionality if the two
  // children are not comparable.
  virtual void
  drawItem(QPainter *painter, int frameIdx, double zoomFactor, bool drawRawData) override;

  // The overlay item itself does not need to load anything. We just pass all of these to the child
  // items.
  virtual ItemLoadingState needsLoading(int frameIdx, bool loadRawData) override;
  // Load the frame in the video item. Emit SignalItemChanged(true,false) when done. Always called
  // from a thread.
  virtual void
  loadFrame(int frameIdx, bool playing, bool loadRawData, bool emitSignals = true) override;

  // Is an image currently being loaded?
  virtual bool isLoading() const override;
  virtual bool isLoadingDoubleBuffer() const override;

  // Overload from playlistItem. Save the playlist item to playlist.
  virtual void savePlaylist(QDomElement &root, const QDir &playlistDir) const override;
  // Create a new playlistItemOverlay from the playlist file entry. Return nullptr if parsing
  // failed.
  static playlistItemOverlay *newPlaylistItemOverlay(const YUViewDomElement &stringElement,
                                                     const QString &         filePath);

  virtual ValuePairListSets getPixelValues(const QPoint &pixelPos, int frameIdx) override;

  void guessBestLayout();

private:
  OverlayLayoutMode layoutMode{OverlayLayoutMode::Overlay};

  void onGroupBoxToggled(int idx, bool on);

  // Overload from playlistItem. Create a properties widget custom to the playlistItemOverlay
  // and set propertiesWidget to point to it.
  virtual void createPropertiesWidget() override;

  SafeUi<Ui::playlistItemOverlay_Widget> ui;

  QRect               boundingRect;   //< The bounding rect of the complete overlay
  QList<QRect>        childItemRects; //< The position and size of each child item
  QList<unsigned int> childItemsIDs;  //< The ID of every child item

  // Update the child item layout and this item's bounding QRect. If onlyIfItemsChanged is true the
  // values will be updated only if the number or oder of items changed.
  void updateLayout(bool onlyIfItemsChanged = true);

  // The grid layout that contains all the custom positions
  QGridLayout *customPositionGrid{};
  void         updateCustomPositionGrid();
  void         clear(int startRow);
  QPoint       getCutomPositionOfItem(int itemIndex) const;

  int               overlayMode{0};
  int               arangementMode{0};
  QMap<int, QPoint> customPositions;

private slots:
  void slotControlChanged();
  void childChanged(bool redraw, recacheIndicator recache) override;

  void on_overlayGroupBox_toggled(bool on) { this->onGroupBoxToggled(0, on); }
  void on_arangeGroupBox_toggled(bool on) { this->onGroupBoxToggled(1, on); };
  void on_customGroupBox_toggled(bool on) { this->onGroupBoxToggled(2, on); };
};
