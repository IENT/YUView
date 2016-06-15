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

#ifndef PLAYLISTITEM_H
#define PLAYLISTITEM_H

#include "typedef.h"

#include <QTreeWidgetItem>
#include <QDomElement>
#include <QDir>
#include <assert.h>

#include "fileInfoWidget.h"

class frameHandler;
class statisticHandler;

class playlistItem :
  public QObject,
  public QTreeWidgetItem
{
  Q_OBJECT

public:

  /* The default constructor requires the user to set a name that will be displayed in the treeWidget and
   * provide a pointer to the widget stack for the properties panels. The constructor will then call
   * addPropertiesWidget to add the custom properties panel.
  */
  playlistItem(QString itemNameOrFileName)  
  {
    playlistName = itemNameOrFileName;
    propertiesWidget = NULL;
    cachingEnabled = false;
  }

  virtual ~playlistItem()
  {
    // If we have children delete them first
    for (int i = 0; i < childCount(); i++)
    {
      playlistItem *plItem = dynamic_cast<playlistItem*>(QTreeWidgetItem::takeChild(0));
      delete plItem;
    }

    delete propertiesWidget;
  }

  // Delete the item later but disable caching of this item before, so that the video cache ignores it
  // until it is really gone.
  void disableCaching()
  {
    // This will block until all background caching processes are done.
    cachingMutex.lock();
    cachingEnabled = false;
    cachingMutex.unlock();
  }

  QString getName() { return playlistName; }

  // Get the parent playlistItem (if any)
  playlistItem *parentPlaylistItem() { return dynamic_cast<playlistItem*>(QTreeWidgetItem::parent()); }

  // Save the element to the given xml structure. Has to be overloaded by the child classes which should
  // know how to load/save themselves.
  virtual void savePlaylist(QDomElement &root, QDir playlistDir) = 0;

  /* Is this item indexed by a frame number or by a duration
   *
   * A playlist item may be indexed by a frame number, or it may be a static object that is shown
   * for a set amount of time.
   * TODO: Add more info here or in the class description
  */
  virtual bool isIndexedByFrame() = 0;
  virtual indexRange getFrameIndexRange() const { return indexRange(-1,-1); }   // range -1,-1 is returend if the item cannot be drawn
  virtual QSize getSize() const = 0; //< Get the size of the item (in pixels)

  // Is this a containter item (can it have children)? If yes this function will be called when the number of children changes.
  virtual void updateChildItems() {};
  virtual void itemAboutToBeDeleter(playlistItem *item) { Q_UNUSED(item); }

  // Return the info title and info list to be shown in the fileInfo groupBox.
  // The default implementations will return empty strings/list.
  virtual QString getInfoTitel() { return ""; }
  virtual QList<infoItem> getInfoList() { return QList<infoItem>(); }

  /* Get the title of the properties panel. The child class has to overload this.
   * This can be different depending on the type of playlistItem.
   * For example a playlistItemYUVFile will return "YUV File properties".
  */
  virtual QString getPropertiesTitle() = 0;
  QWidget *getPropertiesWidget() { if (!propertiesWidget) createPropertiesWidget(); return propertiesWidget; }
  bool propertiesWidgetCreated() { return propertiesWidget; }

  // Does the playlist item currently accept drops of the given item?
  virtual bool acceptDrops(playlistItem *draggingItem) { Q_UNUSED(draggingItem); return false; }

  // ----- is indexed by frame ----
  // if the item is indexed by frame (isIndexedByFrame() returns true) the following functions have to be reimplemented by the item
  virtual double getFrameRate()    { return 0; }
  virtual int    getSampling()     { return 1; }

  // If isIndexedByFrame() return false, the item is shown for a certain period of time (duration).
  virtual double getDuration()  { return -1; }

  // Draw the item using the given painter and zoom factor. If the item is indexed by frame, the given frame index will be drawn. If the
  // item is not indexed by frame, the parameter frameIdx is ignored. If playback is set, the item might change it's drawing behavior. For
  // example, if playback is false, the item might show a "decoding" screen while a background process is decoding someghing. In case of 
  // playback the item should of course not show this screen.
  virtual void drawItem(QPainter *painter, int frameIdx, double zoomFactor, bool playback) = 0;
  
  // Return the source values under the given pixel position.
  // For example a YUV source will provide Y,U and V values. An RGB source might provide RGB values,
  // A difference item will return values from both items and the differences.
  virtual ValuePairListSets getPixelValues(QPoint pixelPos) { Q_UNUSED(pixelPos); return ValuePairListSets(); }

  // If you want your item to be droppable onto a difference object, return true here and return a valid video handler.
  virtual bool canBeUsedInDifference() { return false; }
  virtual frameHandler *getFrameHandler() { return NULL; }

  // If this item provides statistics, return them here so that they can be used correctly in an overlay
  virtual bool              providesStatistics()   { return false; }
  virtual statisticHandler *getStatisticsHandler() { return NULL; }

  // -- Caching
  // Can this item be cached? The default is no. Set cachingEnabled in your subclass to true
  // if caching is enabled. Before every caching operation is started, this is checked. So caching
  // can also be temporarily disabled.
  bool isCachable() const { return cachingEnabled; }
  // Cache the given frame
  virtual void cacheFrame(int idx) { Q_UNUSED(idx); }
  // Get a list of all cached frames (just the frame indices)
  virtual QList<int> getCachedFrames() const { return QList<int>(); }
  // How many bytes will caching one frame use (in bytes)?
  virtual unsigned int getCachingFrameSize() const { return 0; }
  // Remove the frame with the given index from the cache. If idx is -1, remove all frames from the cache.
  virtual void removeFrameFromCache(int idx) { Q_UNUSED(idx); };

  // Overrride from QTreeWidgetItem. For the first column return the file name with path.
  // For the second colum, return the current buffer fill status.
  virtual QVariant data(int column, int role) const Q_DECL_OVERRIDE
  {
    if (role == 0)
    {
      if (column == 0)
        return playlistName;
      if (column == 1 && cachingEnabled)
      {
        indexRange range = getFrameIndexRange();
        float bufferPercent = (float)getCachedFrames().count() / (float)(range.second + 1 - range.first) * 100;
        return QString::number(bufferPercent, 'f', 0) + "%";
      }
      return "";
    }
    return QTreeWidgetItem::data(column, role);
  }
  
signals:
  // Something in the item changed. If redraw is set, a redraw of the item is necessary.
  // If cacheChanged is set, something happened to the cache (maybe some or all of the items
  // in the cache are now invalid).
  void signalItemChanged(bool redraw, bool cacheChanged);

public slots:
  // Emit the signal playlistItem::signalItemChanged. Also emit the data change event (this will trigger the 
  // tree widget to update it's contents).
  void slotEmitSignalItemChanged(bool redraw, bool cacheChanged) { emit signalItemChanged(redraw, cacheChanged); }
  
protected:
  // The widget which is put into the stack.
  QWidget *propertiesWidget;

  // Create the properties widget and set propertiesWidget to point to it.
  // Overload this function in a child class to create a custom widget. The default
  // implementation here will add an empty widget.
  virtual void createPropertiesWidget( ) { propertiesWidget = new QWidget; }

  // This mutex is locked while caching is running in the background. When deleting the item, we have to wait until
  // this mutex is unlocked. Make shure to lock/unlock this mutex in your subclass
  QMutex cachingMutex;
  bool   cachingEnabled;

  // The name that is shown in the playlist. This can be changed.
  QString playlistName;
};

#endif // PLAYLISTITEM_H
