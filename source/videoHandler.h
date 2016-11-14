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

#ifndef VIDEOHANDLER_H
#define VIDEOHANDLER_H

#include <QTreeWidgetItem>
#include <QDomElement>
#include <QDir>
#include <QString>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QGridLayout>
#include <QTimer>
#include <QMutex>

#include "typedef.h"
#include "playlistItem.h"
#include "frameHandler.h"

#include <assert.h>

/* TODO
*/
class videoHandler : public frameHandler
{
  Q_OBJECT

public:

  /*
  */
  videoHandler();
  virtual ~videoHandler() {};
  
  // Draw the frame with the given frame index and zoom factor. If onLoadShowLasFrame is set, show the last frame
  // if the frame with the current frame index is loaded in the background.
  virtual void drawFrame(QPainter *painter, int frameIdx, double zoomFactor);

  // --- Caching ----
  virtual int getNrFramesCached() { return pixmapCache.size(); }
  virtual void cacheFrame(int frameIdx);
  virtual QList<int> getCachedFrames() const { return pixmapCache.keys(); }
  virtual void removefromCache(int idx);

  QImage getCurrentFrameAsImage() { return currentFrame.toImage(); }
    
  // Same as the calculateDifference in frameHandler. For a video we have to make sure that the right frame is loaded first.
  virtual QPixmap calculateDifference(frameHandler *item2, const int frame, QList<infoItem> &differenceInfoList, const int amplificationFactor, const bool markDifference) Q_DECL_OVERRIDE;

  // Try to guess and set the format (frameSize/srcPixelFormat) from the raw data in the right raw format.
  // If a file size is given, it is tested if the guessed format and the file size match. You can overload this
  // for any specific raw format. The default implementation does nothing.
  virtual void setFormatFromCorrelation(const QByteArray &rawData, qint64 fileSize=-1) { Q_UNUSED(rawData); Q_UNUSED(fileSize); }

  // If you know the frame size and the bit depth and the file size then we can try to guess
  // the format from that. You can override this for a specific raw format. The default implementation does nothing.
  virtual void setFormatFromSizeAndName(const QSize &size, int &bitDepth, qint64 fileSize, const QFileInfo &fileInfo) { Q_UNUSED(size); Q_UNUSED(bitDepth); Q_UNUSED(fileSize); Q_UNUSED(fileInfo); }

  // The input frame buffer. After the signal signalRequestFrame(int) is emitted, the corresponding frame should be in here and
  // requestedFrame_idx should be set.
  QPixmap requestedFrame;
  int     requestedFrame_idx;

  // If reloading a raw file (because it changed), this function will clear all buffers (also the cache). With the next drawFrame(),
  // the data will be reloaded from file.
  virtual void invalidateAllBuffers();

public slots:
  // Caching: Remove the frame with the given index from the cache
  virtual void removeFrameFromCache(int frameIdx);

signals:

  // Something in the handler was changed so that the number of frames might have changed.
  // For example the width/height or the YUV format was changed.
  void signalUpdateFrameLimits();

  // The video handler requests a certain frame to be loaded. After this signal is emitted, the frame should be in requestedFrame.
  void signalRequestFrame(int frameIdx, bool caching);
  
protected:

  // --- Drawing: We keep a buffer of the current frame as RGB image so wen don't have to ´convert
  // it from the source every time a draw event is triggered. But if currentFrameIdx is not identical to
  // the requested frame in the draw event, we will have to update currentFrame.
  QPixmap    currentFrame;
  int        currentFrameIdx;

  // As the frameHandler implementations, we get the pixel values from currentImage. For a video, however, we
  // have to first check if currentImage contains the correct frame.
  virtual QRgb getPixelVal(const QPoint &pixelPos) Q_DECL_OVERRIDE;
  virtual QRgb getPixelVal(int x, int y) Q_DECL_OVERRIDE;

  // The video handler want's to draw a frame but it's not cached yet and has to be loaded.
  // A sub class can change this implementation to request raw data of a certain format instead of an image.
  // After this function was called, currentFrame should contain the requested frame and currentFrameIdx should
  // be equal to frameIndex.
  virtual void loadFrame(int frameIndex);
  // The video handler wants to cache a frame. After the operation the frameToCache should contain
  // the requested frame. No other internal state of the specific video format handler should be changed.
  // currentFrame/currentFrameIdx is still the frame on screen. This is called from a background thread.
  virtual void loadFrameForCaching(int frameIndex, QPixmap &frameToCache);

  // True if the data is currently being loaded in the background
  bool loadingInBackground;
  
  // Only one thread at a time should request something to be loaded. 
  QMutex requestDataMutex;

  // --- Caching
  QMap<int, QPixmap> pixmapCache;
  QMutex             pixmapCacheAccess; // Only one thread should write to the pixmapCache at a time
  QTimer             cachingTimer;

  // We might need to update the currentImage
  int currentImage_frameIndex;

private:
  // Each thread caching a frame of this video, will put a mutex in here with the corresponding frame number.
  // The main thread can then wait for a certain frame to be cached.
  QMap<int, QMutex*> cachingFramesMutices;
  // This mutes is used to access the cachingFramesMutices map
  QMutex cachingFramesMuticesAccess;

signals:
  // Start the caching timer (connected to cachingTimer::start())
  void cachingTimerStart();

private slots:
  void cachingTimerEvent();

  // Override the slotVideoControlChanged slot. For a videoHandler, also the number of frames might have changed.
  void slotVideoControlChanged() Q_DECL_OVERRIDE;
};

#endif // VIDEOHANDLER_H
