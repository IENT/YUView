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

  void dragMoveEvent(QDragMoveEvent* event);
  void dragEnterEvent(QDragEnterEvent *event);
  void dropEvent(QDropEvent *event);
  
  // Are there changes in the playlist that haven't been saved yet?
  bool getIsSaved() { return p_isSaved;}

  // Load the given file
  void loadFile(QString file);

  void setPropertiesStack(QStackedWidget *stack) { propertiesStack = stack; }
  void setPropertiesDockWidget(QDockWidget *widget) { propertiesDockWidget = widget; }

  Qt::DropActions supportedDropActions() const;

  QModelIndex indexForItem(playlistItem * item) { return indexFromItem((QTreeWidgetItem*)item); }

public slots:

protected:
  // Overload from QWidget to create a custom context menu
  virtual void contextMenuEvent(QContextMenuEvent * event);
  // Overload from QWidget to capture key presses
  virtual void keyPressEvent(QKeyEvent *event);

protected slots:
  // Overload from QAbstractItemView. Called if a new item is selected.
  void currentChanged(const QModelIndex & current, const QModelIndex & previous);

private:
  QStackedWidget *propertiesStack;
  QDockWidget    *propertiesDockWidget;

  playlistItem* getDropTarget(QPoint pos);

  virtual void mousePressEvent(QMouseEvent *event)
  {
    QModelIndex item = indexAt(event->pos());
    QTreeView::mousePressEvent(event);
    if (item.row() == -1 && item.column() == -1)
    {
      clearSelection();
      const QModelIndex index;
      emit currentItemChanged(NULL, NULL);
      //selectionModel()->setCurrentIndex(index, QItemSelectionModel::Select);
    }
  }
  bool p_isSaved;
  
  // Remove the selected items from the playlist tree widget and delete them
  void PlaylistTreeWidget::deleteSelectedPlaylistItems();
};

#endif // PLAYLISTTREEWIDGET_H
