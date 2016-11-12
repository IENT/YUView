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

#include <assert.h>
#include <QTime>
#include <QDebug>
#include <QGridLayout>
#include <QLabel>
#include <QPainter>

#include "videoHandler.h"

// Activate this if you want to know when wich buffer is loaded/converted to pixmap and so on.
#define VIDEOHANDLER_DEBUG_LOADING 0
#if VIDEOHANDLER_DEBUG_LOADING
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
    
  connect(&cachingTimer, SIGNAL(timeout()), this, SLOT(cachingTimerEvent()));
  connect(this, SIGNAL(cachingTimerStart()), &cachingTimer, SLOT(start()));
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
    // TODO: This has to be done differently.

    // The current buffer is out of date. Update it.
    if (pixmapCache.contains(frameIdx))
    {
      // The frame is buffered
      currentFrame = pixmapCache[frameIdx];
      currentFrameIdx = frameIdx;

      // TODO: Now the yuv values that will be shown using the getPixel(...) function is wrong.
    }
    else
    {
      // Frame not in buffer. Load it.
      loadFrame( frameIdx );

      if (frameIdx != currentFrameIdx)
        // Loading failed ...
        return;
    }
  }
  
  // Create the video rect with the size of the sequence and center it.
  QRect videoRect;
  videoRect.setSize( frameSize * zoomFactor );
  videoRect.moveCenter( QPoint(0,0) );
  
  // Draw the current image ( currentFrame )
  painter->drawPixmap( videoRect, currentFrame );

  if (zoomFactor >= 64)
  {
    // Draw the pixel values onto the pixels
    drawPixelValues(painter, frameIdx, videoRect, zoomFactor);
  }
}

QPixmap videoHandler::calculateDifference(frameHandler *item2, int frame, QList<infoItem> &differenceInfoList, int amplificationFactor, bool markDifference)
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

  return currentImage.pixel( x, y );
}

// Put the frame into the cache (if it is not already in there)
void videoHandler::cacheFrame(int frameIdx)
{
  if (pixmapCache.contains(frameIdx))  
    // No need to add it again
    return;

  // Load the frame. While this is happending in the background the frame size must not change.
  QPixmap cachePixmap;
  cachingFrameSizeMutex.lock();
  loadFrameForCaching(frameIdx, cachePixmap);
  cachingFrameSizeMutex.unlock();

  // Put it into the cache
  pixmapCache.insert(frameIdx, cachePixmap);

  // We will emit a signalHandlerChanged(false) if a frame was cached but we don't want to emit one signal for every 
  // frame. This is just not necessary. We limit the number of signals to one per second.
  if (!cachingTimer.isActive())
  {
    // Start the timer (one shot, 1s).
    // When the timer runs out an signalHandlerChanged(false) signal will be emitted.
    cachingTimer.setSingleShot(true);
    cachingTimer.setInterval(1000);
    emit cachingTimerStart();
  }
}

void videoHandler::removeFrameFromCache(int frameIdx)
{
  Q_UNUSED(frameIdx);
  DEBUG_VIDEO("removeFrameFromCache %d", frameIdx);
}

void videoHandler::cachingTimerEvent()
{
  // Emit to update the info list (how many frames have been chahed)
  emit signalHandlerChanged(false, false);
}

// Compute the MSE between the given char sources for numPixels bytes
float videoHandler::computeMSE( unsigned char *ptr, unsigned char *ptr2, int numPixels )
{
  float mse=0.0;

  if( numPixels > 0 )
  {
    for(int i=0; i<numPixels; i++)
    {
      float diff = (float)ptr[i] - (float)ptr2[i];
      mse += diff*diff;
    }

    /* normalize on correlated pixels */
    mse /= (float)(numPixels);
  }

  return mse;
}

void videoHandler::loadFrame(int frameIndex)
{
  DEBUG_VIDEO( "videoHandler::loadFrame %d\n", frameIndex );

  // Lock the mutex for requesting raw data (we share the requestedFrame buffer with the caching function)
  requestDataMutex.lock();

  // Request the image to be loaded
  emit signalRequestFrame(frameIndex);
  
  if (requestedFrame_idx != frameIndex)
    // Loading failed
    return;

  currentFrame = requestedFrame;
  currentFrameIdx = frameIndex;
  requestDataMutex.unlock();
}

void videoHandler::loadFrameForCaching(int frameIndex, QPixmap &frameToCache)
{
  DEBUG_VIDEO( "videoHandler::loadFrameForCaching %d", frameIndex );
    
  requestDataMutex.lock();

  // Request the image to be loaded
  emit signalRequestFrame(frameIndex);

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
  pixmapCache.clear();
}