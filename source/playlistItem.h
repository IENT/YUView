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
#include <QMutex>
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
  playlistItem(QString itemNameOrFileName);
  virtual ~playlistItem();

  // Set/Get the name of the item. This is also the name that is shown in the tree view
  QString getName() { return plItemNameOrFileName; }
  void setName(QString name) { plItemNameOrFileName = name; setText(0, name); }

  // Every playlist item has a unique (within the playlist) ID
  unsigned int getID() { return id; }
  // If an item is loaded from a playlist, it also has a palylistID (which it was given when the playlist was saved)
  unsigned int getPlaylistID() { return playlistID; }
  // After loading the playlist, this playlistID has to be reset because it is only valid within this playlist. If another 
  // playlist is loaded later on, the value has to be invalid.
  void resetPlaylistID() { playlistID = -1; }

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
  virtual ValuePairListSets getPixelValues(QPoint pixelPos, int frameIdx) { Q_UNUSED(pixelPos); Q_UNUSED(frameIdx); return ValuePairListSets(); }

  // If you want your item to be droppable onto a difference object, return true here and return a valid video handler.
  virtual bool canBeUsedInDifference() { return false; }
  virtual frameHandler *getFrameHandler() { return NULL; }

  // If this item provides statistics, return them here so that they can be used correctly in an overlay
  virtual bool              providesStatistics()   { return false; }
  virtual statisticHandler *getStatisticsHandler() { return NULL; }

  // ----- Caching -----

  // Can this item be cached? The default is no. Set cachingEnabled in your subclass to true
  // if caching is enabled. Before every caching operation is started, this is checked. So caching
  // can also be temporarily disabled.
  bool isCachable() const { return cachingEnabled; }
  // Disable caching for this item. The video cache will not start caching of frames for this item.
  void disableCaching() {cachingEnabled = false;};
  // Cache the given frame. This function is thread save. So multiple instances of this function can run at the same time.
  virtual void cacheFrame(int idx) { Q_UNUSED(idx); }
  // Get a list of all cached frames (just the frame indices)
  virtual QList<int> getCachedFrames() const { return QList<int>(); }
  // How many bytes will caching one frame use (in bytes)?
  virtual unsigned int getCachingFrameSize() const { return 0; }
  // Remove the frame with the given index from the cache. If idx is -1, remove all frames from the cache.
  virtual void removeFrameFromCache(int idx) { Q_UNUSED(idx); };

  // ----- Detection of source/file change events -----

  // Returns if the items source (usually a file) was changed by another process. This means that the playlistItem
  // might be invalid and showing outdated data. We should reload the file. This function will also reset the flag.
  // So the second call to this function will return false (unless the file changed in the meantime).
  virtual bool isSourceChanged() { return false; }
  // If the user wants to reload the item, this function should reload the source and update the item.
  // If isSourceChanged can return true, you have to override this function.
  virtual void reloadItemSource() {}
  // If the user activates/deactivates the file watch feature, this function is called. Every playlistItem should
  // install/remove the file watchers if this function is called.
  virtual void updateFileWatchSetting() {};

  // Return a list containing this item and all child items (if any).
  QList<playlistItem*> getItemAndAllChildren();
  
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
  // Save the given item name or filename that is given when constricting a playlistItem.
  QString plItemNameOrFileName;

  // The widget which is put into the stack.
  QWidget *propertiesWidget;

  // Create the properties widget and set propertiesWidget to point to it.
  // Overload this function in a child class to create a custom widget. The default
  // implementation here will add an empty widget.
  virtual void createPropertiesWidget( ) { propertiesWidget = new QWidget; }

  // Is caching enabled for this item? This can be changed at any point.
  bool cachingEnabled;

  // When saving the playlist, append the properties of the playlist item (the id)
  void appendPropertiesToPlaylist(QDomElementYUView &d);
  // Load the properties (the playlist ID)
  static void loadPropertiesFromPlaylist(QDomElementYUView root, playlistItem *newItem);

private:
  // Every playlist item we create gets an id (automatically). This is saved to the playlist so we can match
  // playlist items to the saved view states.
  static unsigned int idCounter;
  unsigned int id;
  // The playlist ID is set if the item is loaded from a playlist. Don't forget to reset this after the playlist was loaded.
  unsigned int playlistID;
};

#endif // PLAYLISTITEM_H
