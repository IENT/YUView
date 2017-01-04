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

#ifndef PLAYLISTITEMWITHVIDEO_H
#define PLAYLISTITEMWITHVIDEO_H

#include "playlistItem.h"
#include "videoHandler.h"

/* This class is a helper class that you can inherit from if your playlistItem uses a videoHandler. 
 * Here, we already define a lot of the forwards to the video handler. If you have multiple videos
 * or also handle statistics, you have to reimplement some of the functions here.
 */
class playlistItemWithVideo : public playlistItem
{
public:
  playlistItemWithVideo(const QString &itemNameOrFileName, playlistItemType type);

  // Draw the item
  virtual void drawItem(QPainter *painter, int frameIdx, double zoomFactor) Q_DECL_OVERRIDE;

  // All the functions that we have to overload if we are using a video handler
  virtual QSize getSize() const Q_DECL_OVERRIDE { return (video) ? video->getFrameSize() : QSize(); }
  virtual frameHandler *getFrameHandler() Q_DECL_OVERRIDE { return video.data(); }
  // Activate the double buffer (set it as current frame)
  virtual void activateDoubleBuffer() { if (video) video->activateDoubleBuffer(); }

  // Do we need to load the frame first?
  virtual itemLoadingState needsLoading(int frameIdx) Q_DECL_OVERRIDE;

  // -- Caching
  // Cache the given frame
  virtual void cacheFrame(int idx) Q_DECL_OVERRIDE { if (!cachingEnabled) return; video->cacheFrame(idx); }
  // Get a list of all cached frames (just the frame indices)
  virtual QList<int> getCachedFrames() const Q_DECL_OVERRIDE { return video->getCachedFrames(); }
  // How many bytes will caching one frame use (in bytes)?
  virtual unsigned int getCachingFrameSize() const Q_DECL_OVERRIDE { return video->getCachingFrameSize(); }
  // Remove the given frame from the cache (-1: all frames)
  virtual void removeFrameFromCache(int idx) Q_DECL_OVERRIDE { video->removefromCache(idx); }

  // Load the frame in the video item. Emit signalItemChanged(true,false) when done.
  virtual void loadFrame(int frameIdx, bool playing) Q_DECL_OVERRIDE;

  // Is an image currently being loaded?
  virtual bool isLoading() const Q_DECL_OVERRIDE { return isFrameLoading; }
  virtual bool isLoadingDoubleBuffer() const Q_DECL_OVERRIDE { return isFrameLoadingDoubleBuffer; }

protected:
  // A pointer to the videHandler. In the derived class, don't foret to set this.
  QScopedPointer<videoHandler> video;

  // Is the loadFrame function currently loading?
  bool isFrameLoading;
  bool isFrameLoadingDoubleBuffer;
};

#endif