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

#include "playlistItem.h"
#include "video/videoHandlerRGB.h"
#include "video/videoHandlerYUV.h"

#include <memory>

/* This class is a helper class that you can inherit from if your playlistItem uses a videoHandler.
 * Here, we already define a lot of the forwards to the video handler. If you have multiple videos
 * or also handle statistics, you have to reimplement some of the functions.
 */
class playlistItemWithVideo : public playlistItem
{
public:
  playlistItemWithVideo(const QString &itemNameOrFileName);

  // Draw the item
  virtual void
  drawItem(QPainter *painter, int frameIdx, double zoomFactor, bool drawRawValues) override;

  // All the functions that we have to overload if we are using a video handler
  virtual QSize                getSize() const override;
  virtual video::FrameHandler *getFrameHandler() override { return this->video.get(); }
  virtual void                 activateDoubleBuffer() override
  {
    if (video)
      video->activateDoubleBuffer();
  }

  // Do we need to load the frame first?
  virtual ItemLoadingState needsLoading(int frameIdx, bool loadRawValues) override;

  // -- Caching
  // Cache the given frame
  virtual void cacheFrame(int frameIdx, bool testMode) override
  {
    if (!cachingEnabled || unresolvableError)
      return;
    video->cacheFrame(frameIdx, testMode);
  }
  // Get a list of all cached frames (just the frame indices)
  virtual QList<int> getCachedFrames() const override;
  virtual int        getNumberCachedFrames() const override
  {
    return unresolvableError ? 0 : video->getNumberCachedFrames();
  }
  // How many bytes will caching one frame use (in bytes)?
  virtual unsigned int getCachingFrameSize() const override
  {
    return unresolvableError ? 0 : video->getCachingFrameSize();
  }
  // Remove the given frame from the cache
  virtual void removeFrameFromCache(int frameIdx) override
  {
    if (video)
      video->removeFrameFromCache(frameIdx);
  }
  virtual void removeAllFramesFromCache() override
  {
    if (video)
      video->removeAllFrameFromCache();
  }
  // This item is cachable, if caching is enabled and if the raw format is valid (can be cached).
  virtual bool isCachable() const override
  {
    return !unresolvableError && playlistItem::isCachable() && video->isFormatValid();
  }

  // Load the frame in the video item. Emit SignalItemChanged(true,false) when done. Always called
  // from a thread.
  virtual void
  loadFrame(int frameIdx, bool playing, bool loadRawData, bool emitSignals = true) override;

  // Is an image currently being loaded?
  virtual bool isLoading() const override { return isFrameLoading; }
  virtual bool isLoadingDoubleBuffer() const override { return isFrameLoadingDoubleBuffer; }

private slots:
  void slotVideoHandlerChanged(bool redrawNeeded, recacheIndicator recache);

protected:
  // A pointer to the videHandler. In the derived class, don't forget to set this.
  std::unique_ptr<video::videoHandler> video;

  // The videoHandler can be a videoHandlerRGB or a videoHandlerYUV
  video::RawFormat rawFormat{video::RawFormat::Invalid};
  // Get a raw pointer to either version of the videoHandler
  video::yuv::videoHandlerYUV *getYUVVideo()
  {
    assert(rawFormat == video::RawFormat::YUV);
    return dynamic_cast<video::yuv::videoHandlerYUV *>(video.get());
  }
  video::rgb::videoHandlerRGB *getRGBVideo()
  {
    assert(rawFormat == video::RawFormat::RGB);
    return dynamic_cast<video::rgb::videoHandlerRGB *>(video.get());
  }
  const video::yuv::videoHandlerYUV *getYUVVideo() const
  {
    assert(rawFormat == video::RawFormat::YUV);
    return dynamic_cast<const video::yuv::videoHandlerYUV *>(video.get());
  }
  const video::rgb::videoHandlerRGB *getRGBVideo() const
  {
    assert(rawFormat == video::RawFormat::RGB);
    return dynamic_cast<const video::rgb::videoHandlerRGB *>(video.get());
  }

  // Connect the basic signals from the video
  void connectVideo();

  virtual void updateStartEndRange(){};

  // Is the loadFrame function currently loading?
  bool isFrameLoading{};
  bool isFrameLoadingDoubleBuffer{};

  // Set if an unresolvable error occurred. In this case, we just draw an error text.
  bool unresolvableError{};
  bool setError(QString error)
  {
    unresolvableError = true;
    infoText          = error;
    return false;
  }
};
