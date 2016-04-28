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

#ifndef PLAYLISTITEMINDEXED_H
#define PLAYLISTITEMINDEXED_H

#include "typedef.h"
#include <assert.h>

#include "playlistitem.h"

#include "ui_playlistItemIndexed.h"

class playlistItemIndexed :
  public playlistItem
{
  Q_OBJECT

public:

  playlistItemIndexed(QString itemNameOrFileName);
  virtual ~playlistItemIndexed() {}

  // An indexed playlist item is indexed by frame (duh)
  virtual bool       isIndexedByFrame()   Q_DECL_OVERRIDE Q_DECL_FINAL { return true;          }
  virtual indexRange getFrameIndexRange() Q_DECL_OVERRIDE Q_DECL_FINAL { return startEndFrame; }
  virtual double     getFrameRate()       Q_DECL_OVERRIDE Q_DECL_FINAL { return frameRate;     }
  virtual int        getSampling()        Q_DECL_OVERRIDE Q_DECL_FINAL { return sampling;      }

  /* If you inherit from this class (your playlist item is indexed by frame), you must
    provide the absolute minimum and maximum frame indices that the user can set.
    Normally this is: (0, numFrames-1). This value can change. Just emit a
    signalItemChanged to update the limits.
  */
  virtual indexRange getstartEndFrameLimits() = 0;

protected slots:
  // A control of the indexed item (start/end/frameRate/sampling) changed
  void slotVideoControlChanged();

protected:

  // Add the control for the time that this item is shown to
  QLayout *createIndexControllers(QWidget *parentWidget);

  void setStartEndFrame(indexRange range, bool emitSignal);

  // For an indexed item we save the start/end, sampling and frame rate to the playlist
  void appendPropertiesToPlaylist(QDomElementYUV &d);

  // Load the start/end frame, sampling and frame rate from playlist
  static void loadPropertiesFromPlaylist(QDomElementYUV root, playlistItemIndexed *newItem);

  double     frameRate;
  int        sampling;
  indexRange startEndFrame;

private:
  bool controlsCreated;

  Ui::playlistItemIndexed ui;

};

#endif // PLAYLISTITEMINDEXED_H
