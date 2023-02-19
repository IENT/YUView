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

#include "videoHandler.h"

#include <QPainter>

#include <common/FunctionsGui.h>

#include "ui_FrameHandler.h"

namespace video
{

// Activate this if you want to know when which buffer is loaded/converted to image and so on.
#define VIDEOHANDLER_DEBUG_LOADING 0
#if VIDEOHANDLER_DEBUG_LOADING && !NDEBUG
#define DEBUG_VIDEO qDebug
#else
#define DEBUG_VIDEO(fmt, ...) ((void)0)
#endif

videoHandler::videoHandler()
{
}

void videoHandler::slotVideoControlChanged()
{
  // Update the controls and get the new selected size
  auto newSize = getNewSizeFromControls();

  // Set the current frame in the buffer to be invalid
  this->currentImageIndex = -1;

  // The cache is invalid until the item is recached
  setCacheInvalid();

  if (newSize != frameSize && newSize.isValid())
  {
    // Set the new size and update the controls.
    this->setFrameSize(newSize);
    // The frame size changed. We need to redraw/re-cache.
    emit signalHandlerChanged(true, RECACHE_CLEAR);
  }
}

void videoHandler::setFrameSize(Size size)
{
  if (size != frameSize)
  {
    this->currentFrameRawData_frameIndex = -1;
    this->currentImageIndex              = -1;
    this->rawData_frameIndex             = -1;
  }

  FrameHandler::setFrameSize(size);
}

ItemLoadingState videoHandler::needsLoading(int frameIdx, bool loadRawValues)
{
  if (loadRawValues)
  {
    // First, let's check the raw values buffer.
    auto state = needsLoadingRawValues(frameIdx);
    if (state != ItemLoadingState::LoadingNotNeeded)
      return state;
  }

  // Lock the mutex for checking the cache
  QMutexLocker lock(&imageCacheAccess);

  // The raw values are not needed.
  if (frameIdx == currentImageIndex)
  {
    if (doubleBufferImageFrameIndex == frameIdx + 1)
    {
      DEBUG_VIDEO("videoHandler::needsLoading %d is current and %d found in double buffer",
                  frameIdx,
                  frameIdx + 1);
      return ItemLoadingState::LoadingNotNeeded;
    }
    else if (cacheValid && imageCache.contains(frameIdx + 1))
    {
      DEBUG_VIDEO(
          "videoHandler::needsLoading %d is current and %d found in cache", frameIdx, frameIdx + 1);
      return ItemLoadingState::LoadingNotNeeded;
    }
    else
    {
      // The next frame is not in the double buffer so that needs to be loaded.
      DEBUG_VIDEO("videoHandler::needsLoading %d is current but %d not found in double buffer",
                  frameIdx,
                  frameIdx + 1);
      return ItemLoadingState::LoadingNeededDoubleBuffer;
    }
  }

  // Check the double buffer
  if (doubleBufferImageFrameIndex == frameIdx)
  {
    // The frame in question is in the double buffer...
    if (cacheValid && imageCache.contains(frameIdx + 1))
    {
      // ... and the one after that is in the cache.
      DEBUG_VIDEO("videoHandler::needsLoading %d found in double buffer. Next frame in cache.",
                  frameIdx);
      return ItemLoadingState::LoadingNotNeeded;
    }
    else
    {
      // .. and the one after that is not in the cache.
      // Loading of the given frame index is not needed because it is in the double buffer but if
      // you draw it, the double buffer needs an update.
      DEBUG_VIDEO("videoHandler::needsLoading %d found in double buffer", frameIdx);
      return ItemLoadingState::LoadingNeededDoubleBuffer;
    }
  }

  // Check the cache
  if (cacheValid && imageCache.contains(frameIdx))
  {
    // What about the next frame? Is it also in the cache or in the double buffer?
    if (doubleBufferImageFrameIndex == frameIdx + 1)
    {
      DEBUG_VIDEO("videoHandler::needsLoading %d in cache and %d found in double buffer",
                  frameIdx,
                  frameIdx + 1);
      return ItemLoadingState::LoadingNotNeeded;
    }
    else if (cacheValid && imageCache.contains(frameIdx + 1))
    {
      DEBUG_VIDEO(
          "videoHandler::needsLoading %d in cache and %d found in cache", frameIdx, frameIdx + 1);
      return ItemLoadingState::LoadingNotNeeded;
    }
    else
    {
      // The next frame is not in the double buffer so that needs to be loaded.
      DEBUG_VIDEO("videoHandler::needsLoading %d found in cache but %d not found in double buffer",
                  frameIdx,
                  frameIdx + 1);
      return ItemLoadingState::LoadingNeededDoubleBuffer;
    }
  }

  // Frame not in buffer. Return false and request the background loading thread to load the frame.
  DEBUG_VIDEO("videoHandler::needsLoading %d not found in cache - request load", frameIdx);
  return ItemLoadingState::LoadingNeeded;
}

void videoHandler::drawFrame(QPainter *painter, int frameIdx, double zoomFactor, bool drawRawValues)
{
  // Check if the frameIdx changed and if we have to load a new frame
  if (frameIdx != this->currentImageIndex)
  {
    // The current buffer is out of date. Update it.

    // Check the double buffer
    if (frameIdx == doubleBufferImageFrameIndex)
    {
      this->currentImage      = this->doubleBufferImage;
      this->currentImageIndex = frameIdx;
      DEBUG_VIDEO("videoHandler::drawFrame %d loaded from double buffer", frameIdx);
    }
    else
    {
      QMutexLocker lock(&imageCacheAccess);
      if (this->cacheValid && this->imageCache.contains(frameIdx))
      {
        this->currentImage      = this->imageCache[frameIdx];
        this->currentImageIndex = frameIdx;
        DEBUG_VIDEO("videoHandler::drawFrame %d loaded from cache", frameIdx);
      }
    }
  }

  DEBUG_VIDEO("videoHandler::drawFrame frameIdx %d currentImageIndex %d",
              frameIdx,
              this->currentImageIndex);

  // Create the video QRect with the size of the sequence and center it.
  QRect videoRect;
  videoRect.setSize(QSize(frameSize.width * zoomFactor, frameSize.height * zoomFactor));
  videoRect.moveCenter(QPoint(0, 0));

  // Draw the current image (currentImage)
  {
    QMutexLocker lock(&this->currentImageSetMutex);
    painter->drawImage(videoRect, currentImage);
  }

  if (drawRawValues && zoomFactor >= SPLITVIEW_DRAW_VALUES_ZOOMFACTOR)
  {
    // Draw the pixel values onto the pixels
    drawPixelValues(painter, frameIdx, videoRect, zoomFactor);
  }
}

QImage videoHandler::calculateDifference(FrameHandler *   item2,
                                         const int        frameIdxItem0,
                                         const int        frameIdxItem1,
                                         QList<InfoItem> &differenceInfoList,
                                         const int        amplificationFactor,
                                         const bool       markDifference)
{
  // Try to cast item2 to a videoHandler
  videoHandler *videoItem2 = dynamic_cast<videoHandler *>(item2);
  if (videoItem2 == nullptr)
  {
    // The item2 is not a videoItem but this one is.
    if (currentImageIndex != frameIdxItem0)
      loadFrame(frameIdxItem0);
    // Call the FrameHandler implementation to calculate the difference
    return FrameHandler::calculateDifference(item2,
                                             frameIdxItem0,
                                             frameIdxItem1,
                                             differenceInfoList,
                                             amplificationFactor,
                                             markDifference);
  }

  // Load the right images, if not already loaded)
  if (currentImageIndex != frameIdxItem0)
    loadFrame(frameIdxItem0);
  if (videoItem2->currentImageIndex != frameIdxItem1)
    videoItem2->loadFrame(frameIdxItem1);

  return FrameHandler::calculateDifference(
      item2, frameIdxItem0, frameIdxItem1, differenceInfoList, amplificationFactor, markDifference);
}

QRgb videoHandler::getPixelVal(int x, int y)
{
  return currentImage.pixel(x, y);
}

int videoHandler::getNrFramesCached() const
{
  QMutexLocker lock(&imageCacheAccess);
  return imageCache.size();
}

// Put the frame into the cache (if it is not already in there)
void videoHandler::cacheFrame(int frameIdx, bool testMode)
{
  DEBUG_VIDEO("videoHandler::cacheFrame %d %s", frameIdx, testMode ? "testMode" : "");

  if (cacheValid && isInCache(frameIdx) && !testMode)
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
    if (cacheValid && !testMode)
      imageCache.insert(frameIdx, cacheImage);
  }
  else
    DEBUG_VIDEO("videoHandler::cacheFrame loading frame %i for caching failed", frameIdx);
}

unsigned videoHandler::getCachingFrameSize() const
{
  const auto hasAlpha = false;
  auto       bytes    = functionsGui::bytesPerPixel(functionsGui::platformImageFormat(hasAlpha));
  return this->frameSize.width * this->frameSize.height * bytes;
}

QList<int> videoHandler::getCachedFrames() const
{
  QMutexLocker lock(&imageCacheAccess);
  return imageCache.keys();
}

int videoHandler::getNumberCachedFrames() const
{
  QMutexLocker lock(&imageCacheAccess);
  return imageCache.size();
}

bool videoHandler::isInCache(int idx) const
{
  QMutexLocker lock(&imageCacheAccess);
  return imageCache.contains(idx);
}

void videoHandler::removeFrameFromCache(int frameIdx)
{
  DEBUG_VIDEO("removeFrameFromCache %d", frameIdx);
  QMutexLocker lock(&imageCacheAccess);
  imageCache.remove(frameIdx);
  lock.unlock();
}

void videoHandler::removeAllFrameFromCache()
{
  DEBUG_VIDEO("removeAllFrameFromCache");
  QMutexLocker lock(&imageCacheAccess);
  imageCache.clear();
  cacheValid = true;
  lock.unlock();
}

void videoHandler::loadFrame(int frameIndex, bool loadToDoubleBuffer)
{
  DEBUG_VIDEO(
      "videoHandler::loadFrame %d %s\n", frameIndex, (loadToDoubleBuffer) ? "toDoubleBuffer" : "");

  if (requestedFrame_idx != frameIndex)
  {
    // Lock the mutex for requesting raw data (we share the requestedFrame buffer with the caching
    // function)
    QMutexLocker lock(&requestDataMutex);

    // Request the image to be loaded
    emit signalRequestFrame(frameIndex, false);

    if (requestedFrame_idx != frameIndex)
      // Loading failed
      return;
  }

  if (loadToDoubleBuffer)
  {
    // Save the requested frame in the double buffer
    doubleBufferImage           = requestedFrame;
    doubleBufferImageFrameIndex = frameIndex;
  }
  else
  {
    // Set the requested frame as the current frame
    QMutexLocker imageLock(&currentImageSetMutex);
    currentImage      = requestedFrame;
    currentImageIndex = frameIndex;
  }
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
  currentFrameRawData_frameIndex = -1;
  rawData_frameIndex             = -1;

  // Set the current frame in the buffer to be invalid
  currentImageIndex       = -1;
  currentImage_frameIndex = -1;
  currentImageSetMutex.lock();
  currentImage = QImage();
  currentImageSetMutex.unlock();
  requestedFrame_idx = -1;

  imageCache.clear();
  cacheValid = true;
}

void videoHandler::activateDoubleBuffer()
{
  if (doubleBufferImageFrameIndex != -1)
  {
    currentImage      = doubleBufferImage;
    currentImageIndex = doubleBufferImageFrameIndex;
    DEBUG_VIDEO("videoHandler::drawFrame %d loaded from double buffer", currentImageIndex);
  }
}

QLayout *videoHandler::createVideoHandlerControls(bool)
{
  return nullptr;
}

ItemLoadingState videoHandler::needsLoadingRawValues(int frameIndex)
{
  return (this->currentFrameRawData_frameIndex == frameIndex) ? ItemLoadingState::LoadingNotNeeded
                                                              : ItemLoadingState::LoadingNeeded;
}

} // namespace video
