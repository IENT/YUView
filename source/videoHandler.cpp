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

#include "videoHandler.h"

#include <QPainter>
#include "signalsSlots.h"

// Activate this if you want to know when wich buffer is loaded/converted to pixmap and so on.
#define VIDEOHANDLER_DEBUG_LOADING 1
#if VIDEOHANDLER_DEBUG_LOADING && !NDEBUG
#define DEBUG_VIDEO qDebug
#else
#define DEBUG_VIDEO(fmt,...) ((void)0)
#endif

// --------- videoHandler -------------------------------------

videoHandler::videoHandler()
{
  // Init variables
  currentFrameIdx = -1;
  currentImage_frameIndex = -1;
  loadingInBackground = false;

  connect(this, &videoHandler::cachingTimerStart, this, [=]{
    // Start the timer (one shot, 1s).
    // When the timer runs out an signalHandlerChanged(false) signal will be emitted.
    if (!cachingTimer.isActive())
      cachingTimer.start(1000, this);
  });
}

void videoHandler::slotVideoControlChanged()
{
  // First let the frameHandler handle the signal
  frameHandler::slotVideoControlChanged();

  // Check if the new resolution changed the number of frames in the sequence
  emit signalUpdateFrameLimits();

  // Set the current frame in the buffer to be invalid
  currentFrameIdx = -1;

  // Clear the cache
  pixmapCache.clear();

  // emit the signal that something has changed
  emit signalHandlerChanged(true, true);
}

void videoHandler::drawFrame(QPainter *painter, int frameIdx, double zoomFactor)
{
  // Check if the frameIdx changed and if we have to load a new frame
  if (frameIdx != currentFrameIdx)
  {
    // The current buffer is out of date. Update it.

    if (pixmapCache.contains(frameIdx))
    {
      // The frame is buffered
      currentFrame = pixmapCache[frameIdx];
      currentFrameIdx = frameIdx;
    }
    else
    {
      // Frame not in buffer.
      cachingFramesMuticesAccess.lock();
      if (cachingFramesMutices.contains(frameIdx))
      {
        // The frame is not in the buffer BUT the background caching thread is currently caching it.
        // Instead of loading it again, we should wait for the background thread to finish loading
        // and then get it from the cache.
        cachingFramesMutices[frameIdx]->lock();   // Wait for the caching thread
        cachingFramesMutices[frameIdx]->unlock();
        cachingFramesMuticesAccess.unlock();

        // The frame should now be in the cache
        if (pixmapCache.contains(frameIdx))
        {
          // The frame is buffered
          currentFrame = pixmapCache[frameIdx];
          currentFrameIdx = frameIdx;
        }
      }
      else
      {
        cachingFramesMuticesAccess.unlock();
        loadFrame(frameIdx);
      }
    }
  }

  // If the frame index was not updated, loading in the background is on it's way.
  loadingInBackground = (currentFrameIdx != frameIdx);

  // Create the video rect with the size of the sequence and center it.
  QRect videoRect;
  videoRect.setSize(frameSize * zoomFactor);
  videoRect.moveCenter(QPoint(0,0));

  // Draw the current image ( currentFrame )
  painter->drawPixmap(videoRect, currentFrame);

  if (zoomFactor >= 64)
  {
    // Draw the pixel values onto the pixels
    drawPixelValues(painter, frameIdx, videoRect, zoomFactor);
  }
}

QPixmap videoHandler::calculateDifference(frameHandler *item2, const int frame, QList<infoItem> &differenceInfoList, const int amplificationFactor, const bool markDifference)
{
  // Try to cast item2 to a videoHandler
  videoHandler *videoItem2 = dynamic_cast<videoHandler*>(item2);
  if (videoItem2 == NULL)
    // The item2 is not a videoItem. Call the frameHandler implementation to calculate the differen
    return frameHandler::calculateDifference(item2, frame, differenceInfoList, amplificationFactor, markDifference);

  // Load the right images, if not already loaded)
  if (currentFrameIdx != frame)
    loadFrame(frame);
  loadFrame(frame);
  if (videoItem2->currentFrameIdx != frame)
    videoItem2->loadFrame(frame);

  return frameHandler::calculateDifference(item2, frame, differenceInfoList, amplificationFactor, markDifference);
}

QRgb videoHandler::getPixelVal(int x, int y)
{
  if (currentImage_frameIndex != currentFrameIdx)
  {
    currentImage = currentFrame.toImage();
    currentImage_frameIndex = currentFrameIdx;
  }

  return currentImage.pixel(x, y);
}

// Put the frame into the cache (if it is not already in there)
void videoHandler::cacheFrame(int frameIdx)
{
  DEBUG_VIDEO("videoHandler::cacheFrame %d", frameIdx);

  if (pixmapCache.contains(frameIdx))
  {
    // No need to add it again
    DEBUG_VIDEO("videoHandler::cacheFrame frame %i already in cache", frameIdx);
    return;
  }

  // First, put a mutex into the cachingFramesMutices list and lock it.
  cachingFramesMuticesAccess.lock();
  if (cachingFramesMutices.contains(frameIdx))
  {
    // A background task is already caching this frame !?!
    DEBUG_VIDEO("videoHandler::cacheFrame Mute for %d already locked. Are you caching the same frame twice?", frameIdx);
    return;
  }
  cachingFramesMutices[frameIdx] = new QMutex();
  cachingFramesMutices[frameIdx]->lock();
  cachingFramesMuticesAccess.unlock();
  
  // Load the frame. While this is happending in the background the frame size must not change.
  QPixmap cachePixmap;
  loadFrameForCaching(frameIdx, cachePixmap);

  // Put it into the cache
  if (!cachePixmap.isNull())
  {
    DEBUG_VIDEO("videoHandler::cacheFrame insert frame %i into cache", frameIdx);
    pixmapCacheAccess.lock();
    pixmapCache.insert(frameIdx, cachePixmap);
    pixmapCacheAccess.unlock();
  }

  // Unlock the mutex for caching this frame and remove it from the list.
  cachingFramesMutices[frameIdx]->unlock();
  cachingFramesMuticesAccess.lock();
  delete cachingFramesMutices[frameIdx];
  cachingFramesMutices.remove(frameIdx);
  cachingFramesMuticesAccess.unlock();

  // We will emit a signalHandlerChanged(false) if a frame was cached but we don't want to emit one signal for every
  // frame. This is just not necessary. We limit the number of signals to one per second using a timer.
  emit cachingTimerStart();
}

void videoHandler::removefromCache(int idx)
{
  pixmapCacheAccess.lock();
  if (idx == -1)
    pixmapCache.clear();
  else
    pixmapCache.remove(idx);
  pixmapCacheAccess.unlock();

  emit cachingTimerStart();
}

void videoHandler::removeFrameFromCache(int frameIdx)
{
  Q_UNUSED(frameIdx);
  DEBUG_VIDEO("removeFrameFromCache %d", frameIdx);
}

void videoHandler::timerEvent(QTimerEvent *event)
{
  if (event->timerId() != cachingTimer.timerId())
    return frameHandler::timerEvent(event);

  // Stop the single-shot timer.
  cachingTimer.stop();
  // Emit to update the info list (how many frames have been chahed)
  emit signalHandlerChanged(false, false);
}

void videoHandler::loadFrame(int frameIndex)
{
  DEBUG_VIDEO("videoHandler::loadFrame %d\n", frameIndex);

  if (requestedFrame_idx != frameIndex)
  {
    // Lock the mutex for requesting raw data (we share the requestedFrame buffer with the caching function)
    requestDataMutex.lock();

    // Request the image to be loaded
    emit signalRequestFrame(frameIndex, false);

    if (requestedFrame_idx != frameIndex)
    {
      // Loading failed (or is being performed in the background)
      requestDataMutex.unlock();
      return;
    }

    requestDataMutex.unlock();
  }

  // Set the requested frame as the current frame
  currentFrame = requestedFrame;
  currentFrameIdx = frameIndex;
}

void videoHandler::loadFrameForCaching(int frameIndex, QPixmap &frameToCache)
{
  DEBUG_VIDEO("videoHandler::loadFrameForCaching %d", frameIndex);

  requestDataMutex.lock();

  // Request the image to be loaded
  emit signalRequestFrame(frameIndex, true);

  if (requestedFrame_idx != frameIndex)
  {
    // Loading failed
    requestDataMutex.unlock();
    return;
  }

  frameToCache = requestedFrame;
  requestDataMutex.unlock();
}

void videoHandler::invalidateAllBuffers()
{
  // Check if the new resolution changed the number of frames in the sequence
  emit signalUpdateFrameLimits();

  // Set the current frame in the buffer to be invalid 
  currentFrameIdx = -1;
  currentImage_frameIndex = -1;
  currentFrame = QPixmap();
  requestedFrame_idx = -1;

  // Clear the cache
  pixmapCacheAccess.lock();
  pixmapCache.clear();
  pixmapCacheAccess.unlock();
}
