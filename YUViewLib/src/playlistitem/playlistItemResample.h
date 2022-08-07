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

#include "playlistItemContainer.h"
#include "video/videoHandlerResample.h"

#include "ui_playlistItemResample.h"

class playlistItemResample : public playlistItemContainer
{
  Q_OBJECT

public:
  playlistItemResample();

  virtual InfoData getInfo() const override;

  // Overload from playlistItemVideo.
  virtual Size getSize() const override;
  
  // Overload from playlistItemVideo. We add some specific drawing functionality if the two children are not comparable.
  virtual void drawItem(QPainter *painter, int frameIdx, double zoomFactor, bool drawRawData) override;

  virtual void activateDoubleBuffer() override { this->video.activateDoubleBuffer(); }

  // Do we need to load the given frame first?
  virtual ItemLoadingState needsLoading(int frameIdx, bool loadRawData) override;
  // This is part of the caching interface. The loadFrame function is always called from a different thread.
  virtual void loadFrame(int frameIdx, bool playing, bool loadRawData, bool emitSignals=true) override;
  virtual bool isLoading() const override { return this->isFrameLoading; }
  virtual bool isLoadingDoubleBuffer() const override { return this->isFrameLoadingDoubleBuffer; }

  // Overload from playlistItem. Save the playlist item to playlist.
  virtual void savePlaylist(QDomElement &root, const QDir &playlistDir) const override;
  // Create a new playlistItemResample from the playlist file entry. Return nullptr if parsing failed.
  static playlistItemResample *newPlaylistItemResample(const YUViewDomElement &stringElement);

  // Get the pixel values from A, B and the difference.
  virtual ValuePairListSets getPixelValues(const QPoint &pixelPos, int frameIdx) override;

  // Return the frame handler pointer that draws the difference
  virtual video::FrameHandler *getFrameHandler() override { return &video; }

protected slots:
  void childChanged(bool redraw, recacheIndicator recache) override;

private slots:
  void slotResampleControlChanged(int value);
  void slotInterpolationModeChanged(int value);
  void slotCutAndSampleControlChanged(int value);
  void slotButtonSARWidth(bool selected);
  void slotButtonSARHeight(bool selected);

private:

  // Overload from playlistItem. Create a properties widget custom to the playlistItemResample
  // and set propertiesWidget to point to it.
  virtual void createPropertiesWidget() override;

  video::videoHandlerResample video;

  Size scaledSize {0, 0};
  int interpolationIndex {0};
  IndexRange cutRange {0, 0};
  int sampling {1};

  bool useLoadedValues {false};

  // Is the loadFrame function currently loading?
  bool isFrameLoading {false};
  bool isFrameLoadingDoubleBuffer {false};

  SafeUi<Ui::playlistItemResample> ui;
};
