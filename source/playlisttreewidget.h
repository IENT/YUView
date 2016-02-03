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

#include <QTreeWidget>
#include <QDockWidget>
#include "QMouseEvent"

#include "playlistitem.h"

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
  bool getIsSaved() { return p_isSaved;}

  // load the given files into the playlist
  void loadFiles(QStringList files);
  
  // Remove the selected items from the playlist tree widget and delete them
  void deleteSelectedPlaylistItems();

  Qt::DropActions supportedDropActions() const;

  QModelIndex indexForItem(playlistItem * item) { return indexFromItem((QTreeWidgetItem*)item); }

  // Get the first two selected items
  void getSelectedItems( playlistItem *&first, playlistItem *&second );

public slots:
  void savePlaylistToFile();

  // Slots for going to the next/previous item
  void selectNextItem();
  void selectPreviousItem();

  // Slots for adding text/difference items
  void addTextItem();
  void addDifferenceItem();

signals:
  // The user requests to show the open filel dialog
  void openFileDialog();

  // The current selection changed. Give the new first (and second) selection.
  void selectionChanged( playlistItem *first, playlistItem *second );

  // The properties of (one of) the currently selected items changed.
  void selectionPropertiesChanged();

  // The item is about to be deleted. Last chance to do something with it.
  void itemAboutToBeDeleted( playlistItem *item );

protected:
  // Overload from QWidget to create a custom context menu
  virtual void contextMenuEvent(QContextMenuEvent * event);
  // Overload from QWidget to capture file drops onto the playlist
  virtual void dropEvent(QDropEvent *event);
  // Overload from QWidget to determine if we can accept this item for dropping
  virtual void dragEnterEvent(QDragEnterEvent *event);
  // Overload from QWidget to set if the item being dragged can be dropped onto the item under the cursor
  void dragMoveEvent(QDragMoveEvent* event);
  // Overload from QWidget to ...
  virtual void mousePressEvent(QMouseEvent *event);

protected slots:

  // Connect the signal QTreeWidget::itemSelectionChanged() here
  // Note: Previously tried to overload QAbstractItemView::currentChanged(...). Did not work becuase when currentChanged
  // is called, selectedItems does not return the correct selection list. When slotSelectionChanged is called, the list
  // of selected items is correct.
  void slotSelectionChanged();

  // The signals of all playlistItem::signalRedrawItem() are connected here.
  void slotItemPropertiesChanged();

private:
  
  // 
  playlistItem* getDropTarget(QPoint pos);

  void loadPlaylistFile(QString filePath);

  // 
  bool p_isSaved;

  // In the QSettings we keep a list of recent files. Add the given file.
  void addFileToRecentFileSetting(QString file);

  // Append the new item at the end of the playlist and connect signals/slots
  void appendNewItem(playlistItem *item);
};

#endif // PLAYLISTTREEWIDGET_H
