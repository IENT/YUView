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

#ifndef VIDEOHANDLER_H
#define VIDEOHANDLER_H

#include "frameHandler.h"
#include <QBasicTimer>
#include <QFileInfo>
#include <QMutex>

/* TODO
*/
class videoHandler : public frameHandler
{
  Q_OBJECT

public:

  /*
  */
  videoHandler();
  
  // Draw the frame with the given frame index and zoom factor. If onLoadShowLasFrame is set, show the last frame
  // if the frame with the current frame index is loaded in the background.
  virtual void drawFrame(QPainter *painter, int frameIdx, double zoomFactor, bool drawRawValues);

  // --- Caching ----
  // These methods are all thread-safe and can be invoked from any thread.
  int getNrFramesCached() const;
  void cacheFrame(int frameIdx, bool testMode);
  unsigned int getCachingFrameSize() const; // How much bytes will be used when caching one frame?
  QList<int> getCachedFrames() const;
  int getNumberCachedFrames() const;
  bool isInCache(int idx) const;
  virtual void removeFrameFromCache(int frameIdx);
  virtual void removeAllFrameFromCache();
  
  // Same as the calculateDifference in frameHandler. For a video we have to make sure that the right frame is loaded first.
  virtual QImage calculateDifference(frameHandler *item2, const int frameIdxItem0, const int frameIdxItem1, QList<infoItem> &differenceInfoList, const int amplificationFactor, const bool markDifference) Q_DECL_OVERRIDE;

  // Try to guess and set the format (frameSize/srcPixelFormat) from the raw data in the right raw format.
  // If a file size is given, it is tested if the guessed format and the file size match. You can overload this
  // for any specific raw format. The default implementation does nothing.
  virtual void setFormatFromCorrelation(const QByteArray &rawData, qint64 fileSize=-1) { Q_UNUSED(rawData); Q_UNUSED(fileSize); }

  // If you know the frame size and the bit depth and the file size then we can try to guess
  // the format from that. You can override this for a specific raw format. The default implementation does nothing.
  virtual void setFormatFromSizeAndName(const QSize size, int bitDepth, qint64 fileSize, const QFileInfo &fileInfo) { Q_UNUSED(size); Q_UNUSED(bitDepth); Q_UNUSED(fileSize); Q_UNUSED(fileInfo); }

  // The input frame buffer. After the signal signalRequestFrame(int) is emitted, the corresponding frame should be in here and
  // requestedFrame_idx should be set.
  QImage requestedFrame;
  int    requestedFrame_idx;

  // If reloading a raw file (because it changed), this function will clear all buffers (also the cache). With the next drawFrame(),
  // the data will be reloaded from file.
  virtual void invalidateAllBuffers();

  // The user changed the frame. Do we need to load something before we can draw it? Do we need to update the double buffer?
  // loadRawValues: Do we also need to update the buffer of the raw values because they will be drawn?
  itemLoadingState needsLoading(int frameIndex, bool loadRawValues);

  // The video handler want's to draw a frame but it's not cached yet and has to be loaded.
  // A sub class can change this implementation to request raw data of a certain format instead of an image.
  // After this function was called, currentFrame should contain the requested frame and currentFrameIdx should
  // be equal to frameIndex.
  virtual void loadFrame(int frameIndex, bool loadToDoubleBuffer=false);

  int getCurrentImageIndex() { return currentImageIdx; }

  // Set the image in the double buffer as the current image. After this, a new image can be loaded to the double buffer.
  void activateDoubleBuffer();
  
signals:

  // Something in the handler was changed so that the number of frames might have changed.
  // For example the width/height or the YUV format was changed.
  void signalUpdateFrameLimits();

  // The video handler requests a certain frame to be loaded. After this signal is emitted, the frame should be in requestedFrame.
  void signalRequestFrame(int frameIdx, bool caching);
    
protected:

  // --- Drawing: The current frame is kept in the frameHandler::currentImage. But if currentImageIdx is not identical to
  // the requested frame in the draw event, we will have to update currentImage.
  int currentImageIdx;

  // Do we need to load the raw values (because they are drawn on screen?)
  // The videoHandler will draw the pixel values (drawPixelValues()) using the 8bit QImage currentImage so 
  // no loading is needed. However, the videoHandlerRGB or YUV may have to load the raw values from the file.
  virtual itemLoadingState needsLoadingRawValues(int frameIdx) { Q_UNUSED(frameIdx); return LoadingNotNeeded; }

  // As the frameHandler implementations, we get the pixel values from currentImage. For a video, however, we
  // have to first check if currentImage contains the correct frame.
  virtual QRgb getPixelVal(int x, int y) Q_DECL_OVERRIDE;

  // The video handler wants to cache a frame. After the operation the frameToCache should contain
  // the requested frame. No other internal state of the specific video format handler should be changed.
  // currentFrame/currentFrameIdx is still the frame on screen. This is called from a background thread.
  virtual void loadFrameForCaching(int frameIndex, QImage &frameToCache);
    
  // Only one thread at a time should request something to be loaded. 
  QMutex requestDataMutex;

  // We might need to update the currentImage
  int currentImage_frameIndex;
  
  // Don't let the background loading thread set the image while we are drawing it.
  QMutex currentImageSetMutex;

  // Double buffering
  QImage doubleBufferImage;
  int    doubleBufferImageFrameIdx;

  // Set the cache to be invalid until a call to removefromCache(-1) clears it.
  void setCacheInvalid() { cacheValid = false; }

  // --- Caching
  QMutex mutable     imageCacheAccess;
  QMap<int, QImage>  imageCache;
  // Is the cache valid? The cache can be ivalid in the following scenario:
  // Somethign about how an item is shown changes (e.g. the resolution) but caching of the item is currently performed.
  // If we just cleared the cache, the wrong (currently being cached) frames would still end up in the cache. So we emit
  // signalItemChanged with 'recache' set to true. The video cache will stop, clear the cache of this item and recache everything.
  // Until then, however, the items that are in the cache (or are being put into the cache by the still running threads) are invalid.
  bool cacheValid;

private slots:
  // Override the slotVideoControlChanged slot. For a videoHandler, also the number of frames might have changed.
  void slotVideoControlChanged() Q_DECL_OVERRIDE;
};

#endif // VIDEOHANDLER_H
