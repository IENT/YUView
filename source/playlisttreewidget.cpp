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
#include "playlistItemDifference.h"
#include "mainwindow.h"
#include <QDebug>

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

  // Update the properties panel and the file info group box.
  // When dragging an item onto another one (for example a video onto a difference)
  // the currentChanged slot is called but at a time when the item has not been dropped 
  // yet. 
  QTreeWidgetItem *item = currentItem();
  playlistItem *pItem = dynamic_cast<playlistItem*>( item );
  pItem->showPropertiesWidget();
  propertiesDockWidget->setWindowTitle( pItem->getPropertiesTitle() );
  fileInfoGroupBox->setFileInfo( pItem->getInfoTitel(), pItem->getInfoList() );
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

  QAction *deleteAction = NULL;

  QTreeWidgetItem* itemAtPoint = itemAt( event->pos() );
  if (itemAtPoint)
  {
    menu.addSeparator();
    deleteAction = menu.addAction("Delete Item");    
  }

  //QPoint globalPos = viewport()->mapToGlobal(point);
  QAction* action = menu.exec( event->globalPos() );
  if (action == NULL)
    return;

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
    playlistItemDifference *newDiff = new playlistItemDifference( propertiesStack );
    insertTopLevelItem(0, newDiff);
  }
  else if (action == deleteAction)
  {
    deleteSelectedPlaylistItems();
  }
}

void PlaylistTreeWidget::currentChanged(const QModelIndex & current, const QModelIndex & previous)
{
  // Show the correct properties panel in the propertiesStack and update the file info dock widet
  if (current.isValid())
  {
    // Properties panel
    QTreeWidgetItem *item = itemFromIndex( current );
    playlistItem *pItem = dynamic_cast<playlistItem*>( item );
    pItem->showPropertiesWidget();
    propertiesDockWidget->setWindowTitle( pItem->getPropertiesTitle() );

    // File info group box
    fileInfoGroupBox->setFileInfo( pItem->getInfoTitel(), pItem->getInfoList() );
  }
  else
  {
    // Show the widget 0 (empty widget)
    propertiesStack->setCurrentIndex(0);

    // Clear the file info group box
    fileInfoGroupBox->setFileInfo();
  }
}

void PlaylistTreeWidget::keyPressEvent(QKeyEvent *event)
{  
  // Let the parent handle this key press
  QWidget::keyPressEvent(event);
}

// Remove the selected items from the playlist tree widget and delete them
void PlaylistTreeWidget::deleteSelectedPlaylistItems()
{
  QList<QTreeWidgetItem*> items = selectedItems();
  foreach (QTreeWidgetItem *item, items)
  {
    int idx = indexOfTopLevelItem( item );
    takeTopLevelItem( idx );
    delete item;
  }
}
