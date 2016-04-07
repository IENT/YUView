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
#include <QDragEnterEvent>
#include <QUrl>
#include <QMimeData>
#include "playlistItem.h"
#include "playlistItemText.h"
#include "playlistItemDifference.h"
#include "playlistItemOverlay.h"
#include "playlistItemYUVFile.h"
#include "playlistItemStatisticsFile.h"
#include "playlistItemHEVCFile.h"
#include "mainwindow.h"
#include <QDebug>
#include <QFileDialog>
#include <QDomElement>
#include <QDomDocument>
#include <QBuffer>
#include <QTime>

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
    // difference items to check their children and see if they need updating.
    updateAllDifferenceItems();
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

void PlaylistTreeWidget::updateAllDifferenceItems()
{
  for (int i = 0; i < topLevelItemCount(); i++)
  {
    QTreeWidgetItem *item = topLevelItem(i);
    playlistItemDifference *diffItem = dynamic_cast<playlistItemDifference*>(item);
    if (diffItem != NULL)
      diffItem->updateChildren();
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

  newDiff->updateChildren();
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

void PlaylistTreeWidget::appendNewItem(playlistItem *item)
{
  insertTopLevelItem(topLevelItemCount(), item);
  connect(item, SIGNAL(signalItemChanged(bool)), this, SLOT(slotItemChanged(bool)));
}

void PlaylistTreeWidget::receiveCachingCurrentSelection(indexRange range)
{
  // the controller requested a buffering, this could of course also be done from the controller itself
  // TODO: maybe move the code to the controller to save some signal/slot dealing
  playlistItem *item1, *item2;
  getSelectedItems(item1,item2);
  connect(this,SIGNAL(startCachingCurrentSelection(indexRange)),item1,SLOT(startCaching(indexRange)));
  emit startCachingCurrentSelection(range);
  disconnect(this,SIGNAL(startCachingCurrentSelection(indexRange)),NULL,NULL);
}

void PlaylistTreeWidget::receiveRemoveFromCacheCurrentSelection(indexRange range)
{
  playlistItem *item1, *item2;
  getSelectedItems(item1,item2);
  connect(this,SIGNAL(removeFromCacheCurrentSelection(indexRange)),item1,SLOT(removeFromCache(indexRange)));
  emit removeFromCacheCurrentSelection(range);
  disconnect(this,SIGNAL(removeFromCacheCurrentSelection(indexRange)),NULL,NULL);

  if (item2)
    {
      connect(this,SIGNAL(removeFromCacheCurrentSelection(indexRange)),item2,SLOT(removeFromCache(indexRange)));
      emit removeFromCacheCurrentSelection(range);
      disconnect(this,SIGNAL(removeFromCacheCurrentSelection(indexRange)),NULL,NULL);
    }
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
  emit selectionChanged(item1, item2);
}

void PlaylistTreeWidget::slotItemChanged(bool redraw)
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

void PlaylistTreeWidget::selectNextItem()
{
  QList<QTreeWidgetItem*> items = selectedItems();
  if (items.count() == 0)
    return;

  // Get index of current item
  int idx = indexOfTopLevelItem( items[0] );

  // Is there a next item?
  if (idx == topLevelItemCount() - 1)
    return;

  // Set next item as current
  setCurrentItem( topLevelItem(idx + 1) );
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
}

// Remove the selected items from the playlist tree widget and delete them
void PlaylistTreeWidget::deleteSelectedPlaylistItems()
{
  QList<QTreeWidgetItem*> items = selectedItems();
  foreach (QTreeWidgetItem *item, items)
  {
    playlistItem *plItem = dynamic_cast<playlistItem*>(item);
    emit itemAboutToBeDeleted( plItem );

    int idx = indexOfTopLevelItem( item );
    takeTopLevelItem( idx );

    // Delete the item later. This will wait until all events have been processed and then delete the item.
    // This way we don't have to take care about still connected signals/slots. They are automatically
    // disconnected by the QObject.
    if (plItem->isCaching())
      plItem->stopCaching();

    plItem->deleteLater();
  }

  // One of the items we deleted might be the child of a difference item. 
  // Update all difference items.
  updateAllDifferenceItems();
}

// Remove all items from the playlist tree widget and delete them
void PlaylistTreeWidget::deleteAllPlaylistItems()
{
  for (int i=topLevelItemCount()-1; i>=0; i--)
  {
    playlistItem *plItem = dynamic_cast<playlistItem*>( topLevelItem(i) );

    emit itemAboutToBeDeleted( plItem );
    takeTopLevelItem( i );

    // Delete the item later. This will wait until all events have been processed and then delete the item.
    // This way we don't have to take care about still connected signals/slots. They are automatically
    // disconnected by the QObject.
    if (plItem->isCaching())
      plItem->stopCaching();

    plItem->deleteLater();
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
        playlistItemYUVFile *newYUVFile = new playlistItemYUVFile(fileName);
        appendNewItem(newYUVFile);
        lastAddedItem = newYUVFile;

        // save as recent
        addFileToRecentFileSetting( fileName );
        p_isSaved = false;
      }
      else if (ext == "csv")
      {
        playlistItemStatisticsFile *newStatisticsFile = new playlistItemStatisticsFile(fileName);
        appendNewItem(newStatisticsFile);
        lastAddedItem = newStatisticsFile;

        // save as recent
        addFileToRecentFileSetting( fileName );
        p_isSaved = false;
      }
      else if (ext == "hevc")
      {
        playlistItemHEVCFile *newHEVCFile = new playlistItemHEVCFile(fileName);
        appendNewItem(newHEVCFile);
        lastAddedItem = newHEVCFile;

        // save as recent
        addFileToRecentFileSetting( fileName );
        p_isSaved = false;
      }
      else if (ext == "yuvplaylist")
      {
        // Load the playlist
        loadPlaylistFile(fileName);
        
        // Do not save as recent. Or should this also be saved as recent?
      }
    }

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

  //for (int i = 0; i < p_playlistWidget->topLevelItemCount(); i++)
  //{
  //  PlaylistItem* anItem = dynamic_cast<PlaylistItem*>(p_playlistWidget->topLevelItem(i));

  //  QVariantMap itemInfo;
  //  QVariantMap itemProps;

  //  if (anItem->itemType() == PlaylistItem_Video)
  //  {
  //    QSharedPointer<FrameObject> vidItem = anItem->getFrameObject();

  //    itemInfo["Class"] = "YUVFile";

  //    QUrl fileURL(vidItem->path());
  //    fileURL.setScheme("file");
  //    QString relativePath = dirName.relativeFilePath(vidItem->path());
  //    itemProps["URL"] = fileURL.toString();
  //    itemProps["rURL"] = relativePath;
  //    itemProps["endFrame"] = vidItem->endFrame();
  //    itemProps["frameOffset"] = vidItem->startFrame();
  //    itemProps["frameSampling"] = vidItem->sampling();
  //    itemProps["framerate"] = vidItem->frameRate();
  //    itemProps["pixelFormat"] = vidItem->pixelFormat();
  //    itemProps["height"] = vidItem->height();
  //    itemProps["width"] = vidItem->width();

  //    // store potentially associated statistics file
  //    if (anItem->childCount() == 1)
  //    {
  //      QSharedPointer<StatisticsObject> statsItem = anItem->getStatisticsObject();

  //      Q_ASSERT(statsItem);

  //      QVariantMap itemInfoAssoc;
  //      itemInfoAssoc["Class"] = "AssociatedStatisticsFile";

  //      QVariantMap itemPropsAssoc;
  //      QUrl fileURL(statsItem->path());
  //      QString relativePath = dirName.relativeFilePath(statsItem->path());

  //      fileURL.setScheme("file");
  //      itemPropsAssoc["URL"] = fileURL.toString();
  //      itemProps["rURL"] = relativePath;
  //      itemPropsAssoc["endFrame"] = statsItem->endFrame();
  //      itemPropsAssoc["frameOffset"] = statsItem->startFrame();
  //      itemPropsAssoc["frameSampling"] = statsItem->sampling();
  //      itemPropsAssoc["framerate"] = statsItem->frameRate();
  //      itemPropsAssoc["height"] = statsItem->height();
  //      itemPropsAssoc["width"] = statsItem->width();

  //      // save active statistics types
  //      StatisticsTypeList statsTypeList = statsItem->getStatisticsTypeList();

  //      QVariantList activeStatsTypeList;
  //      Q_FOREACH(StatisticsType aType, statsTypeList)
  //      {
  //        if (aType.render)
  //        {
  //          QVariantMap statsTypeParams;

  //          statsTypeParams["typeID"] = aType.typeID;
  //          statsTypeParams["typeName"] = aType.typeName;
  //          statsTypeParams["drawGrid"] = aType.renderGrid;
  //          statsTypeParams["alpha"] = aType.alphaFactor;

  //          activeStatsTypeList.append(statsTypeParams);
  //        }
  //      }

  //      if (activeStatsTypeList.count() > 0)
  //        itemPropsAssoc["typesChecked"] = activeStatsTypeList;

  //      itemInfoAssoc["Properties"] = itemPropsAssoc;

  //      // link to video item
  //      itemProps["statistics"] = itemInfoAssoc;
  //    }
  //  }
  //  else if (anItem->itemType() == PlaylistItem_Text)
  //  {
  //    QSharedPointer<TextObject> textItem = anItem->getTextObject();

  //    itemInfo["Class"] = "TextFrameProvider";

  //    itemProps["duration"] = textItem->duration();
  //    itemProps["fontSize"] = textItem->font().pointSize();
  //    itemProps["fontName"] = textItem->font().family();
  //    itemProps["fontColor"] = textItem->color().name();
  //    itemProps["text"] = textItem->text();
  //  }
  //  else if (anItem->itemType() == PlaylistItem_Statistics)
  //  {
  //    QSharedPointer<StatisticsObject> statsItem = anItem->getStatisticsObject();

  //    itemInfo["Class"] = "StatisticsFile";

  //    QUrl fileURL(statsItem->path());
  //    QString relativePath = dirName.relativeFilePath(statsItem->path());

  //    fileURL.setScheme("file");
  //    itemProps["URL"] = fileURL.toString();
  //    itemProps["rURL"] = relativePath;
  //    itemProps["endFrame"] = statsItem->endFrame();
  //    itemProps["frameOffset"] = statsItem->startFrame();
  //    itemProps["frameSampling"] = statsItem->sampling();
  //    itemProps["framerate"] = statsItem->frameRate();
  //    itemProps["height"] = statsItem->height();
  //    itemProps["width"] = statsItem->width();

  //    // save active statistics types
  //    StatisticsTypeList statsTypeList = statsItem->getStatisticsTypeList();

  //    QVariantList activeStatsTypeList;
  //    Q_FOREACH(StatisticsType aType, statsTypeList)
  //    {
  //      if (aType.render)
  //      {
  //        QVariantMap statsTypeParams;

  //        statsTypeParams["typeID"] = aType.typeID;
  //        statsTypeParams["typeName"] = aType.typeName;
  //        statsTypeParams["drawGrid"] = aType.renderGrid;
  //        statsTypeParams["alpha"] = aType.alphaFactor;

  //        activeStatsTypeList.append(statsTypeParams);
  //      }
  //    }

  //    if (activeStatsTypeList.count() > 0)
  //      itemProps["typesChecked"] = activeStatsTypeList;
  //  }
  //  else
  //  {
  //    continue;
  //  }

  //  itemInfo["Properties"] = itemProps;

  //  itemList.append(itemInfo);
  //}

  //// generate plist from item list
  //QVariantMap plistMap;
  //plistMap["Modules"] = itemList;

  //QString plistFileContents = PListSerializer::toPList(plistMap);

  //QFile file(filename);
  //file.open(QIODevice::WriteOnly | QIODevice::Text);
  //QTextStream outStream(&file);
  //outStream << plistFileContents;
  //file.close();
  //p_playlistWidget->setIsSaved(true);
}


void PlaylistTreeWidget::loadPlaylistFile(QString filePath)
{
  //// clear playlist first
  //p_playlistWidget->clear();

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
  while (!n.isNull()) {
    QString tmp = n.toElement().tagName();
    QDomElement elem = n.toElement();
    if (n.isElement()) 
    {
      // Parse the item
      if (elem.tagName() == "playlistItemYUVFile")
      {
        // This is a playlistItemYUVFile. Create a new one and add it to the playlist
        playlistItemYUVFile *newYUVFile = playlistItemYUVFile::newplaylistItemYUVFile(elem, filePath);
        if (newYUVFile)
          appendNewItem(newYUVFile);
      }
      else if (elem.tagName() == "playlistItemText")
      {
        // This is a playlistItemText. Load it from file.
        playlistItemText *newText = playlistItemText::newplaylistItemText(elem);
        if (newText)
          appendNewItem(newText);
      }
      else if (elem.tagName() == "playlistItemDifference")
      {
        // This is a playlistItemDifference. Load it from file.
        playlistItemDifference *newDiff = playlistItemDifference::newPlaylistItemDifference(elem, filePath);
        if (newDiff)
          appendNewItem(newDiff);
      }
    }
    n = n.nextSibling();
  }
}
