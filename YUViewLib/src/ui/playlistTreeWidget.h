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

#ifndef PLAYLISTTREEWIDGET_H
#define PLAYLISTTREEWIDGET_H

#include <array>
#include <QPointer>
#include <QTimer>
#include <QTreeWidget>

#include "playlistitem/playlistItem.h"
#include "common/typedef.h"
#include "viewStateHandler.h"

class QDomElement;

/* The PlaylistTreeWidget is the widget that contains all the playlist items.
 *
 * It accapts dropping of things onto it and dragging of items within it.
 */
class PlaylistTreeWidget : public QTreeWidget
{
  Q_OBJECT

public:
  explicit PlaylistTreeWidget(QWidget *parent = 0);
  ~PlaylistTreeWidget();
  
  // Are there changes in the playlist that haven't been saved yet?
  // If the playlist is empty, this will always return true.
  bool getIsSaved() { return (topLevelItemCount() == 0) ? true : isSaved; }

  void loadFiles(const QStringList &files);
  void deletePlaylistItems(bool deleteAllItems);

  // Get a list of all playlist items that are currently in the playlist. Including all child items.
  QList<playlistItem*> getAllPlaylistItems(const bool topLevelOnly=false) const;

  Qt::DropActions supportedDropActions() const Q_DECL_OVERRIDE;
  QModelIndex indexForItem(playlistItem *item) { return indexFromItem((QTreeWidgetItem*)item); }

  // Get the first two selected items
  std::array<playlistItem *, 2> getSelectedItems() const;
  // Set (up to two) selected items
  void setSelectedItems(playlistItem *item1, playlistItem *item2);

  // Is there a next item? Is the currently selected item the last one in the playlist?
  bool hasNextItem();

  // Check if the source of the items is still up to date. If not aske the user if he wants to reload the item.
  void checkAndUpdateItems();

  void setViewStateHandler(viewStateHandler *handler) { stateHandler = handler; }

  // The settings were changed by the user. Reload all settings that affect the tree and the playlist items.
  void updateSettings();

  // Update the caching status of all items
  void updateCachingStatus() { emit dataChanged(QModelIndex(), QModelIndex()); };

  bool isAutosaveAvailable();
  void loadAutosavedPlaylist();
  void dropAutosavedPlaylist();
  void startAutosaveTimer();
  
public slots:
  void savePlaylistToFile();

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

  // Something changed for the given item so that all cached frames are now invalid. This is 
  // connected to the cache so that it can recache the item.
  void signalItemRecache(playlistItem *item, recacheIndicator clearItemCache);

  // Emit that the item is about to be delete. This will do two things:
  // - The property widget will remove the properties panel from the stacked widget panel (but not delete it because
  //   is it owned by the playlist item itself)
  // - The video cache will actually delete the item (or scheduel it for deletion if caching/loading is still running)
  void itemAboutToBeDeleted(playlistItem *item);

  void startCachingCurrentSelection(indexRange range);
  void removeFromCacheCurrentSelection(indexRange range);

  // Something about the playlist changed (an item was moved, a different item was selected,
  // an item was deleted). This is used by the videoCache to rethink what to cache next.
  void playlistChanged();

  void saveViewStatesToPlaylist(QDomElement &root);

  // The selected item finished loading the double buffer.
  void selectedItemDoubleBufferLoad(int itemID);

protected:
  // Overload from QWidget to create a custom context menu
  virtual void contextMenuEvent(QContextMenuEvent *event) Q_DECL_OVERRIDE;
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
  // If 'recache' is set, pass this to the video cache.
  void slotItemChanged(bool redraw, recacheIndicator recache);

  // All item's signals signalItemDoubleBufferLoaded are connected here. If the sending item is currently selected,
  // forward this to the playbackController which might me waiting for this.
  void slotItemDoubleBufferLoaded();

private:

  playlistItem* getDropTarget(const QPoint &pos) const;

  bool loadPlaylistFile(const QString &filePath);
  bool loadPlaylistFromByteArray(QByteArray data, QString filePath);
  QString getPlaylistString(QDir dirName);

  // Whether slotSelectionChanged should immediately exit
  bool ignoreSlotSelectionChanged {false};

  // If the playlist is changed and the changes have not been saved yet, this will be true.
  bool isSaved { true };

  // Update all items that have children. This is called when an item is deleted (it might be one from a 
  // compbound item) or something is dropped (might be dropped on a compbound item)
  void updateAllContainterItems();

  // In the QSettings we keep a list of recent files. Add the given file.
  void addFileToRecentFileSetting(const QString &file);

  // Append the new item at the end of the playlist and connect signals/slots
  void appendNewItem(playlistItem *item, bool emitplaylistChanged = true);

  // Clone the selected item as often as the user wants
  void cloneSelectedItem();

  // We have a pointer to the viewStateHandler to load/save the view states to playlist
  QPointer<viewStateHandler> stateHandler;

  void autoSavePlaylist();
  QTimer autosaveTimer;
};

#endif // PLAYLISTTREEWIDGET_H
