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
#include "playlistItemYUVFile.h"
#include "mainwindow.h"
#include <QDebug>
#include <QFileDialog>

#include "de265File.h"


PlaylistTreeWidget::PlaylistTreeWidget(QWidget *parent) : QTreeWidget(parent)
{
  setDragEnabled(true);
  setDropIndicatorShown(true);
  setDragDropMode(QAbstractItemView::InternalMove);
  setSortingEnabled(true);
  p_isSaved = true;
  setContextMenuPolicy(Qt::DefaultContextMenu);

  connect(this, SIGNAL(itemSelectionChanged()), this, SLOT(slotSelectionChanged()));
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

    // Load the files to the playlist
    loadFiles(fileList);
  }
  else
  {
    QTreeWidget::dropEvent(event);
  }

  //// Update the properties panel and the file info group box.
  //// When dragging an item onto another one (for example a video onto a difference)
  //// the currentChanged slot is called but at a time when the item has not been dropped 
  //// yet. 
  //QTreeWidgetItem *item = currentItem();
  //playlistItem *pItem = dynamic_cast<playlistItem*>( item );
  //pItem->showPropertiesWidget();
  //propertiesDockWidget->setWindowTitle( pItem->getPropertiesTitle() );
  //fileInfoGroupBox->setFileInfo( pItem->getInfoTitel(), pItem->getInfoList() );
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

void PlaylistTreeWidget::slotSelectionChanged()
{
  // The selection changed. Get the first and second selection and emit the selectionChanged signal.
  playlistItem *item1 = NULL;
  playlistItem *item2 = NULL;
  QList<QTreeWidgetItem*> items = selectedItems();
  if (items.count() > 0)
    item1 = dynamic_cast<playlistItem*>(items[0]);
  if (items.count() > 1)
    item2 = dynamic_cast<playlistItem*>(items[1]);

  // Show the correct properties panel in the propertiesStack
  if (item1)
  {
    item1->showPropertiesWidget();
    propertiesDockWidget->setWindowTitle( item1->getPropertiesTitle() );
  }
  else
  {
    // Show the widget 0 (empty widget)
    propertiesStack->setCurrentIndex(0);
    propertiesDockWidget->setWindowTitle( "Properties" );
  }

  emit selectionChanged(item1, item2);
}

void PlaylistTreeWidget::keyPressEvent(QKeyEvent *event)
{  
  // Let the parent handle this key press
  QWidget::keyPressEvent(event);
}

void PlaylistTreeWidget::mousePressEvent(QMouseEvent *event)
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

void PlaylistTreeWidget::loadFiles(QStringList files)
{
  //qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "MainWindow::loadFiles()";

  QStringList filter;

  // this might be used to associate a statistics item with a video item
  playlistItem* lastAddedItem = NULL;

  QStringList::Iterator it = files.begin();
  while (it != files.end())
  {
    QString fileName = *it;

    if (!(QFile(fileName).exists()))
    {
      ++it;
      continue;
    }

    QFileInfo fi(fileName);

    if (fi.isDir())
    {
      QDir dir = QDir(*it);
      filter.clear();
      filter << "*.yuv";
      QStringList dirFiles = dir.entryList(filter);

      QStringList::const_iterator dirIt = dirFiles.begin();

      QStringList filePathList;
      while (dirIt != dirFiles.end())
      {
        filePathList.append((*it) + "/" + (*dirIt));

        // next file
        ++dirIt;
      }
    }
    else
    {
      QString ext = fi.suffix();
      ext = ext.toLower();
      
      //if (ext == "hevc")
      //{
      //  // Open an hevc file
      //  PlaylistItem *newListItemVid = new PlaylistItem(PlaylistItem_Video, fileName, p_playlistWidget);
      //  lastAddedItem = newListItemVid;

      //  QSharedPointer<FrameObject> frmObj = newListItemVid->getFrameObject();
      //  QSharedPointer<de265File> dec = qSharedPointerDynamicCast<de265File>(frmObj->getSource());
      //  if (dec->getStatisticsEnabled()) {
      //    // The library supports statistics.
      //    PlaylistItem *newListItemStats = new PlaylistItem(dec, newListItemVid);
      //    // Do not issue unused variable warning.
      //    // This is actually intentional. The new list item goes into the playlist
      //    // and just needs a pointer to the decoder.
      //    (void)newListItemStats;
      //  }

      //  // save as recent
      //  QSettings settings;
      //  QStringList files = settings.value("recentFileList").toStringList();
      //  files.removeAll(fileName);
      //  files.prepend(fileName);
      //  while (files.size() > MaxRecentFiles)
      //    files.removeLast();

      //  settings.setValue("recentFileList", files);
      //  updateRecentFileActions();
      //}
      if (ext == "yuv")
      {
        playlistItemYUVFile *newYUVFile = new playlistItemYUVFile(fileName, propertiesStack);
        insertTopLevelItem(0, newYUVFile);
        lastAddedItem = newYUVFile;

        // save as recent
        addFileToRecentFileSetting( fileName );
      }
      //else if (ext == "csv")
      //{
      //  PlaylistItem *newListItemStats = new PlaylistItem(PlaylistItem_Statistics, fileName, p_playlistWidget);
      //  lastAddedItem = newListItemStats;

      //  // save as recent
      //  QSettings settings;
      //  QStringList files = settings.value("recentFileList").toStringList();
      //  files.removeAll(fileName);
      //  files.prepend(fileName);
      //  while (files.size() > MaxRecentFiles)
      //    files.removeLast();

      //  settings.setValue("recentFileList", files);
      //  updateRecentFileActions();
      //}
      //else if (ext == "yuvplaylist")
      //{
      //  // we found a playlist: cancel here and load playlist as a whole
      //  loadPlaylistFile(fileName);

      //  // save as recent
      //  QSettings settings;
      //  QStringList files = settings.value("recentFileList").toStringList();
      //  files.removeAll(fileName);
      //  files.prepend(fileName);
      //  while (files.size() > MaxRecentFiles)
      //    files.removeLast();

      //  settings.setValue("recentFileList", files);
      //  updateRecentFileActions();
      //  return;
      //}
    }

    // Insert the item into the playlist
    

    ++it;
  }

  // select last added item
  if (lastAddedItem)
    setCurrentItem(lastAddedItem, 0, QItemSelectionModel::ClearAndSelect);
}

void PlaylistTreeWidget::addFileToRecentFileSetting(QString fileName)
{
  QSettings settings;
  QStringList files = settings.value("recentFileList").toStringList();
  files.removeAll(fileName);
  files.prepend(fileName);
  while (files.size() > MAX_RECENT_FILES)
    files.removeLast();

  settings.setValue("recentFileList", files);
}