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

#include "playlistTreeWidget.h"
#include "playlistItem.h"
#include "playlistItems.h"

#include <QDebug>
#include <QFileDialog>
#include <QDomElement>
#include <QDomDocument>
#include <QBuffer>
#include <QTime>
#include <QDragEnterEvent>
#include <QUrl>
#include <QMimeData>
#include <QMenu>
#include <QSettings>
#include <QMessageBox>

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

    // A drop event occured which was not a file being loaded.
    // Maybe we can find out what was dropped where, but for now we just tell all
    // containter items to check their children and see if they need updating.
    updateAllContainterItems();

    emit playlistChanged();
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

void PlaylistTreeWidget::updateAllContainterItems()
{
  for (int i = 0; i < topLevelItemCount(); i++)
  {
    QTreeWidgetItem *item = topLevelItem(i);
    playlistItem *plItem = dynamic_cast<playlistItem*>(item);
    if (plItem != NULL)
      plItem->updateChildItems();
  }
}

Qt::DropActions PlaylistTreeWidget::supportedDropActions () const
{
  return Qt::CopyAction | Qt::MoveAction;
}

void PlaylistTreeWidget::addTextItem()
{
  // Create a new playlistItemText and add it at the end of the list
  playlistItemText *newText = new playlistItemText();
  appendNewItem(newText);
}

void PlaylistTreeWidget::addDifferenceItem()
{
  // Create a new playlistItemDifference and add it at the end of the list
  playlistItemDifference *newDiff = new playlistItemDifference();

  // Get the currently selected items that canBeUsedInDifference
  QList<QTreeWidgetItem*> selection;
  for (int i = 0; i < selectedItems().count(); i++)
  {
    playlistItem *item = dynamic_cast<playlistItem*>(selectedItems()[i]);
    if (item->canBeUsedInDifference())
      selection.append(selectedItems()[i]);
  }

  // If one or two video items are selected right now, add them as children to the difference
  int nrItems = 2;
  if (selection.count() < 2)
    nrItems = selection.count();
  if (selection.count() > 2)
    nrItems = 0;

  for (int i = 0; i < nrItems; i++)
  {
    QTreeWidgetItem* item = selection[i];

    int index = indexOfTopLevelItem(item);
    if (index != INT_INVALID)
    {
      item = takeTopLevelItem(index);
      newDiff->addChild(item);
      newDiff->setExpanded(true);
    }
  }

  newDiff->updateChildItems();
  appendNewItem(newDiff);
  setCurrentItem(newDiff);
}

void PlaylistTreeWidget::addOverlayItem()
{
  // Create a new playlistItemDifference and add it at the end of the list
  playlistItemOverlay *newOverlay = new playlistItemOverlay();

  // Get the currently selected items
  QList<QTreeWidgetItem*> selection;
  for (int i = 0; i < selectedItems().count(); i++)
  {
    playlistItem *item = dynamic_cast<playlistItem*>(selectedItems()[i]);
    if (item)
      selection.append(selectedItems()[i]);
  }

  // Add all selected items to the overlay
  for (int i = 0; i < selection.count(); i++)
  {
    QTreeWidgetItem* item = selection[i];

    int index = indexOfTopLevelItem(item);
    if (index != INT_INVALID)
    {
      item = takeTopLevelItem(index);
      newOverlay->addChild(item);
      newOverlay->setExpanded(true);
    }
  }

  appendNewItem(newOverlay);
  setCurrentItem(newOverlay);
}

void PlaylistTreeWidget::appendNewItem(playlistItem *item, bool emitplaylistChanged)
{
  insertTopLevelItem(topLevelItemCount(), item);
  connect(item, SIGNAL(signalItemChanged(bool,bool)), this, SLOT(slotItemChanged(bool,bool)));

  // A new item was appended. The playlist changed.
  if (emitplaylistChanged)
    emit playlistChanged();
}

void PlaylistTreeWidget::contextMenuEvent(QContextMenuEvent * event)
{
  QMenu menu;

  // first add generic items to context menu
  QAction *open       = menu.addAction("Open File...");
  QAction *createText = menu.addAction("Add Text Frame");
  QAction *createDiff = menu.addAction("Add Difference Sequence");
  QAction *createOverlay = menu.addAction("Add Overlay");

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
    // Show the open file dialog
    emit openFileDialog();
  else if (action == createText)
    addTextItem();
  else if (action == createDiff)
    addDifferenceItem();
  else if (action == createOverlay)
    addOverlayItem();
  else if (action == deleteAction)
    deleteSelectedPlaylistItems();
}

void PlaylistTreeWidget::getSelectedItems( playlistItem *&item1, playlistItem *&item2 )
{
  QList<QTreeWidgetItem*> items = selectedItems();
  item1 = NULL;
  item2 = NULL;
  if (items.count() > 0)
    item1 = dynamic_cast<playlistItem*>(items[0]);
  if (items.count() > 1)
    item2 = dynamic_cast<playlistItem*>(items[1]);
}

void PlaylistTreeWidget::slotSelectionChanged()
{
  // The selection changed. Get the first and second selection and emit the selectionChanged signal.
  playlistItem *item1, *item2;
  getSelectedItems(item1, item2);
  emit selectionChanged(item1, item2, false);

  // Also notify the cache that a new object was selected
  emit playlistChanged();
}

void PlaylistTreeWidget::slotItemChanged(bool redraw, bool cacheChanged)
{
  // Check if the calling object is (one of) the currently selected item(s)
  playlistItem *item1, *item2;
  getSelectedItems(item1, item2);

  QObject *sender = QObject::sender();
  if (sender == item1 || sender == item2)
  {
    // One of the currently selected items send this signal. Inform the playbackController that something might have changed.
    emit selectedItemChanged(redraw);
  }

  // One of the items changed. This might concern the caching process (the size of the item might have changed.
  // In this case all cached frames are invalid)
  if (cacheChanged)
    emit playlistChanged();
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

bool PlaylistTreeWidget::hasNextItem()
{
  QList<QTreeWidgetItem*> items = selectedItems();
  if (items.count() == 0)
    return false;

  // Get index of current item
  int idx = indexOfTopLevelItem( items[0] );

  // Is there a next item?
  if (idx < topLevelItemCount() - 1)
    return true;

  return false;
}

bool PlaylistTreeWidget::selectNextItem(bool wrapAround, bool callByPlayback)
{
  QList<QTreeWidgetItem*> items = selectedItems();
  if (items.count() == 0)
    return false;

  // Get index of current item
  int idx = indexOfTopLevelItem( items[0] );

  // Is there a next item?
  if (idx == topLevelItemCount() - 1)
  {
    if (wrapAround)
      // The next item is 0
      idx = -1;
    else
      return false;
  }

  if (callByPlayback)
  {
    // Select the next item but emit the selectionChanged event with changedByPlayback=true.
    disconnect(this, SIGNAL(itemSelectionChanged()), NULL, NULL);

    // Select the next item
    setCurrentItem( topLevelItem(idx + 1) );
    
    // Do what the function slotSelectionChanged usually does but this time with changedByPlayback=false.
    playlistItem *item1, *item2;
    getSelectedItems(item1, item2);
    emit selectionChanged(item1, item2, true);
    // Also notify the cache that a new object was selected
    emit playlistChanged();

    connect(this, SIGNAL(itemSelectionChanged()), this, SLOT(slotSelectionChanged()));
  }
  else
    // Set next item as current and emit the selectionChanged event with changedByPlayback=false.
    setCurrentItem( topLevelItem(idx + 1) );

  // Another item was selected. The caching thread also has to be notified about this.
  emit playlistChanged();

  return true;
}

void PlaylistTreeWidget::selectPreviousItem()
{
  QList<QTreeWidgetItem*> items = selectedItems();
  if (items.count() == 0)
    return;

  // Get index of current item
  int idx = indexOfTopLevelItem( items[0] );

  // Is there a previous item?
  if (idx == 0)
    return;

  // Set next item as current
  setCurrentItem( topLevelItem(idx - 1) );

  // Another item was selected. The caching thread also has to be notified about this.
  emit playlistChanged();
}

// Remove the selected items from the playlist tree widget and delete them
void PlaylistTreeWidget::deleteSelectedPlaylistItems()
{
  QList<QTreeWidgetItem*> items = selectedItems();
  if (items.count() == 0)
    return;
  foreach (QTreeWidgetItem *item, items)
  {
    playlistItem *plItem = dynamic_cast<playlistItem*>(item);
    
    // If the item is cachable, abort this and disable all further caching until the item is gone.
    plItem->disableCaching();
        
    // Emit that the item is about to be delete
    emit itemAboutToBeDeleted( plItem );

    int idx = indexOfTopLevelItem( item );
    takeTopLevelItem( idx );

    // If the item is in a container item we have to inform the container that the item will be deleted.
    playlistItem *parentItem = plItem->parentPlaylistItem();
    if (parentItem)
      parentItem->itemAboutToBeDeleter( plItem );
    
    // Delete the item later. This will wait until all events have been processed and then delete the item.
    // This way we don't have to take care about still connected signals/slots. They are automatically
    // disconnected by the QObject.
    plItem->deleteLater();
  }

  // One of the items we deleted might be the child of a containter item. 
  // Update all containter items.
  updateAllContainterItems();

  // Something was deleted. We don't need to emit the playlistChanged signal here again. If an item was deleted,
  // the selection also changes and the signal is automatically emitted.
}

// Remove all items from the playlist tree widget and delete them
void PlaylistTreeWidget::deleteAllPlaylistItems()
{
  if (topLevelItemCount() == 0)
    return;

  for (int i=topLevelItemCount()-1; i>=0; i--)
  {
    playlistItem *plItem = dynamic_cast<playlistItem*>( topLevelItem(i) );

    // If the item is cachable, abort this and disable all further caching until the item is gone.
    plItem->disableCaching();

    // Emit that the item is about to be delete
    emit itemAboutToBeDeleted( plItem );
    takeTopLevelItem( i );

    // Delete the item later. This will wait until all events have been processed and then delete the item.
    // This way we don't have to take care about still connected signals/slots. They are automatically
    // disconnected by the QObject.
    plItem->deleteLater();
  }

  // Something was deleter. The playlist changed.
  emit playlistChanged();
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
      QString ext = fi.suffix().toLower();
      if (ext == "yuvplaylist")
      {
        // Load the playlist
        loadPlaylistFile(fileName);
      }
      else
      {
        // Try to open the file
        playlistItem *newItem = playlistItems::createPlaylistItemFromFile(fileName);
        if (newItem)
        {
          appendNewItem(newItem, false);
          lastAddedItem = newItem;

          // save as recent
          addFileToRecentFileSetting( fileName );
          p_isSaved = false;
        }
      }
    }

    ++it;
  }

  if (lastAddedItem)
  {
    // Something was added. Select the last added item.
    setCurrentItem(lastAddedItem, 0, QItemSelectionModel::ClearAndSelect);

    // The signal playlistChanged mus not be emitted again here because the setCurrentItem(...) function already did
  }
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

void PlaylistTreeWidget::savePlaylistToFile()
{
  // ask user for file location
  QSettings settings;

  QString filename = QFileDialog::getSaveFileName(this, tr("Save Playlist"), settings.value("LastPlaylistPath").toString(), tr("Playlist Files (*.yuvplaylist)"));
  if (filename.isEmpty())
    return;
  if (!filename.endsWith(".yuvplaylist", Qt::CaseInsensitive))
    filename += ".yuvplaylist";

  // remember this directory for next time
  QDir dirName(filename.section('/', 0, -2));
  settings.setValue("LastPlaylistPath", dirName.path());

  // Create the xml document structure
  QDomDocument document;
  document.appendChild(document.createProcessingInstruction(QStringLiteral("xml"), QStringLiteral("version=\"1.0\" encoding=\"UTF-8\"")));
  QDomElement plist = document.createElement(QStringLiteral("playlistItems"));
  plist.setAttribute(QStringLiteral("version"), QStringLiteral("2.0"));
  document.appendChild(plist);

  // Append all the playlist items to the output
  for( int i = 0; i < topLevelItemCount(); ++i )
  {
    QTreeWidgetItem *item = topLevelItem( i );
    playlistItem *plItem = dynamic_cast<playlistItem*>(item);

    plItem->savePlaylist(plist, dirName);
  }

  // Write the xml structure to file
  QFile file(filename);
  file.open(QIODevice::WriteOnly | QIODevice::Text);
  QTextStream outStream(&file);
  outStream << document.toString();
  file.close();

  // We saved the playlist
  p_isSaved = true;
}


void PlaylistTreeWidget::loadPlaylistFile(QString filePath)
{
  if (topLevelItemCount() != 0)
  {
    // Clear playlist first? Ask the user
    QMessageBox msgBox;
    msgBox.setWindowTitle("Load playlist...");
    msgBox.setText("The current playlist is not empty. Do you want to clear the playlist first or append the playlist items to the current playlist?");
    QPushButton *clearPlaylist = msgBox.addButton(tr("Clear Playlist"), QMessageBox::ActionRole);
    msgBox.addButton(tr("Append"), QMessageBox::ActionRole);
    QPushButton *abortButton = msgBox.addButton(QMessageBox::Abort);

    msgBox.exec();

    if (msgBox.clickedButton() == clearPlaylist) 
    {
      // Clear the playlist and continue
      clear();
    } 
    else if (msgBox.clickedButton() == abortButton) 
    {
      // Abort loading
      return;
    }
  }
  
  // parse plist structure of playlist file
  QFile file(filePath);
  QFileInfo fileInfo(file);

  if (!file.open(QIODevice::ReadOnly))
    return;

  // Load the playlist file to buffer
  QByteArray fileBytes = file.readAll();
  QBuffer buffer(&fileBytes);

  // Try to open the dom document
  QDomDocument doc;
  QString errorMessage;
  int errorLine;
  int errorColumn;
  bool success = doc.setContent(&buffer, false, &errorMessage, &errorLine, &errorColumn);
  if (!success)
  {
    qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "PListParser Warning: Could not parse PList file!";
    qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "Error message: " << errorMessage;
    qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "Error line: " << errorLine;
    qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "Error column: " << errorColumn;
    return;
  }

  // Get the root and parser the header
  QDomElement root = doc.documentElement();
  if (root.attribute(QStringLiteral("version"), QStringLiteral("2.0")) != QLatin1String("2.0")) {
    qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "PListParser Warning: plist is using an unknown format version, parsing might fail unexpectedly";
  }

  // Iterate over all items in the playlist
  QDomNode n = root.firstChild();
  while (!n.isNull()) 
  {
    QString tmp = n.toElement().tagName();
    QDomElement elem = n.toElement();
    if (n.isElement())
    {
      playlistItem *newItem = loadPlaylistItem(elem, filePath);
      if (newItem)
        appendNewItem(newItem, false);
    }
    n = n.nextSibling();
  }

  if (topLevelItemCount() != 0 && selectedItems().count() == 0)
  {
    // There are items in the playlist, but no item is currently selected.
    // Select the first item in the playlist.
    setCurrentItem(0);
  }

  // A new item was appended. The playlist changed.
  emit playlistChanged();
}

// Load one playlist item. Load it and return it. This function is seperate so it can be called
// recursively if an item has children.
playlistItem *PlaylistTreeWidget::loadPlaylistItem(QDomElement elem, QString filePath)
{
  playlistItem *newItem = NULL;
  bool parseChildren = false;

  // Parse the item
  if (elem.tagName() == "playlistItemRawFile")
  {
    // This is a playlistItemYUVFile. Create a new one and add it to the playlist
    newItem = playlistItemRawFile::newplaylistItemRawFile(elem, filePath);
  }
  else if (elem.tagName() == "playlistItemHEVCFile")
  {
    // Load the playlistItemHEVCFile
    newItem = playlistItemHEVCFile::newplaylistItemHEVCFile(elem, filePath);
  }
  else if (elem.tagName() == "playlistItemStatisticsFile")
  {
    // Load the playlistItemStatisticsFile
    newItem = playlistItemStatisticsFile::newplaylistItemStatisticsFile(elem, filePath);
  }
  else if (elem.tagName() == "playlistItemText")
  {
    // This is a playlistItemText. Load it from file.
    newItem = playlistItemText::newplaylistItemText(elem);
  }
  else if (elem.tagName() == "playlistItemDifference")
  {
    // This is a playlistItemDifference. Load it from file.
    newItem = playlistItemDifference::newPlaylistItemDifference(elem);
    parseChildren = true;
  }
  else if (elem.tagName() == "playlistItemOverlay")
  {
    // This is a playlistItemOverlay. Load it from file.
    newItem = playlistItemOverlay::newPlaylistItemOverlay(elem, filePath);
    parseChildren = true;
  }
  else if (elem.tagName() == "playlistItemImageFile")
  {
    // This is a playlistItemImageFile. Load it.
    newItem = playlistItemImageFile::newplaylistItemImageFile(elem, filePath);
  }

  if (newItem != NULL && parseChildren)
  {
    // The playlistItem can have children. Parse them.
    QDomNodeList children = elem.childNodes();
  
    for (int i = 0; i < children.length(); i++)
    {
      // Parse the child items
      QDomElement childElem = children.item(i).toElement();
      playlistItem *childItem = loadPlaylistItem(childElem, filePath);

      if (childItem)
        newItem->addChild(childItem);
    }

    newItem->updateChildItems();
  }

  return newItem;
}