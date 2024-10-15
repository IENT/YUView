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

#pragma once

#include <common/InfoItemAndData.h>
#include <common/SaveUi.h>
#include <common/Typedef.h>
#include <common/YUViewDomElement.h>
#include <statistics/StatisticUIHandler.h>

#include <QDir>
#include <QObject>
#include <QTreeWidgetItem>

#include <memory>

#include "ui_playlistItem.h"

namespace video
{
class FrameHandler;
}

class playlistItem : public QObject, public QTreeWidgetItem
{
  Q_OBJECT

public:
  /* Is this item indexed by a frame number or by a duration?
   * There are some modes that the playlist item can have:
   * Static: The item is shown for a specific amount of time. There is no concept of "frames" for
   * these items. Indexed: The item is indexed by frames. The item is shown by displaying all frames
   * at it's framerate. Indexing is transparent to the internal handling of numbers. The index
   * always ranges from 0 to getEndFrameIdx(). Internally, the real index in the sequence is
   * calculated using the set start/end frames.
   */
  enum class Type
  {
    Static,  // The playlist item is static
    Indexed, // The playlist item is indexed
  };

  struct Properties
  {
    QString name{};
    // Every playlist item has a unique (within the playlist) ID
    // TODO: This ID is also saved in the playlist. We can append playlists to existing
    //       playlists. In this case, we could get items with identical IDs. This must not happen.
    //       I think we need the playlist item to generate a truely unique ID here (using a timecode
    //       and everything).
    int id;
    // The playlist ID is set if the item is loaded from a playlist. Don't forget to reset this
    // after the playlist was loaded.
    int playlistID{0};

    QString propertiesWidgetTitle{};
    bool    isFileSource{false};

    Type type;
    bool isIndexedByFrame() const { return this->type == Type::Indexed; }
    bool providesStatistics{false};

    // ------ Type::Static
    double duration{PLAYLISTITEMTEXT_DEFAULT_DURATION};
    // ------ Type::Indexed
    double frameRate{DEFAULT_FRAMERATE};
    /* If your item type is playlistItem_Indexed, you must
       provide the absolute minimum and maximum frame indices that the user can set.
       Normally this is: (0, numFrames-1). This value can change. Just emit a
       SignalItemChanged to update the limits.
      */
    indexRange startEndRange{-1, -1};

    Ratio sampleAspectRatio{1, 1};
  };

  /* The default constructor requires the user to set a name that will be displayed in the
   * treeWidget and provide a pointer to the widget stack for the properties panels. The constructor
   * will then call addPropertiesWidget to add the custom properties panel.
   */
  playlistItem(const QString &itemNameOrFileName, Type type);
  virtual ~playlistItem();

  virtual Properties properties() const { return this->prop; };

  // Set the name of the item. This is also the name that is shown in the tree view
  void setName(const QString &name);

  // After loading the playlist, this playlistID has to be reset because it is only valid within
  // this playlist. If another
  // playlist is loaded later on, the value has to be invalid.
  void resetPlaylistID() { this->prop.playlistID = -1; }

  // Get the parent playlistItem (if any)
  playlistItem *parentPlaylistItem() const
  {
    return dynamic_cast<playlistItem *>(QTreeWidgetItem::parent());
  }

  // Save the element to the given XML structure. Has to be overloaded by the child classes which
  // should know how to load/save themselves.
  virtual void savePlaylist(QDomElement &root, const QDir &playlistDir) const = 0;

  // Get the size of the item (in pixels). The default implementation will return
  // the size when the infoText is drawn. In your inherited calss, you should return this
  // size if you call the playlistItem::drawItem function to draw the info text.
  virtual QSize getSize() const;

  virtual void itemAboutToBeDeleted(playlistItem *) {}

  // Return the info title and info list to be shown in the fileInfo groupBox.
  // The default implementations will return empty strings/list.
  virtual InfoData getInfo() const { return InfoData(); }
  // If the playlist item indicates to put a button into the fileInfo, this call back is called if
  // the user presses the button.
  virtual void infoListButtonPressed(int) {}

  QWidget *getPropertiesWidget()
  {
    if (!propertiesWidget)
      createPropertiesWidget();
    return propertiesWidget.get();
  }
  bool propertiesWidgetCreated() const { return static_cast<bool>(this->propertiesWidget); }

  // Does the playlist item currently accept drops of the given item?
  virtual bool acceptDrops(playlistItem *) const { return false; }

  // Draw the item using the given painter and zoom factor. If the item is indexed by frame, the
  // given frame index will be drawn. If the item is not indexed by frame, the parameter frameIdx is
  // ignored. drawRawValues can control if the raw pixel values are drawn. This implementation will
  // draw the infoText on screen. You can use this in derived classes to draw an info text in
  // certain situations.
  virtual void drawItem(QPainter *painter, int frameIdx, double zoomFactor, bool drawRawValues);

  // When a new frame is selected (by the user or by playback), it will firstly be checked if the
  // playlistitem needs to load the frame. If this returns true, the loadFrame() function will be
  // called in the background. loadRawValues is set if the raw values are also drawn.
  virtual ItemLoadingState needsLoading(int, bool) { return ItemLoadingState::LoadingNotNeeded; }

  // If the drawItem() function returned false (a frame needs to be loaded first), this function
  // will be called in a background thread so that the frame is loaded. Then the drawItem() function
  // is called again and the frame is drawn. The default implementation does nothing, but if the
  // drawItem() function can return false, this function must be reimplemented in the inherited
  // class. If playback is running, the item then might load the next frame into a double buffer for
  // fast playback. Only emit signals (loading complete) if emitSignals is set. If this item is used
  // in a container (like an overlay), the overlay will emit the appropriate signals.
  virtual void loadFrame(int, bool, bool, bool emitSignals = true) { (void)emitSignals; }

  // Return the source values under the given pixel position.
  // For example a YUV source will provide Y,U and V values. An RGB source might provide RGB values,
  // A difference item will return values from both items and the differences.
  virtual ValuePairListSets getPixelValues(const QPoint &, int) { return ValuePairListSets(); }

  // If you want your item to be droppable onto a difference/resample object, return true here and
  // return a valid video handler.
  virtual bool                 canBeUsedInProcessing() const { return false; }
  virtual video::FrameHandler *getFrameHandler() { return nullptr; }

  // If this item provides statistics, return them here so that they can be used correctly in an
  // overlay
  virtual stats::StatisticUIHandler *getStatisticsUIHandler() { return nullptr; }

  // Return true if something is currently being loaded in the background. (As in: When loading is
  // done, the item will update itself and look different)
  virtual bool isLoading() const { return false; }
  virtual bool isLoadingDoubleBuffer() const { return false; }

  // If the needsLoading function returns LoadingNeededDoubleBuffer, this should activate the double
  // buffer so that in the next draw operation it is drawn. This is done because loading of a new
  // double buffer is triggered right after the call to this function. If the buffer is not
  // activated first, it could be overwritten by the background loading process if the draw even is
  // scheduled too late.
  virtual void activateDoubleBuffer() {}

  // ----- Caching -----

  // Can this item be cached? The default is no. Set cachingEnabled in your subclass to true
  // if caching is enabled. Before every caching operation is started, this is checked. So caching
  // can also be temporarily disabled.
  virtual bool isCachable() const { return cachingEnabled && !itemTaggedForDeletion; }
  // is the item being deleted?
  virtual bool taggedForDeletion() const { return itemTaggedForDeletion; }
  // Is there a limit on the number of threads that can cache from this item at the same time? (-1 =
  // no limit)
  virtual int cachingThreadLimit() { return -1; }
  // Tag the item as "to be deleted"
  void tagItemForDeletion() { itemTaggedForDeletion = true; }
  // Cache the given frame. This function is thread save. So multiple instances of this function can
  // run at the same time. In test mode, we don't check if the frame is already cached and don't
  // cache it. We just convert it and return.
  virtual void cacheFrame(int, bool) {}
  // Get a list of all cached frames (just the frame indices)
  virtual QList<int> getCachedFrames() const { return QList<int>(); }
  virtual int        getNumberCachedFrames() const { return 0; }
  // How many bytes will caching one frame use (in bytes)?
  virtual unsigned int getCachingFrameSize() const { return 0; }
  // Remove the frame with the given index from the cache.
  virtual void removeFrameFromCache(int) {}
  virtual void removeAllFramesFromCache() {};

  // ----- Detection of source/file change events -----

  // Returns if the items source (usually a file) was changed by another process. This means that
  // the playlistItem might be invalid and showing outdated data. We should reload the file. This
  // function will also reset the flag. So the second call to this function will return false
  // (unless the file changed in the meantime).
  virtual bool isSourceChanged() { return false; }
  // If the user wants to reload the item, this function should reload the source and update the
  // item. If isSourceChanged can return true, you have to override this function.
  virtual void reloadItemSource() {}
  // If the settings change, this is called. Every playlistItem should update the icons and
  // install/remove the file watchers if this function is called.
  virtual void updateSettings() {}

  // Each playlistitem can remember the position/zoom that it was shown in to recall when it is
  // selected again
  void saveCenterOffset(QPointF centerOffset, bool primaryView)
  {
    savedCenterOffset[primaryView ? 0 : 1] = centerOffset;
  }
  void saveZoomFactor(double zoom, bool primaryView) { savedZoom[primaryView ? 0 : 1] = zoom; }
  void getZoomAndPosition(QPointF &centerOffset, double &zoom, bool primaryView)
  {
    centerOffset = savedCenterOffset[primaryView ? 0 : 1];
    zoom         = savedZoom[primaryView ? 0 : 1];
  }

signals:
  // Something in the item changed. If redraw is set, a redraw of the item is necessary.
  // If recache is set, the entire cache is now invalid and needs to be recached. This is passed to
  // the video cache, which will wait for all caching jobs to finish, clear the cache and recache
  // everything. This will trigger the tree widget to update it's contents.
  void SignalItemChanged(bool redraw, recacheIndicator recache);

  // The item finished loading a frame into the double buffer. This is relevant if playback is
  // paused and waiting for the item to load the next frame into the double buffer. This will
  // restart the timer.
  void signalItemDoubleBufferLoaded();

protected:
  // The widget which is put into the stack.
  std::unique_ptr<QWidget> propertiesWidget;

  // Create the properties widget and set propertiesWidget to point to it.
  // Overload this function in a child class to create a custom widget.
  virtual void createPropertiesWidget();

  // Create a new widget and populate it with controls
  void preparePropertiesWidget(const QString &name);

  // Is caching enabled for this item? This can be changed at any point.
  bool cachingEnabled{false};

  // Item is being deleted. We might need to wait until all caching/loading jobs for the item are
  // finished before we can actually delete it. An item that is tagged for deletion should not be
  // cached/loaded anymore.
  bool itemTaggedForDeletion{false};

  // When saving the playlist, append the properties of the playlist item (the id)
  void appendPropertiesToPlaylist(YUViewDomElement &d) const;
  // Load the properties (the playlist ID)
  static void loadPropertiesFromPlaylist(const YUViewDomElement &root, playlistItem *newItem);

  // What is the (current) type of the item?
  void setType(Type newType);

  // Create the playlist controls and return a pointer to the root layout
  QLayout *createPlaylistItemControls();

  // The default implementation of playlistItem::drawItem will draw this text on screen. You can use
  // this in derived classes to draw an info text on screen.
  QString infoText{};

  Properties prop;

protected slots:

  // A control of the playlistitem (start/end/frameRate/sampling,duration) changed
  void slotVideoControlChanged();

private:
  // Every playlist item we create gets an id (automatically). This is saved to the playlist so we
  // can match playlist items to the saved view states.
  static unsigned int idCounter;

  // Each playlistitem can remember the position/zoom that it was shown in to recall when it is
  // selected again
  QPointF savedCenterOffset[2];
  double  savedZoom[2]{1.0, 1.0};

  // The UI
  SafeUi<Ui::playlistItem> ui;
};
