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

#ifndef VIEWSTATEHANDLER_H
#define VIEWSTATEHANDLER_H

#include <QPoint>
#include <QObject>
#include <QKeyEvent>

class playlistItem;
class PlaybackController;
class splitViewWidget;
class PlaylistTreeWidget;

/* The viewStateHandler saves all values that are needed to restore a certain state of YUView. This is used when the user presses
 * 1...8 (Ctrl+1..8 to save). Saving a state means that we save the following:
 * - Which items are currently selected
 * - The state of the splitViewWidget (zoom factor, position, splitting, splitPosition, viewMode...)
 * - Which frame is currently selected
 */
class viewStateHandler : public QObject
{
  Q_OBJECT

public:
  viewStateHandler();

  void setConctrols(PlaybackController *play, PlaylistTreeWidget *plist, splitViewWidget *primaryView, splitViewWidget *secondaryView);

  bool handleKeyPress(QKeyEvent *event, bool keyFromSeparateView);

private slots:
  void itemAboutToBeDeleted(playlistItem *plItem);

private:

  void saveViewState(int slot, bool saveOnSeparateView);
  void loadViewState(int slot, bool loadOnSeparateView);

  // For the playbackController save the current frame index. This also indicates if a slot is valid or not.
  // If the frame index is -1, the slot is not valid.
  int playbackStateFrameIdx[8];

  // For the playlistTreeWidget, we save a list of the currently selected items
  playlistItem *selectionStates[8][2];

  // Class to save the current view state of the splitViewWidget
  class splitViewWidgetState
  {
  public:
    QPoint centerOffset;
    double zoomFactor;
    bool splitting;
    double splittingPoint;
    int viewMode;
  };
  splitViewWidgetState viewStates[8];

  // Save pointers to the controls
  PlaybackController *playback;
  splitViewWidget *splitView[2];
  PlaylistTreeWidget *playlist;
};

#endif // VIEWSTATEHANDLER_H