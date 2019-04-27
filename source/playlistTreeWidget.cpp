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

#include "playlistTreeWidget.h"

#include <QBuffer>
#include <QFileDialog>
#include <QHeaderView>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QScopedValueRollback>
#include <QSettings>
#include <QHeaderView>
#include "playlistItems.h"

// Activate this if you want to know when which signals/slots are handled
#define PLAYLISTTREEWIDGET_DEBUG_EVENTS 0
#if PLAYLISTTREEWIDGET_DEBUG_EVENTS && !NDEBUG
#define DEBUG_TREE_WIDGET qDebug
#else
#define DEBUG_TREE_WIDGET(fmt,...) ((void)0)
#endif

class bufferStatusWidget : public QWidget
{
public:
  bufferStatusWidget(playlistItem *item, QWidget* parent) : QWidget(parent) { plItem = item; setMinimumWidth(50); }
  virtual void paintEvent(QPaintEvent *event) Q_DECL_OVERRIDE
  {
    Q_UNUSED(event);

    // Draw
    QPainter painter(this);
    QSize s = size();

    if (!plItem->isCachable())
    {
      // Only draw the border
      painter.drawRect(0, 0, s.width()-1, s.height()-1);
      return;
    }

    // Draw the cached frames
    QList<int> frameList = plItem->getCachedFrames();
    indexRange range = plItem->getFrameIdxRange();
    if (frameList.count() > 0)
    {
      int lastPos = frameList[0];
      for (int i = 1; i < frameList.count(); i++)
      {
        int pos = frameList[i];
        if (pos > lastPos + 1)
        {
          // This is the end of a block. Draw it
          int xStart = (int)((float)lastPos / range.second * s.width());
          int xEnd   = (int)((float)pos     / range.second * s.width());
          painter.fillRect(xStart, 0, xEnd - xStart, s.height(), QColor(33,150,243));

          // A new rectangle starts here
          lastPos = pos;
        }
      }
      // Draw the last rectangle that goes to the end of the list
      int xStart = (int)((float)lastPos / range.second * s.width());
      int xEnd   = (int)((float)frameList.last() / range.second * s.width());
      painter.fillRect(xStart, 0, xEnd - xStart, s.height(), QColor(33,150,243));
    }

    // Draw the percentage as text
    //painter.setPen(Qt::black);
    float bufferPercent = (float)frameList.count() / (float)(range.second + 1 - range.first) * 100;
    QString pTxt = QString::number(bufferPercent, 'f', 0) + "%";
    painter.drawText(0, 0, s.width(), s.height(), Qt::AlignCenter, pTxt);

    // Draw the border
    painter.drawRect(0, 0, s.width()-1, s.height()-1);
  }
  virtual QSize	minimumSizeHint() const Q_DECL_OVERRIDE { return QSize(20, 5); }
private:
  playlistItem *plItem;
};

PlaylistTreeWidget::PlaylistTreeWidget(QWidget *parent) : QTreeWidget(parent)
{
  setDragEnabled(true);
  setDropIndicatorShown(true);
  setDragDropMode(QAbstractItemView::InternalMove);
  setSortingEnabled(true);
  setContextMenuPolicy(Qt::DefaultContextMenu);

  setColumnCount(2);
  headerItem()->setText(0, "Item");
  headerItem()->setText(1, "Buffer");

  header()->setSectionResizeMode(1, QHeaderView::Fixed);
  header()->setSectionResizeMode(0, QHeaderView::Stretch);

  // This does not work here. Don't know why. Setting it every time a new item is added, however, works.
  //header()->resizeSection(1, 10);

  connect(this, &PlaylistTreeWidget::itemSelectionChanged, this, &PlaylistTreeWidget::slotSelectionChanged);
  connect(&autosaveTimer, &QTimer::timeout, this, &PlaylistTreeWidget::autoSavePlaylist);
}

PlaylistTreeWidget::~PlaylistTreeWidget()
{
  // This is a conventional quit. Remove the automatically saved playlist.
  autosaveTimer.stop();
  QSettings settings;
  if (settings.contains("Autosaveplaylist"))
    settings.remove("Autosaveplaylist");
}

playlistItem* PlaylistTreeWidget::getDropTarget(const QPoint &pos) const
{
  playlistItem *pItem = dynamic_cast<playlistItem*>(this->itemAt(pos));
  if (pItem != nullptr)
  {
    // check if dropped on or below/above pItem
    QRect rc = this->visualItemRect(pItem);
    QRect rcNew = QRect(rc.left(), rc.top() + 2, rc.width(), rc.height() - 4);
    if (!rcNew.contains(pos, true))
      // dropped next to pItem
      pItem = nullptr;
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
    if (!dropTarget->acceptDrops(draggedItem))
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
    event->acceptProposedAction();
  else    // default behavior
    QTreeWidget::dragEnterEvent(event);
}

void PlaylistTreeWidget::dropEvent(QDropEvent *event)
{
  if (event->mimeData()->hasUrls())
  {
    QStringList fileList;
    const QList<QUrl> urls = event->mimeData()->urls();
    for (auto &url : urls)
    {
      QString fileName = url.toLocalFile();
      fileList.append(fileName);
    }
    event->acceptProposedAction();
    // the playlist has been modified and changes are not saved
    isSaved = false;

    // Load the files to the playlist
    loadFiles(fileList);
  }
  else
  {
    // get the list of the items that are about to be dragged
    QList<QTreeWidgetItem*> dragItems = selectedItems();

    // Actually move all the items
    QTreeWidget::dropEvent(event);

    // Query the selected items that were dropped and add a new bufferStatusWidget 
    // for each of them. The old bufferStatusWidget will be deleted by the tree widget.
    QList<int> toRows;
    for(QTreeWidgetItem* item : dragItems)
    {
      playlistItem *plItem = dynamic_cast<playlistItem*>(item);
      if (plItem)
        setItemWidget(item, 1, new bufferStatusWidget(plItem, this));
    }

    // A drop event occurred which was not a file being loaded.
    // Maybe we can find out what was dropped where, but for now we just tell all
    // container items to check their children and see if they need updating.
    updateAllContainterItems();

    emit playlistChanged();
  }

  //// Update the properties panel and the file info group box.
  //// When dragging an item onto another one (for example a video onto a difference)
  //// the currentChanged slot is called but at a time when the item has not been dropped
  //// yet.
  //QTreeWidgetItem *item = currentItem();
  //playlistItem *pItem = dynamic_cast<playlistItem*>(item);
  //pItem->showPropertiesWidget();
  //propertiesDockWidget->setWindowTitle(pItem->getPropertiesTitle());
  //fileInfoGroupBox->setFileInfo(pItem->getInfoTitle(), pItem->getInfoList());
}

void PlaylistTreeWidget::updateAllContainterItems()
{
  for (int i = 0; i < topLevelItemCount(); i++)
  {
    QTreeWidgetItem *item = topLevelItem(i);
    playlistItemContainer *containerItem = dynamic_cast<playlistItemContainer*>(item);
    if (containerItem != nullptr)
      containerItem->updateChildItems();
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

  // Don't emit the itemSelectionChanged signal until we are done with adding the difference item
  {
    const QScopedValueRollback<bool> back(ignoreSlotSelectionChanged, true);

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

    appendNewItem(newDiff);
  }

  // Select the new difference item. This will also cause a itemSelectionChanged event to be send.
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

  newOverlay->guessBestLayout();
  appendNewItem(newOverlay);
  setCurrentItem(newOverlay);
}

void PlaylistTreeWidget::appendNewItem(playlistItem *item, bool emitplaylistChanged)
{
  insertTopLevelItem(topLevelItemCount(), item);
  connect(item, &playlistItem::signalItemChanged, this, &PlaylistTreeWidget::slotItemChanged);
  connect(item, &playlistItem::signalItemDoubleBufferLoaded, this, &PlaylistTreeWidget::slotItemDoubleBufferLoaded);
  setItemWidget(item, 1, new bufferStatusWidget(item, this));
  header()->resizeSection(1, 50);

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

  QAction *deleteAction = nullptr;
  QAction *cloneAction = nullptr;

  QTreeWidgetItem* itemAtPoint = itemAt(event->pos());
  if (itemAtPoint)
  {
    menu.addSeparator();
    deleteAction = menu.addAction("Delete Item");

    playlistItemText *txt = dynamic_cast<playlistItemText*>(itemAtPoint);
    if (txt)
    {
      cloneAction = menu.addAction("Clone Item...");
    }
  }

  QAction* action = menu.exec(event->globalPos());
  if (action == nullptr)
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
    deletePlaylistItems(true);
  else if (action == cloneAction)
    cloneSelectedItem();
}

std::array<playlistItem *, 2> PlaylistTreeWidget::getSelectedItems() const
{
  std::array<playlistItem *, 2> result{ { nullptr, nullptr } };
  QList<QTreeWidgetItem*> items = selectedItems();
  unsigned int i = 0;
  for (auto item : items)
  {
    result[i] = dynamic_cast<playlistItem*>(item);
    if (result[i])
      i++;
    if (i >= result.size())
      break;
  }
  return result;
}

void PlaylistTreeWidget::slotSelectionChanged()
{
  if (ignoreSlotSelectionChanged)
    return;

  // The selection changed. Get the first and second selection and emit the selectionRangeChanged signal.
  auto items = getSelectedItems();
  emit selectionRangeChanged(items[0], items[1], false);

  // Also notify the cache that a new object was selected
  emit playlistChanged();
}

void PlaylistTreeWidget::slotItemChanged(bool redraw, recacheIndicator recache)
{
  // Check if the calling object is (one of) the currently selected item(s)
  auto items = getSelectedItems();
  QObject *sender = QObject::sender();
  if (sender == items[0] || sender == items[1])
  {
    DEBUG_TREE_WIDGET("PlaylistTreeWidget::slotItemChanged sender %s", sender == items[0] ? "items[0]" : "items[1]");
    // One of the currently selected items send this signal. Inform the playbackController that something might have changed.
    emit selectedItemChanged(redraw);
  }

  if (recache != RECACHE_NONE)
  {
    playlistItem *senderItem = dynamic_cast<playlistItem*>(sender);
    emit signalItemRecache(senderItem, recache);
  }
}

void PlaylistTreeWidget::slotItemDoubleBufferLoaded()
{
  // Check if the calling object is (one of) the currently selected item(s)
  auto items = getSelectedItems();
  QObject *sender = QObject::sender();
  if (sender == items[0])
    // The first of the currently selected items send this signal.
    // Inform the playbackController that loading the double buffer of the item finished.
    emit selectedItemDoubleBufferLoad(0);
  if (sender == items[1])
    // The second of the currently selected items send this signal.
    // Inform the playbackController that loading the double buffer of the item finished.
    emit selectedItemDoubleBufferLoad(1);
}

void PlaylistTreeWidget::mousePressEvent(QMouseEvent *event)
{
  QModelIndex item = indexAt(event->pos());
  QTreeView::mousePressEvent(event);
  if (item.row() == -1 && item.column() == -1)
  {
    clearSelection();
    const QModelIndex index;
    emit currentItemChanged(nullptr, nullptr);
  }
}

void PlaylistTreeWidget::keyPressEvent(QKeyEvent *event)
{
  int key = event->key();
  bool noModifiers = (event->modifiers() == Qt::NoModifier);
  if (noModifiers && (key == Qt::Key_1 || key == Qt::Key_2 || key == Qt::Key_3 || key == Qt::Key_4 ||
    key == Qt::Key_5 || key == Qt::Key_6 || key == Qt::Key_7 || key == Qt::Key_8))
  {
    // The user pressed one of the keys 1..8 without ctrl, alt or shift. Usually, the QAbstractItemView consumes
    // this key event. However, we want to handle this in the main window. So we are not going to pass it to the
    // QTreeWidget base class, but directly to the QWidget.
    QWidget::keyPressEvent(event);
    return;
  }

  // Pass the key to the base class
  QTreeWidget::keyPressEvent(event);
}

bool PlaylistTreeWidget::hasNextItem()
{
  QList<QTreeWidgetItem*> items = selectedItems();
  if (items.count() == 0)
    return false;

  // Get index of current item
  int idx = indexOfTopLevelItem(items[0]);

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
  int idx = indexOfTopLevelItem(items[0]);

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
    // Select the next item but emit the selectionRangeChanged event with changedByPlayback=true.
    const QScopedValueRollback<bool> back(ignoreSlotSelectionChanged, true);

    // Select the next item
    QTreeWidgetItem *nextItem = topLevelItem(idx + 1);
    setCurrentItem(nextItem, 0, QItemSelectionModel::ClearAndSelect);
    assert(selectedItems().count() == 1);

    // Do what the function slotSelectionChanged usually does but this time with changedByPlayback=false.
    auto items = getSelectedItems();
    emit selectionRangeChanged(items[0], items[1], true);
  }
  else
    // Set next item as current and emit the selectionRangeChanged event with changedByPlayback=false.
    setCurrentItem(topLevelItem(idx + 1));

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
  int idx = indexOfTopLevelItem(items[0]);

  // Is there a previous item?
  if (idx == 0)
    return;

  // Set next item as current
  setCurrentItem(topLevelItem(idx - 1));

  // Another item was selected. The caching thread also has to be notified about this.
  emit playlistChanged();
}

// Remove the selected items from the playlist tree widget and delete them
void PlaylistTreeWidget::deletePlaylistItems(bool selectionOnly)
{
  // First get all the items to process (top level or selected)
  QList<playlistItem*> itemList;
  if (selectionOnly)
  {
    // Get the selected items
    QList<QTreeWidgetItem*> itemsList = selectedItems();
    for (QTreeWidgetItem* item : itemsList)
      itemList.append(dynamic_cast<playlistItem*>(item));
  }
  else
  {
    // Get all top level items
    for (int i = 0; i < topLevelItemCount(); i++)
      itemList.append(dynamic_cast<playlistItem*>(topLevelItem(i)));
  }
    
  // For all items, expand the items that contain children. However, do not add an item twice.
  QList<playlistItem*> unfoldedItemList;
  for (playlistItem *plItem : itemList)
  {
    playlistItemContainer *containerItem = dynamic_cast<playlistItemContainer*>(plItem);
    if (containerItem)
    {
      // Add all children (if not yet in the list)
      QList<playlistItem*> children = containerItem->takeAllChildItemsRecursive();
      for (playlistItem* child : children)
        if (!unfoldedItemList.contains(child))
          unfoldedItemList.append(child);
    }
    
    // Add the item itself (if not yet in the list)
    if (!unfoldedItemList.contains(plItem))
      unfoldedItemList.append(plItem);
  }

  // Is there anything to delete?
  if (unfoldedItemList.count() == 0)
    return;

  // Actually delete the items in the unfolded list
  for (playlistItem *plItem : unfoldedItemList)
  {
    // Tag the item for deletion. This will disable loading/caching of the item.
    plItem->tagItemForDeletion();

    // If the item is in a container item we have to inform the container that the item will be deleted.
    playlistItem *parentItem = plItem->parentPlaylistItem();
    if (parentItem)
      parentItem->itemAboutToBeDeleted(plItem);
    else
    {
      // The item has no parent. It is a top level item.
      int topIdx = indexOfTopLevelItem(plItem);
      if (topIdx != -1)
        takeTopLevelItem(topIdx);
    }

    // Emit that the item is about to be delete
    emit itemAboutToBeDeleted(plItem);
  }

  if (!selectionOnly)
    // One of the items we deleted might be the child of a container item.
    // Update all container items.
    updateAllContainterItems();
  
  // Something was deleted. The playlist changed.
  emit playlistChanged();
}

void PlaylistTreeWidget::loadFiles(const QStringList &files)
{
  //qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "MainWindow::loadFiles()";

  // this might be used to associate a statistics item with a video item
  playlistItem* lastAddedItem = nullptr;

  QStringList filesToOpen;

  for (auto &fileName : files)
  {
    if (!(QFile(fileName).exists()))
      continue;

    QFileInfo fi(fileName);

    if (fi.isDir())
    {
      // This is a directory. Open all supported files from the directory.

      // Get all supported extensions/filters
      QStringList filters = playlistItems::getSupportedNameFilters();

      // Add all files in the directory to filePathList
      const QStringList dirFiles = QDir(fileName).entryList(filters);
      for (auto &f : dirFiles)
      {
        QString filePathName = fileName + "/" + f;
        if ((QFile(filePathName).exists()))
          filesToOpen.append(filePathName);
      }
    }
    else
      filesToOpen.append(fileName);
  }

  // Open all files that are in filesToOpen
  for (auto filePath : filesToOpen)
  {
    QFileInfo fi(filePath);

    QString ext = fi.suffix().toLower();
    if (ext == "yuvplaylist")
    {
      // Load the playlist
      if (loadPlaylistFile(filePath))
        // Add the playlist file as one of the recently opened files.
        addFileToRecentFileSetting(filePath);
    }
    else
    {
      // Try to open the file
      playlistItem *newItem = playlistItems::createPlaylistItemFromFile(this, filePath);
      if (newItem)
      {
        appendNewItem(newItem, false);
        lastAddedItem = newItem;

        // Add the file as one of the recently openend files.
        addFileToRecentFileSetting(filePath);
        isSaved = false;
      }
    }
  }

  // Open all files

  if (lastAddedItem)
  {
    // Something was added. Select the last added item.
    setCurrentItem(lastAddedItem, 0, QItemSelectionModel::ClearAndSelect);

    // The signal playlistChanged mus not be emitted again here because the setCurrentItem(...) function already did
  }
}

void PlaylistTreeWidget::addFileToRecentFileSetting(const QString &fileName)
{
  QSettings settings;
  QStringList files = settings.value("recentFileList").toStringList();
  files.removeAll(fileName);
  files.prepend(fileName);
  while (files.size() > MAX_RECENT_FILES)
    files.removeLast();

  settings.setValue("recentFileList", files);
}

QString PlaylistTreeWidget::getPlaylistString(QDir dirName)
{
  // Create the XML document structure
  QDomDocument document;
  document.appendChild(document.createProcessingInstruction(QStringLiteral("xml"), QStringLiteral("version=\"1.0\" encoding=\"UTF-8\"")));
  QDomElement plist = document.createElement(QStringLiteral("playlistItems"));
  plist.setAttribute(QStringLiteral("version"), QStringLiteral("2.0"));
  document.appendChild(plist);

  // Append all the playlist items to the output
  for(int i = 0; i < topLevelItemCount(); ++i)
  {
    QTreeWidgetItem *item = topLevelItem(i);
    playlistItem *plItem = dynamic_cast<playlistItem*>(item);

    plItem->savePlaylist(plist, dirName);
  }

  // Append the view states
  QDomElement states = document.createElement(QStringLiteral("viewStates"));
  plist.appendChild(states);
  stateHandler->savePlaylist(states);

  return document.toString();
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

  // Write the XML structure to file
  QFile file(filename);
  file.open(QIODevice::WriteOnly | QIODevice::Text);
  QTextStream outStream(&file);
  outStream << getPlaylistString(dirName);
  file.close();

  // We saved the playlist
  isSaved = true;
}

bool PlaylistTreeWidget::loadPlaylistFile(const QString &filePath)
{
  if (topLevelItemCount() != 0)
  {
    // Clear playlist first? Ask the user
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Load playlist...");
    msgBox.setText("The current playlist is not empty. Do you want to clear the playlist first or append the playlist items to the current playlist?");
    QPushButton *clearPlaylist = msgBox.addButton(tr("Clear Playlist"), QMessageBox::ActionRole);
    msgBox.addButton(tr("Append"), QMessageBox::ActionRole);
    QPushButton *abortButton = msgBox.addButton(QMessageBox::Abort);

    msgBox.exec();

    if (msgBox.clickedButton() == clearPlaylist)
    {
      // Clear the playlist and continue
      deletePlaylistItems(false);
    }
    else if (msgBox.clickedButton() == abortButton)
    {
      // Abort loading
      return false;
    }
  }

  // Open the playlist file
  QFile file(filePath);
  QFileInfo fileInfo(file);
  if (!file.open(QIODevice::ReadOnly))
    return false;

  // Load the playlist file to buffer
  QByteArray fileBytes = file.readAll();
  return loadPlaylistFromByteArray(fileBytes, filePath);
}

bool PlaylistTreeWidget::loadPlaylistFromByteArray(QByteArray data, QString filePath)
{
  QBuffer buffer(&data);

  // Try to open the DOM document
  QDomDocument doc;
  QString errorMessage;
  int errorLine;
  int errorColumn;
  bool success = doc.setContent(&buffer, false, &errorMessage, &errorLine, &errorColumn);
  if (!success)
  {
    QMessageBox::critical(this, "Error loading playlist.", "The playlist file format could not be recognized.");
    return false;
  }

  // Get the root and parser the header
  QDomElement root = doc.documentElement();
  QString tmp1 = root.tagName();
  QString tmp2 = root.attribute("version");
  if (root.tagName() == "plist" && root.attribute("version") == "1.0")
  {
    // This is a playlist file in the old format. This is not supported anymore.
    QMessageBox::critical(this, "Error loading playlist.", "The given playlist file seems to be in the old XML format. The playlist format was changed a while back and the old format is no longer supported.");
    return false;
  }
  if (root.tagName() != "playlistItems" || root.attribute("version") != "2.0")
  {
    QMessageBox::critical(this, "Error loading playlist.", "The playlist file format could not be recognized.");
    return false;
  }

  // Iterate over all items in the playlist
  QDomNode n = root.firstChild();
  while (!n.isNull())
  {
    QDomElement elem = n.toElement();
    if (n.isElement())
    {
      playlistItem *newItem = playlistItems::loadPlaylistItem(elem, filePath);
      if (newItem)
        appendNewItem(newItem, false);
    }
    n = n.nextSibling();
  }

  // Iterate over the playlist again and load the view states
  n = root.firstChild();
  while (!n.isNull())
  {
    QDomElement elem = n.toElement();
    if (n.isElement())
    {
      if (elem.tagName() == "viewStates")
      {
        // These are the view states. Load them
        stateHandler->loadPlaylist(elem);
      }
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
  return true;
}

void PlaylistTreeWidget::checkAndUpdateItems()
{
  // Append all the playlist items to the output
  QList<playlistItem*> changedItems;
  for(int i = 0; i < topLevelItemCount(); ++i)
  {
    QTreeWidgetItem *item = topLevelItem(i);
    playlistItem *plItem = dynamic_cast<playlistItem*>(item);

    // Check (and reset) the flag if the source was changed.
    if (plItem->isSourceChanged())
      changedItems.append(plItem);
  }

  if (!changedItems.empty())
  {
    // One of the two items was changed. Does the user want to reload it?
    int ret = QMessageBox::question(parentWidget(), "Item changed", "The source of one or more currently loaded items has changed. Do you want to reload the item(s)?");
    if (ret != QMessageBox::Yes)
    {
      // The user pressed no (or the x). This can not be recommended.
      ret = QMessageBox::question(parentWidget(), "Item changed", "It is really recommended to reload the changed items. YUView does not always buffer all data from the items. We can not guarantee that the data you are shown is correct anymore. For the shown values, there is no indication if they are old or new. Parsing of statistics files may fail. So again:  Do you want to reload the item(s)?");
      if (ret != QMessageBox::Yes)
        // Really no
        return;
    }

    // Reload all items
    for (playlistItem *plItem : changedItems)
      plItem->reloadItemSource();
  }
}

void PlaylistTreeWidget::updateSettings()
{
  for(int i = 0; i < topLevelItemCount(); ++i)
  {
    QTreeWidgetItem *item = topLevelItem(i);
    playlistItem *plItem = dynamic_cast<playlistItem*>(item);

    plItem->updateSettings();
  }
}

bool PlaylistTreeWidget::isAutosaveAvailable()
{
  QSettings settings;
  return settings.contains("Autosaveplaylist");
}

void PlaylistTreeWidget::loadAutosavedPlaylist()
{
  QSettings settings;
  if (!settings.contains("Autosaveplaylist"))
    return;

  QByteArray compressedPlaylist = settings.value("Autosaveplaylist").toByteArray();
  QByteArray uncompressedPlaylist = qUncompress(compressedPlaylist);
  loadPlaylistFromByteArray(uncompressedPlaylist, QDir::current().absolutePath());

  dropAutosavedPlaylist();
}

void PlaylistTreeWidget::dropAutosavedPlaylist()
{
  QSettings settings;
  settings.remove("Autosaveplaylist");
}

void PlaylistTreeWidget::cloneSelectedItem()
{
  QList<QTreeWidgetItem*> items = selectedItems();
  if (items.count() == 0)
    return;

  // Ask the user how many clones he wants to create
  bool ok;
  int nrClones = QInputDialog::getInt(this, "How many clones of each item do you need?", "Number of clones", 1, 1, 100000, 1, &ok);
  if (!ok)
    return;

  for (QTreeWidgetItem *item : items)
  {
    playlistItem *plItem = dynamic_cast<playlistItem*>(item);

    // Is this is playlistItemText?
    playlistItemText *plItemTxt = dynamic_cast<playlistItemText*>(plItem);
    if (plItemTxt)
    {
      // Clone it
      for (int i = 0; i < nrClones; i++)
      {
        playlistItemText *newText = new playlistItemText(plItemTxt);
        appendNewItem(newText);
      }
    }
  }
}

void PlaylistTreeWidget::autoSavePlaylist()
{
  QSettings settings;
  if (topLevelItemCount() == 0)
  {
    // Empty playlist
    if (settings.contains("Autosaveplaylist"))
      settings.remove("Autosaveplaylist");
  }
  else
  {
    QString playlistAsString = getPlaylistString(QDir::current());
    QByteArray compressedPlaylist = qCompress(playlistAsString.toLatin1());
    settings.setValue("Autosaveplaylist", compressedPlaylist);
  }
}

void PlaylistTreeWidget::startAutosaveTimer()
{
  autosaveTimer.setTimerType(Qt::VeryCoarseTimer);
  autosaveTimer.start(10000);
}

void PlaylistTreeWidget::setSelectedItems(playlistItem *item1, playlistItem *item2)
{
  if (item1 != nullptr || item2 != nullptr)
  {
    auto curItems = getSelectedItems();

    if (curItems[0] == item1 && curItems[1] == item2)
      // The selection we are about to load is already selected
      return;

    // Select the saved two items
    clearSelection();

    if (item1 != nullptr)
      item1->setSelected(true);
    if (item2 != nullptr)
      item2->setSelected(true);
  }
}

QList<playlistItem*> PlaylistTreeWidget::getAllPlaylistItems(const bool topLevelOnly) const
{
  QList<playlistItem*> returnList;
  for (int i = 0; i < topLevelItemCount(); i++)
  {
    QTreeWidgetItem *item = topLevelItem(i);
    playlistItem *plItem = dynamic_cast<playlistItem*>(item);
    if (plItem != nullptr)
    {
      returnList.append(plItem);
      if (!topLevelOnly)
      {
        playlistItemContainer *container = dynamic_cast<playlistItemContainer*>(plItem);
        if (container)
          returnList.append(container->getAllChildPlaylistItems());
      }
    }
  }
  return returnList;
}
