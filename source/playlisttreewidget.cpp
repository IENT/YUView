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
#include <QDomElement>
#include <QDomDocument>
#include <QBuffer>
#include <QTime>

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
    playlistItemText *newText = new playlistItemText();
    insertTopLevelItem(topLevelItemCount(), newText);
  }
  else if (action == createDiff)
  {
    // Create a new playlistItemDifference
    playlistItemDifference *newDiff = new playlistItemDifference();
    insertTopLevelItem(topLevelItemCount(), newDiff);
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

  //// Show the correct properties panel in the propertiesStack
  //if (item1)
  //{
  //  item1->showPropertiesWidget();
  //  propertiesDockWidget->setWindowTitle( item1->getPropertiesTitle() );
  //}
  //else
  //{
  //  // Show the widget 0 (empty widget)
  //  propertiesStack->setCurrentIndex(0);
  //  propertiesDockWidget->setWindowTitle( "Properties" );
  //}

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
    playlistItem *plItem = dynamic_cast<playlistItem*>(item);
    emit itemAboutToBeDeleted( plItem );

    int idx = indexOfTopLevelItem( item );
    takeTopLevelItem( idx );
    
    // Delete the item later. This will wait until all events have been processed and then delete the item.
    // This way we don't have to take care about still connected signals/slots. They are automatically
    // disconnected by the QObject.
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
        insertTopLevelItem(topLevelItemCount(), newYUVFile);
        lastAddedItem = newYUVFile;

        // save as recent
        addFileToRecentFileSetting( fileName );
        p_isSaved = false;
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
      else if (ext == "yuvplaylist")
      {
        // Load the playlist
        loadPlaylistFile(fileName);

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
      }
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
  QDomDocument document(QStringLiteral("plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\""));
  document.appendChild(document.createProcessingInstruction(QStringLiteral("xml"), QStringLiteral("version=\"1.0\" encoding=\"UTF-8\"")));
  QDomElement plist = document.createElement(QStringLiteral("plist"));
  plist.setAttribute(QStringLiteral("version"), QStringLiteral("1.0"));
  document.appendChild(plist);

  QDomElement dictElement = document.createElement(QStringLiteral("dict"));
  plist.appendChild(dictElement);
  
  QDomElement modulesKey = document.createElement("key");
  modulesKey.appendChild(document.createTextNode("Modules"));
  dictElement.appendChild(modulesKey);

  QDomElement rootArray = document.createElement("array");
  dictElement.appendChild(rootArray);

  // Append all the playlist items to the output
  for( int i = 0; i < topLevelItemCount(); ++i )
  {
    QTreeWidgetItem *item = topLevelItem( i );
    playlistItem *plItem = dynamic_cast<playlistItem*>(item);

    QDomElement itemDict = document.createElement("dict");
    rootArray.appendChild(itemDict);

    plItem->savePlaylist(document, itemDict, dirName);
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
  if (root.attribute(QStringLiteral("version"), QStringLiteral("1.0")) != QLatin1String("1.0")) {
    qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "PListParser Warning: plist is using an unknown format version, parsing might fail unexpectedly";
  }

  QDomElement rootDict = root.firstChild().toElement();
  if (rootDict.tagName() != QLatin1String("dict"))
  {
    qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "Error parsing playlist file.";
    qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "No root <dict> element found.";
    return;
  }

  QDomElement modulesKey = rootDict.firstChild().toElement();
  if (modulesKey.tagName() != QLatin1String("key") || modulesKey.text() != "Modules")
  {
    qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "Error parsing playlist file.";
    qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "No <key>Modules</key> element found.";
    return;
  }
  
  QDomElement arrayElement = modulesKey.nextSibling().toElement();
  if (arrayElement.tagName() != QLatin1String("array"))
  {
    qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "Error parsing playlist file.";
    qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "No Modules array found.";
    return;
  }

  // Iterate over all items in the playlist
  QDomNode n = arrayElement.firstChild();
  while (!n.isNull()) {
    if (n.isElement()) {
      QDomElement entryDict = n.toElement();
      if (entryDict.tagName() != QLatin1String("dict"))
      {
        qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "Error parsing playlist file.";
        qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "Item entry did not start with a <dict>";
        n = n.nextSibling();
        continue;
      }

      QDomElement classKey = entryDict.firstChild().toElement();
      if (classKey.tagName() != QLatin1String("key") || classKey.text() != "Class")
      {
        qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "Error parsing playlist file.";
        qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "Item entry did not start with a <key>Class</key>";
        n = n.nextSibling();
        continue;
      }

      QDomElement typeString = classKey.nextSiblingElement();
      if (typeString.tagName() != QLatin1String("string"))
      {
        qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "Error parsing playlist file.";
        qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "Item entry did not start with a <string> definition of a type";
        n = n.nextSibling();
        continue;
      }

      if (typeString.text() == "YUVFile")
      {
        playlistItemYUVFile *newYUVFile = playlistItemYUVFile::newplaylistItemYUVFile(typeString, filePath);
        if (newYUVFile)
          insertTopLevelItem(topLevelItemCount() , newYUVFile);
      }
      else if (typeString.text() == "TextFrameProvider")
      {
        playlistItemText *newText = playlistItemText::newplaylistItemText(typeString);
        if (newText)
          insertTopLevelItem(topLevelItemCount(), newText);
      }

    }
    n = n.nextSibling();
  }

  //QVariantList itemList = map["Modules"].toList();
  //for (int i = 0; i < itemList.count(); i++)
  //{
  //  QVariantMap itemInfo = itemList[i].toMap();
  //  QVariantMap itemProps = itemInfo["Properties"].toMap();

  //  if (itemInfo["Class"].toString() == "TextFrameProvider")
  //  {
  //    float duration = itemProps["duration"].toFloat();
  //    QString fontName = itemProps["fontName"].toString();
  //    int fontSize = itemProps["fontSize"].toInt();
  //    QColor color = QColor(itemProps["fontColor"].toString());
  //    QFont font(fontName, fontSize);
  //    QString text = itemProps["text"].toString();

  //    // create text item and set properties
  //    PlaylistItem *newPlayListItemText = new PlaylistItem(PlaylistItem_Text, text, p_playlistWidget);
  //    QSharedPointer<TextObject> newText = newPlayListItemText->getTextObject();
  //    newText->setFont(font);
  //    newText->setDuration(duration);
  //    newText->setColor(color);
  //  }
  //  else if (itemInfo["Class"].toString() == "YUVFile")
  //  {
  //    QString fileURL = itemProps["URL"].toString();
  //    QString filePath = QUrl(fileURL).path();

  //    QString relativeURL = itemProps["rURL"].toString();
  //    QFileInfo checkAbsoluteFile(filePath);
  //    // check if file with absolute path exists, otherwise check relative path
  //    if (!checkAbsoluteFile.exists())
  //    {
  //      QString combinePath = QDir(fileInfo.path()).filePath(relativeURL);
  //      QFileInfo checkRelativeFile(combinePath);
  //      if (checkRelativeFile.exists() && checkRelativeFile.isFile())
  //      {
  //        filePath = QDir::cleanPath(combinePath);
  //      }
  //      else
  //      {
  //        QMessageBox msgBox;
  //        msgBox.setTextFormat(Qt::RichText);
  //        msgBox.setText("File not found <br> Absolute Path: " + fileURL + "<br> Relative Path: " + combinePath);
  //        msgBox.exec();
  //        break;
  //      }
  //    }

  //    int endFrame = itemProps["endFrame"].toInt();
  //    int frameOffset = itemProps["frameOffset"].toInt();
  //    int frameSampling = itemProps["frameSampling"].toInt();
  //    float frameRate = itemProps["framerate"].toFloat();
  //    YUVCPixelFormatType pixelFormat = (YUVCPixelFormatType)itemProps["pixelFormat"].toInt();
  //    int height = itemProps["height"].toInt();
  //    int width = itemProps["width"].toInt();


  //    // create video item and set properties
  //    PlaylistItem *newPlayListItemVideo = new PlaylistItem(PlaylistItem_Video, filePath, p_playlistWidget);
  //    QSharedPointer<FrameObject> newVideo = newPlayListItemVideo->getFrameObject();
  //    newVideo->setSize(width, height);
  //    newVideo->setSrcPixelFormat(pixelFormat);
  //    newVideo->setFrameRate(frameRate);
  //    newVideo->setSampling(frameSampling);
  //    newVideo->setStartFrame(frameOffset);
  //    newVideo->setEndFrame(endFrame);

  //    // load potentially associated statistics file
  //    if (itemProps.contains("statistics"))
  //    {
  //      QVariantMap itemInfoAssoc = itemProps["statistics"].toMap();
  //      QVariantMap itemPropsAssoc = itemInfoAssoc["Properties"].toMap();

  //      QString fileURL = itemProps["URL"].toString();
  //      QString filePath = QUrl(fileURL).path();

  //      QString relativeURL = itemProps["rURL"].toString();
  //      QFileInfo checkAbsoluteFile(filePath);
  //      // check if file with absolute path exists, otherwise check relative path
  //      if (!checkAbsoluteFile.exists())
  //      {
  //        QString combinePath = QDir(fileInfo.path()).filePath(relativeURL);
  //        QFileInfo checkRelativeFile(combinePath);
  //        if (checkRelativeFile.exists() && checkRelativeFile.isFile())
  //        {
  //          filePath = QDir::cleanPath(combinePath);
  //        }
  //        else
  //        {
  //          QMessageBox msgBox;
  //          msgBox.setTextFormat(Qt::RichText);
  //          msgBox.setText("File not found <br> Absolute Path: " + fileURL + "<br> Relative Path: " + combinePath);
  //          msgBox.exec();
  //          break;
  //        }
  //      }

  //      int endFrame = itemPropsAssoc["endFrame"].toInt();
  //      int frameOffset = itemPropsAssoc["frameOffset"].toInt();
  //      int frameSampling = itemPropsAssoc["frameSampling"].toInt();
  //      float frameRate = itemPropsAssoc["framerate"].toFloat();
  //      int height = itemPropsAssoc["height"].toInt();
  //      int width = itemPropsAssoc["width"].toInt();
  //      QVariantList activeStatsTypeList = itemPropsAssoc["typesChecked"].toList();

  //      // create associated statistics item and set properties
  //      PlaylistItem *newPlayListItemStat = new PlaylistItem(PlaylistItem_Statistics, filePath, newPlayListItemVideo);
  //      QSharedPointer<StatisticsObject> newStat = newPlayListItemStat->getStatisticsObject();
  //      newStat->setSize(width, height);
  //      newStat->setFrameRate(frameRate);
  //      newStat->setSampling(frameSampling);
  //      newStat->setStartFrame(frameOffset);
  //      newStat->setEndFrame(endFrame);

  //      // set active statistics
  //      StatisticsTypeList statsTypeList;

  //      for (int i = 0; i < activeStatsTypeList.count(); i++)
  //      {
  //        QVariantMap statsTypeParams = activeStatsTypeList[i].toMap();

  //        StatisticsType aType;
  //        aType.typeID = statsTypeParams["typeID"].toInt();
  //        aType.typeName = statsTypeParams["typeName"].toString();
  //        aType.render = true;
  //        aType.renderGrid = statsTypeParams["drawGrid"].toBool();
  //        aType.alphaFactor = statsTypeParams["alpha"].toInt();

  //        statsTypeList.append(aType);
  //      }

  //      if (statsTypeList.count() > 0)
  //        newStat->setStatisticsTypeList(statsTypeList);
  //    }
  //  }
  //  else if (itemInfo["Class"].toString() == "StatisticsFile")
  //  {
  //    QString fileURL = itemProps["URL"].toString();
  //    QString filePath = QUrl(fileURL).path();

  //    QString relativeURL = itemProps["rURL"].toString();
  //    QFileInfo checkAbsoluteFile(filePath);
  //    // check if file with absolute path exists, otherwise check relative path
  //    if (!checkAbsoluteFile.exists())
  //    {
  //      QString combinePath = QDir(fileInfo.path()).filePath(relativeURL);
  //      QFileInfo checkRelativeFile(combinePath);
  //      if (checkRelativeFile.exists() && checkRelativeFile.isFile())
  //      {
  //        filePath = QDir::cleanPath(combinePath);
  //      }
  //      else
  //      {
  //        QMessageBox msgBox;
  //        msgBox.setTextFormat(Qt::RichText);
  //        msgBox.setText("File not found <br> Absolute Path: " + fileURL + "<br> Relative Path: " + combinePath);
  //        msgBox.exec();
  //        break;
  //      }
  //    }

  //    int endFrame = itemProps["endFrame"].toInt();
  //    int frameOffset = itemProps["frameOffset"].toInt();
  //    int frameSampling = itemProps["frameSampling"].toInt();
  //    float frameRate = itemProps["framerate"].toFloat();
  //    int height = itemProps["height"].toInt();
  //    int width = itemProps["width"].toInt();
  //    QVariantList activeStatsTypeList = itemProps["typesChecked"].toList();

  //    // create statistics item and set properties
  //    PlaylistItem *newPlayListItemStat = new PlaylistItem(PlaylistItem_Statistics, filePath, p_playlistWidget);
  //    QSharedPointer<StatisticsObject> newStat = newPlayListItemStat->getStatisticsObject();
  //    newStat->setSize(width, height);
  //    newStat->setFrameRate(frameRate);
  //    newStat->setSampling(frameSampling);
  //    newStat->setStartFrame(frameOffset);
  //    newStat->setEndFrame(endFrame);

  //    // set active statistics
  //    StatisticsTypeList statsTypeList;

  //    for (int i = 0; i < activeStatsTypeList.count(); i++)
  //    {
  //      QVariantMap statsTypeParams = activeStatsTypeList[i].toMap();

  //      StatisticsType aType;
  //      aType.typeID = statsTypeParams["typeID"].toInt();
  //      aType.typeName = statsTypeParams["typeName"].toString();
  //      aType.render = true;
  //      aType.renderGrid = statsTypeParams["drawGrid"].toBool();
  //      aType.alphaFactor = statsTypeParams["alpha"].toInt();

  //      statsTypeList.append(aType);
  //    }

  //    if (statsTypeList.count() > 0)
  //      newStat->setStatisticsTypeList(statsTypeList);
  //  }
  //}

  //if (p_playlistWidget->topLevelItemCount() > 0)
  //{
  //  p_playlistWidget->setCurrentItem(p_playlistWidget->topLevelItem(0), 0, QItemSelectionModel::ClearAndSelect);
  //  // this is the first playlist that was loaded, therefore no saving needed
  //  p_playlistWidget->setIsSaved(true);
  //}
}