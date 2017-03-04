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

#ifndef VIEWSTATEHANDLER_H
#define VIEWSTATEHANDLER_H

#include <QPoint>
#include <QPointer>

class QDomElement;
class QKeyEvent;
class PlaybackController;
class playlistItem;
class PlaylistTreeWidget;
class splitViewWidget;

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

  // Set the controls which we save/load the states from/to.
  void setConctrols(PlaybackController *play, PlaylistTreeWidget *plist, splitViewWidget *primaryView, splitViewWidget *secondaryView);

  // Handle the key press (if we handle these keys). Return if the key press was consumed.
  bool handleKeyPress(QKeyEvent *event, bool keyFromSeparateView);

  // Save/Load the view states to/from the playlist
  void savePlaylist(QDomElement &plist);
  void loadPlaylist(const QDomElement &viewStateNode);

  // Save/load the view state to/from the given slot number
  void saveViewState(int slot, bool saveOnSeparateView);
  void loadViewState(int slot, bool loadOnSeparateView);

public slots:
  void saveViewState1() { saveViewState(1, false); }
  void saveViewState2() { saveViewState(2, false); }
  void saveViewState3() { saveViewState(3, false); }
  void saveViewState4() { saveViewState(4, false); }
  void saveViewState5() { saveViewState(5, false); }
  void saveViewState6() { saveViewState(6, false); }
  void saveViewState7() { saveViewState(7, false); }
  void saveViewState8() { saveViewState(8, false); }

  void loadViewState1() { loadViewState(1, false); }
  void loadViewState2() { loadViewState(2, false); }
  void loadViewState3() { loadViewState(3, false); }
  void loadViewState4() { loadViewState(4, false); }
  void loadViewState5() { loadViewState(5, false); }
  void loadViewState6() { loadViewState(6, false); }
  void loadViewState7() { loadViewState(7, false); }
  void loadViewState8() { loadViewState(8, false); }

private:

  int playbackStateFrameIdxData[8];

  // For the playbackController save the current frame index. This also indicates if a slot is valid or not.
  // If the frame index is -1, the slot is not valid.
  int playbackStateFrameIdx(int index) const;
  int & playbackStateFrameIdx(int index);

  // For the playlistTreeWidget, we save a list of the currently selected items
  QPointer<playlistItem> selectionStates[8][2];

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
  QPointer<PlaybackController> playback;
  QPointer<splitViewWidget> splitView[2];
  QPointer<PlaylistTreeWidget> playlist;
};

#endif // VIEWSTATEHANDLER_H