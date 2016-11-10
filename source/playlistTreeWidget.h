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

#ifndef PLAYLISTTREEWIDGET_H
#define PLAYLISTTREEWIDGET_H

#include <QPointer>
#include <QTreeWidget>
#include <QDockWidget>
#include "QMouseEvent"

#include "playlistItem.h"
#include "viewStateHandler.h"

/* The PlaylistTreeWidget is the widget that contains all the playlist items.
 *
 * It accapts dropping of things onto it and dragging of items within it.
 */
class PlaylistTreeWidget : public QTreeWidget
{
  Q_OBJECT
public:
  explicit PlaylistTreeWidget(QWidget *parent = 0);
  
  // Are there changes in the playlist that haven't been saved yet?
  // If the playlist is empty, this will always return true.
  bool getIsSaved() { return (topLevelItemCount() == 0) ? true : p_isSaved; }

  // load the given files into the playlist
  void loadFiles(QStringList files);

  // Remove the selected / all items from the playlist tree widget and delete them
  void deleteSelectedPlaylistItems();
  void deleteAllPlaylistItems();

  Qt::DropActions supportedDropActions() const;

  QModelIndex indexForItem(playlistItem * item) { return indexFromItem((QTreeWidgetItem*)item); }

  // Get the first two selected items
  void getSelectedItems(playlistItem *&first, playlistItem *&second);
  // Set (up to two) selected items
  void setSelectedItems(playlistItem *item1, playlistItem *item2);

  // Is there a next item? Is the currently selected item the last one in the playlist?
  bool hasNextItem();

  // Check if the source of the items is still up to date. If not aske the user if he wants to reload the item.
  void checkAndUpdateItems();

  void setViewStateHandler(viewStateHandler *handler) { stateHandler = handler; }

  // Return a list with all playlist items (also all child items)
  QList<playlistItem*> getAllPlaylistItems();
  
public slots:
  void savePlaylistToFile();

  // The settings were changed by the user. Reload all settings that affect the tree and the playlist items.
  void updateSettings();

  // Slots for going to the next item. WrapAround = if the current item is the last one in the list, 
  // goto the first item in the list. Return if there is a next item.
  // callByPlayback is true if this call is caused by the playback function going to the next item.
  bool selectNextItem(bool wrapAround=false, bool callByPlayback=false);
  // Goto the previous item
  void selectPreviousItem();

  // Slots for adding text/difference items
  void addTextItem();
  void addDifferenceItem();
  void addOverlayItem();

signals:
  // The user requests to show the open filel dialog
  void openFileDialog();

  // The current selection changed. Give the new first (and second) selection.
  // chageByPlayback is true if the selection changed because of the playback going to the next frame.
  void selectionRangeChanged(playlistItem *first, playlistItem *second, bool chageByPlayback);

  // The properties of (one of) the currently selected items changed.
  // We need to update the values of the item. Redraw the item if redraw is set.
  void selectedItemChanged(bool redraw);

  // The item is about to be deleted. Last chance to do something with it.
  void itemAboutToBeDeleted(playlistItem *item);
  void startCachingCurrentSelection(indexRange range);
  void removeFromCacheCurrentSelection(indexRange range);

  // Something about the playlist changed (an item was moved, a different item was selected,
  // an item was deleted). This is used by the videoCache to rethink what to cache next.
  void playlistChanged();

  void saveViewStatesToPlaylist(QDomElement &root);

protected:
  // Overload from QWidget to create a custom context menu
  virtual void contextMenuEvent(QContextMenuEvent * event) Q_DECL_OVERRIDE;
  // Overload from QWidget to capture file drops onto the playlist
  virtual void dropEvent(QDropEvent *event) Q_DECL_OVERRIDE;
  // Overload from QWidget to determine if we can accept this item for dropping
  virtual void dragEnterEvent(QDragEnterEvent *event) Q_DECL_OVERRIDE;
  // Overload from QWidget to set if the item being dragged can be dropped onto the item under the cursor
  void dragMoveEvent(QDragMoveEvent* event) Q_DECL_OVERRIDE;
  // Overload from QWidget to ...
  virtual void mousePressEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
  // Overload from QAbstractItemView. This will changed the defaul QTreeWidget behavior for the keys 1..8.
  virtual void keyPressEvent(QKeyEvent *event) Q_DECL_OVERRIDE;

protected slots:

  // Connect the signal QTreeWidget::itemSelectionChanged() here
  // Note: Previously tried to overload QAbstractItemView::currentChanged(...). Did not work becuase when currentChanged
  // is called, selectedItems does not return the correct selection list. When slotSelectionChanged is called, the list
  // of selected items is correct.
  void slotSelectionChanged();

  // All item's signals signalItemChanged are connected here. Check if the item which sent the signal is currently
  // selected. If yes, emit the signal selectionInfoChanged().
  void slotItemChanged(bool redraw, bool cacheChanged);

private:

  //
  playlistItem* getDropTarget(QPoint pos);

  // Load the given playlist file
  void loadPlaylistFile(QString filePath);

  // If the playlist is changed and the changes have not been saved yet, this will be true.
  bool p_isSaved;

  // Update all items that have children. This is called when an item is deleted (it might be one from a 
  // compbound item) or something is dropped (might be dropped on a compbound item)
  void updateAllContainterItems();

  // In the QSettings we keep a list of recent files. Add the given file.
  void addFileToRecentFileSetting(QString file);

  // Append the new item at the end of the playlist and connect signals/slots
  void appendNewItem(playlistItem *item, bool emitplaylistChanged = true);

  // Clone the selected item as often as the user wants
  void cloneSelectedItem();

  // We have a pointer to the viewStateHandler to load/save the view states to playlist
  QPointer<viewStateHandler> stateHandler;
};

#endif // PLAYLISTTREEWIDGET_H
