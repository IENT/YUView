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

// Activate this if you want to know when which buffer is loaded/converted to image and so on.
#define VIDEOHANDLER_DEBUG_LOADING 1
#if VIDEOHANDLER_DEBUG_LOADING && !NDEBUG
#define DEBUG_VIDEO qDebug
#else
#define DEBUG_VIDEO(fmt,...) ((void)0)
#endif

// --------- videoHandler -------------------------------------

videoHandler::videoHandler()
{
  // Initialize variables
  currentImageIdx = -1;
  currentImage_frameIndex = -1;

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
  currentImageIdx = -1;

  // Clear the cache
  clearCache();

  // emit the signal that something has changed
  emit signalHandlerChanged(true, true);
}

bool videoHandler::drawFrame(QPainter *painter, int frameIdx, double zoomFactor)
{
  // Check if the frameIdx changed and if we have to load a new frame
  bool retVal = true;
  if (frameIdx != currentImageIdx)
  {
    // The current buffer is out of date. Update it.

    if (makeCachedFrameCurrent(frameIdx))
      DEBUG_VIDEO("videoHandler::drawFrame %d loaded from cache", frameIdx);
    else
    {
      // Frame not in buffer. Return false. This will trigger the videoCache::interactiveWorker to load the frame.
      // It this is done, the playlistItem will trigger a redraw and this function will be called again.
      DEBUG_VIDEO("videoHandler::drawFrame %d not found in cache - request load", frameIdx);
      retVal = false;
    }
  }

  // Create the video QRect with the size of the sequence and center it.
  QRect videoRect;
  videoRect.setSize(frameSize * zoomFactor);
  videoRect.moveCenter(QPoint(0,0));

  // Draw the current image (currentImage)
  currentImageSetMutex.lock();
  painter->drawImage(videoRect, currentImage);
  currentImageSetMutex.unlock();

  if (zoomFactor >= 64)
  {
    // Draw the pixel values onto the pixels
    drawPixelValues(painter, frameIdx, videoRect, zoomFactor);
  }

  return retVal;
}

QImage videoHandler::calculateDifference(frameHandler *item2, const int frame, QList<infoItem> &differenceInfoList, const int amplificationFactor, const bool markDifference)
{
  // Try to cast item2 to a videoHandler
  videoHandler *videoItem2 = dynamic_cast<videoHandler*>(item2);
  if (videoItem2 == NULL)
    // The item2 is not a videoItem. Call the frameHandler implementation to calculate the difference
    return frameHandler::calculateDifference(item2, frame, differenceInfoList, amplificationFactor, markDifference);

  // Load the right images, if not already loaded)
  if (currentImageIdx != frame)
    loadFrame(frame);
  loadFrame(frame);
  if (videoItem2->currentImageIdx != frame)
    videoItem2->loadFrame(frame);

  return frameHandler::calculateDifference(item2, frame, differenceInfoList, amplificationFactor, markDifference);
}

QRgb videoHandler::getPixelVal(int x, int y)
{
  return currentImage.pixel(x, y);
}

bool videoHandler::makeCachedFrameCurrent(int frameIdx)
{
  QMutexLocker lock(&imageCacheAccess);
  if (imageCache.contains(frameIdx))
  {
    currentImage = imageCache[frameIdx];
    currentImageIdx = frameIdx;
    return true;
  }
  return false;
}

int videoHandler::getNrFramesCached() const
{
  QMutexLocker lock(&imageCacheAccess);
  return imageCache.size();
}

// Put the frame into the cache (if it is not already in there)
void videoHandler::cacheFrame(int frameIdx)
{
  DEBUG_VIDEO("videoHandler::cacheFrame %d", frameIdx);

  if (isInCache(frameIdx))
  {
    // No need to add it again
    DEBUG_VIDEO("videoHandler::cacheFrame frame %i already in cache - returning", frameIdx);
    return;
  }

  // Load the frame. While this is happening in the background the frame size must not change.
  QImage cacheImage;
  loadFrameForCaching(frameIdx, cacheImage);

  // Put it into the cache
  if (!cacheImage.isNull())
  {
    DEBUG_VIDEO("videoHandler::cacheFrame insert frame %i into cache", frameIdx);
    QMutexLocker imageCacheLock(&imageCacheAccess);
    imageCache.insert(frameIdx, cacheImage);
  }
    
  // We will emit a signalHandlerChanged(false) if a frame was cached but we don't want to emit one signal for every
  // frame. This is just not necessary. We limit the number of signals to one per second using a timer.
  emit cachingTimerStart();
}

unsigned int videoHandler::getCachingFrameSize() const
{
  auto bytes = bytesPerPixel(platformImageFormat());
  return frameSize.width() * frameSize.height() * bytes;
}

QList<int> videoHandler::getCachedFrames() const
{
  QMutexLocker lock(&imageCacheAccess);
  return imageCache.keys();
}

bool videoHandler::isInCache(int idx) const
{
  QMutexLocker lock(&imageCacheAccess);
  return imageCache.contains(idx);
}

void videoHandler::removefromCache(int idx)
{
  QMutexLocker lock(&imageCacheAccess);
  if (idx == -1)
    imageCache.clear();
  else
    imageCache.remove(idx);
  lock.unlock();

  emit cachingTimerStart();
}

void videoHandler::removeFrameFromCache(int frameIdx)
{
  Q_UNUSED(frameIdx);
  DEBUG_VIDEO("removeFrameFromCache %d", frameIdx);
}

void videoHandler::clearCache()
{
  QMutexLocker lock(&imageCacheAccess);
  imageCache.clear();
}

void videoHandler::timerEvent(QTimerEvent *event)
{
  if (event->timerId() != cachingTimer.timerId())
    return frameHandler::timerEvent(event);

  // Stop the single-shot timer.
  cachingTimer.stop();
  // Emit to update the info list (how many frames have been cached)
  emit signalHandlerChanged(false, false);
}

void videoHandler::loadFrame(int frameIndex)
{
  DEBUG_VIDEO("videoHandler::loadFrame %d\n", frameIndex);

  if (requestedFrame_idx != frameIndex)
  {
    // Lock the mutex for requesting raw data (we share the requestedFrame buffer with the caching function)
    QMutexLocker lock(&requestDataMutex);

    // Request the image to be loaded
    emit signalRequestFrame(frameIndex, false);

    if (requestedFrame_idx != frameIndex)
      // Loading failed (or is being performed in the background)
      return;
  }

  // Set the requested frame as the current frame
  QMutexLocker imageLock(&currentImageSetMutex);
  currentImage = requestedFrame;
  currentImageIdx = frameIndex;
}

void videoHandler::loadFrameForCaching(int frameIndex, QImage &frameToCache)
{
  DEBUG_VIDEO("videoHandler::loadFrameForCaching %d", frameIndex);

  QMutexLocker lock(&requestDataMutex);

  // Request the image to be loaded
  emit signalRequestFrame(frameIndex, true);

  if (requestedFrame_idx != frameIndex)
    // Loading failed
    return;

  frameToCache = requestedFrame;
}

void videoHandler::invalidateAllBuffers()
{
  // Check if the new resolution changed the number of frames in the sequence
  emit signalUpdateFrameLimits();

  // Set the current frame in the buffer to be invalid 
  currentImageIdx = -1;
  currentImage_frameIndex = -1;
  currentImageSetMutex.lock();
  currentImage = QImage();
  currentImageSetMutex.unlock();
  requestedFrame_idx = -1;

  clearCache();
}
