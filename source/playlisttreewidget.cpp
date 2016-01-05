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

#include "playlisttreewidget.h"
#include <QDragEnterEvent>
#include <QUrl>
#include <QMimeData>
#include "playlistitem.h"
#include "playlistItemText.h"

#include "mainwindow.h"

PlaylistTreeWidget::PlaylistTreeWidget(QWidget *parent) : QTreeWidget(parent)
{
  setDragEnabled(true);
  setDropIndicatorShown(true);
  setDragDropMode(QAbstractItemView::InternalMove);
  setSortingEnabled(true);
  p_isSaved = true;
  setContextMenuPolicy(Qt::DefaultContextMenu);
}

playlistItem* PlaylistTreeWidget::getDropTarget(QPoint pos)
{
  playlistItem *pItem = dynamic_cast<playlistItem*>(this->itemAt(pos));
  if (pItem != NULL)
  {
    // check if dropped on or below/above pItem
    QRect rc = this->visualItemRect(pItem);
    QRect rcNew = QRect(rc.left(), rc.top() + 2, rc.width(), rc.height() - 4);
    if (!rcNew.contains(pos, true))
      // dropped next to pItem
      pItem = NULL;
  }

  return pItem;
}

void PlaylistTreeWidget::dragMoveEvent(QDragMoveEvent* event)
{
  playlistItem* dropTarget = getDropTarget(event->pos());
  if (dropTarget)
  {
    QList<QTreeWidgetItem*> draggedItems = selectedItems();
    playlistItem* draggedItem = dynamic_cast<playlistItem*>(draggedItems[0]);

    // handle video items as target
    if ( !dropTarget->acceptDrops( draggedItem ))
    {
      // no valid drop
      event->ignore();
      return;
    }
  }

  QTreeWidget::dragMoveEvent(event);
}

void PlaylistTreeWidget::dragEnterEvent(QDragEnterEvent *event)
{
  if (event->mimeData()->hasUrls())
  {
    event->acceptProposedAction();
  }
  else    // default behavior
  {
    QTreeWidget::dragEnterEvent(event);
  }
}

void PlaylistTreeWidget::dropEvent(QDropEvent *event)
{
  if (event->mimeData()->hasUrls())
  {
    QStringList fileList;
    QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty())
    {
      QUrl url;
      foreach (url, urls)
      {
        QString fileName = url.toLocalFile();

        fileList.append(fileName);
      }
    }
    event->acceptProposedAction();
    // the playlist has been modified and changes are not saved
    p_isSaved = false;

    // use our main window to open this file
    MainWindow* mainWindow = dynamic_cast<MainWindow*>(this->window());
    mainWindow->loadFiles(fileList);
  }
  else
  {
    QTreeWidget::dropEvent(event);
  }
}

Qt::DropActions PlaylistTreeWidget::supportedDropActions () const
{
  return Qt::CopyAction | Qt::MoveAction;
}

void PlaylistTreeWidget::contextMenuEvent(QContextMenuEvent * event)
{
  QMenu menu;

  // first add generic items to context menu
  QAction *open       = menu.addAction("Open File...");
  QAction *createText = menu.addAction("Add Text Frame");
  QAction *createDiff = menu.addAction("Add Difference Sequence");

  //QTreeWidgetItem* itemAtPoint = p_playlistWidget->itemAt(point);
  //if (itemAtPoint)
  //{
  //  menu.addSeparator();
  //  menu.addAction("Delete Item", this, SLOT(deleteItem()));

  //  PlaylistItem* item = dynamic_cast<PlaylistItem*>(itemAtPoint);

  //  if (item->itemType() == PlaylistItem_Statistics)
  //  {
  //    // TODO: special actions for statistics items
  //  }
  //  if (item->itemType() == PlaylistItem_Video)
  //  {
  //    // TODO: special actions for video items
  //  }
  //  if (item->itemType() == PlaylistItem_Text)
  //  {
  //    menu.addAction("Edit Properties", this, SLOT(editTextFrame()));
  //  }
  //  if (item->itemType() == PlaylistItem_Difference)
  //  {
  //    // TODO: special actions for difference items
  //  }
  //}

  //QPoint globalPos = viewport()->mapToGlobal(point);
  QAction* action = menu.exec( event->globalPos() );
  if (action)
  {
    if (action == open)
    {
      // Show the open file dialog
    }
    else if (action == createText)
    {
      // Create a new playlistItemText
      playlistItemText *newText = new playlistItemText( propertiesStack );
      insertTopLevelItem(0, newText);
    }
    else if (action == createDiff)
    {
      // Create a new playlistItemDifference

    }
  }
}

void PlaylistTreeWidget::currentChanged(const QModelIndex & current, const QModelIndex & previous)
{
  // Show the correct properties panel in the propertiesStack
  
}