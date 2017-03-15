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

#include "viewStateHandler.h"

#include <QKeyEvent>
#include "playbackController.h"

viewStateHandler::viewStateHandler()
{
}

int viewStateHandler::playbackStateFrameIdx(int i) const
{
  return (selectionStates[i][0] || selectionStates[i][1]) ? playbackStateFrameIdxData[i] : -1;
}

int & viewStateHandler::playbackStateFrameIdx(int i) {
  if (!selectionStates[i][0] && !selectionStates[i][1])
    playbackStateFrameIdxData[i] = -1;
  return playbackStateFrameIdxData[i];
}

void viewStateHandler::setConctrols(PlaybackController *play, PlaylistTreeWidget *plist, splitViewWidget *primaryView, splitViewWidget *secondaryView)
{
  playback = play; 
  splitView[0] = primaryView;
  splitView[1] = secondaryView;
  playlist = plist;

}

bool viewStateHandler::handleKeyPress(QKeyEvent *event, bool keyFromSeparateView)
{
  int key = event->key();
  bool controlOnly = (event->modifiers() == Qt::ControlModifier);

  if (key == Qt::Key_1 || key == Qt::Key_2 || key == Qt::Key_3 || key == Qt::Key_4 || key == Qt::Key_5 || key == Qt::Key_6 || key == Qt::Key_7 || key == Qt::Key_8)
  {
    // The original idea was to use Ctr+Shift+1..8 to save and Ctr+1..8 to load. However, this does not work with Qt because
    // Shift+1..8 results in key events depending on the used keyboard layout and there is no way to get the actual button.
    // So now we use Ctrl+(1..8) to save and (1..8) to load.
    int slot = key - Qt::Key_1;
    if (controlOnly)
      saveViewState(slot, keyFromSeparateView);
    else if (event->modifiers() == Qt::NoModifier)
      loadViewState(slot, keyFromSeparateView);
    return true;
  }

  return false;
}

void viewStateHandler::saveViewState(int slot, bool saveOnSeparateView)
{
  if (slot < 0 || slot >= 8)
    // Only eight slots
    return;

  // Get the selected items from the playlist
  playlistItem *item1, *item2;
  playlist->getSelectedItems(item1, item2);
  selectionStates[slot][0] = item1;
  selectionStates[slot][1] = item2;

  // Get the current frame index from the playbackController
  playbackStateFrameIdx(slot) = playback->getCurrentFrame();

  // Get the state of the view from the splitView
  if (saveOnSeparateView)
    splitView[1]->getViewState(viewStates[slot].centerOffset, viewStates[slot].zoomFactor, viewStates[slot].splitting, viewStates[slot].splittingPoint, viewStates[slot].viewMode);
  else
    splitView[0]->getViewState(viewStates[slot].centerOffset, viewStates[slot].zoomFactor, viewStates[slot].splitting, viewStates[slot].splittingPoint, viewStates[slot].viewMode);
}

// Load the view state for a specific slot. If the views are linked, the behavior is different if loading was toggled
// from the primary, or the separate view (loadOnSeparateView).
void viewStateHandler::loadViewState(int slot, bool loadOnSeparateView)
{
  if (slot < 0 || slot >= 8)
    // Only eight slots
    return;

  if (playbackStateFrameIdx(slot) == -1)
    // Slot not valid
    return;

  // First load the correct selection
  playlist->setSelectedItems(selectionStates[slot][0], selectionStates[slot][1]);
    
  // Then load the correct frame index
  playback->setCurrentFrame(playbackStateFrameIdx(slot));

  if (loadOnSeparateView)
    splitView[1]->setViewState(viewStates[slot].centerOffset, viewStates[slot].zoomFactor, viewStates[slot].splitting, viewStates[slot].splittingPoint, viewStates[slot].viewMode);
  else
    splitView[0]->setViewState(viewStates[slot].centerOffset, viewStates[slot].zoomFactor, viewStates[slot].splitting, viewStates[slot].splittingPoint, viewStates[slot].viewMode);
}

void viewStateHandler::savePlaylist(QDomElement &root)
{
  for (int i = 0; i < 8; i++)
  {
    if (playbackStateFrameIdx(i) != -1)
    {
      // Create a new entry for this slot
      QDomElementYUView state = root.ownerDocument().createElement(QString("slot%1").arg(i));
      root.appendChild(state);

      // Append the frame index
      state.appendProperiteChild("frameIdx", QString::number(playbackStateFrameIdx(i)));
      // Append the IDs of the selected items
      if (selectionStates[i][0] != NULL)
        state.appendProperiteChild("itemID1",  QString::number(selectionStates[i][0]->getID()));
      if (selectionStates[i][1] != NULL)
        state.appendProperiteChild("itemID2",  QString::number(selectionStates[i][1]->getID()));
      // Append the state of the split view
      state.appendProperiteChild("centerOffsetX", QString::number(viewStates[i].centerOffset.x()));
      state.appendProperiteChild("centerOffsetY", QString::number(viewStates[i].centerOffset.y()));
      if (viewStates[i].splitting)
      {
        state.appendProperiteChild("splitting", "1");
        state.appendProperiteChild("splittingPoint", QString::number(viewStates[i].splittingPoint));
      }
      state.appendProperiteChild("viewMode", QString::number(viewStates[i].viewMode));
      state.appendProperiteChild("zoomFactor", QString::number(viewStates[i].zoomFactor));
    }
  }

  // Finally, save the current state
  QDomElementYUView state = root.ownerDocument().createElement(QString("current"));
  root.appendChild(state);
  
  // Append the frame index
  state.appendProperiteChild("frameIdx", QString::number(playback->getCurrentFrame()));
  
  // Append the IDs of the selected items
  playlistItem *item1, *item2;
  playlist->getSelectedItems(item1, item2);
  if (item1 != NULL)
    state.appendProperiteChild("itemID1",  QString::number(item1->getID()));
  if (item2 != NULL)
    state.appendProperiteChild("itemID2",  QString::number(item2->getID()));
  
  // Append the state of the split view
  splitViewWidgetState viewState;
  splitView[0]->getViewState(viewState.centerOffset, viewState.zoomFactor, viewState.splitting, viewState.splittingPoint, viewState.viewMode);
  state.appendProperiteChild("centerOffsetX", QString::number(viewState.centerOffset.x()));
  state.appendProperiteChild("centerOffsetY", QString::number(viewState.centerOffset.y()));
  if (viewState.splitting)
  {
    state.appendProperiteChild("splitting", "1");
    state.appendProperiteChild("splittingPoint", QString::number(viewState.splittingPoint));
  }
  state.appendProperiteChild("viewMode", QString::number(viewState.viewMode));
  state.appendProperiteChild("zoomFactor", QString::number(viewState.zoomFactor));

}

void viewStateHandler::loadPlaylist(const QDomElement &viewStateNode)
{
  // In order to get the pointers to the right playlist items, we need a list of all playlist items
  QList<playlistItem*> allPlaylistItems = playlist->getAllPlaylistItems();

  QDomNode n = viewStateNode.firstChild();
  while (!n.isNull()) 
  {
    QDomElementYUView elem = n.toElement();
    if (!n.isElement())
    {
      n = n.nextSibling();
      continue;
    }
    n = n.nextSibling();

    QString name = elem.tagName();
    if (!name.startsWith("slot") && name != "current")
      continue;

    int id = name.right(1).toInt();
    if (name != "current" && (id < 0 || id >= 8))
      continue;

    // Get all the values from the entry
    int frameIdx = elem.findChildValue("frameIdx").toInt();
    
    // Get the selection item ID's (set to -1 if not found)
    bool ok;
    int itemId1 = elem.findChildValue("itemID1").toInt(&ok);
    if (!ok)
      itemId1 = -1;
    int itemId2 = elem.findChildValue("itemID2").toInt(&ok);
    if (!ok)
      itemId2 = -1;

    int centerOffsetX = elem.findChildValue("centerOffsetX").toInt();
    int centerOffsetY = elem.findChildValue("centerOffsetY").toInt();
    bool splitting = false;
    double splittingPoint = 0.5;
    if (elem.findChildValue("splitting") == "1")
    {
      splitting = true;
      splittingPoint = elem.findChildValue("splittingPoint").toDouble();
    }
    int viewMode = elem.findChildValue("viewMode").toInt();
    double zoomFactor = elem.findChildValue("zoomFactor").toDouble();
          
    // Perform a quick check if the values seem correct
    if (frameIdx < 0 || itemId1 < -1 || itemId2 < -1)
      continue;
    if (splitting && (splittingPoint < 0 || splittingPoint > 1))
      continue;
    if (viewMode < 0 || viewMode > 1 || zoomFactor < 0)
      continue;

    // Everything seems to be OK
    // Search through the allPlaylistItems list and get the pointer to the item with the given playlist id
    playlistItem *item1 = NULL;
    playlistItem *item2 = NULL;
    for (int i = 0; i < allPlaylistItems.count(); i++)
    {
      if (itemId1 > -1 && allPlaylistItems[i]->getPlaylistID() == (unsigned int)itemId1)
        item1 = allPlaylistItems[i];
      if (itemId2 > -1 && allPlaylistItems[i]->getPlaylistID() == (unsigned int)itemId2)
        item2 = allPlaylistItems[i];
    }

    if (name == "current")
    {
      // Set the state as the current state
      // First load the correct selection
      playlist->setSelectedItems(item1, item2);

      // Then load the correct frame index
      playback->setCurrentFrame(frameIdx);

      // Set the values for the split view
      splitView[0]->setViewState(QPoint(centerOffsetX, centerOffsetY), zoomFactor, splitting, splittingPoint, viewMode);
    }
    else
    {
      // Save the values to the slot.
      // Save the frame index
      playbackStateFrameIdx(id) = frameIdx;
      
      // Save the item pointers
      selectionStates[id][0] = item1;
      selectionStates[id][1] = item2;
      
      // Save the view state
      viewStates[id].centerOffset = QPoint(centerOffsetX, centerOffsetY);
      viewStates[id].splitting = splitting;
      viewStates[id].splittingPoint = splittingPoint;
      viewStates[id].viewMode = viewMode;
      viewStates[id].zoomFactor = zoomFactor;
    }
  }

  // Loading is done. Reset all the loaded playlist IDs from the playlist items.
  for (int i = 0; i < allPlaylistItems.count(); i++)
    allPlaylistItems[i]->resetPlaylistID();
}