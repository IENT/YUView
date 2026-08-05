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

#include <filesource/FrameFormatGuess.h>

#include "FrameHandler.h"
#include "PixelFormat.h"

#include <QBasicTimer>
#include <QFileInfo>
#include <QMutex>
#include <QSettings>
#include <atomic>
#include <functional>

namespace video
{

// Cache mode for HDR 10-bit optimization
// RawYUV mode caches original YUV data, avoiding CPU conversion overhead
// GPU does the YUV->RGB conversion in realtime using shaders
enum class CacheMode {
  ConvertedRGB,  // Traditional mode: cache CPU-converted RGB QImage
  RawYUV         // HDR optimized mode: cache raw YUV data, GPU converts at render time
};

class videoHandler : public FrameHandler
{
  Q_OBJECT

public:
  /*
   */
  videoHandler();

  // Draw the frame with the given frame index and zoom factor. If onLoadShowLasFrame is set, show
  // the last frame if the frame with the current frame index is loaded in the background.
  virtual void drawFrame(QPainter *painter, int frameIndex, double zoomFactor, bool drawRawValues);

  // --- Caching ----
  // These methods are all thread-safe and can be invoked from any thread.
  int              getNrFramesCached() const;
  void             cacheFrame(int frameIndex, bool testMode);
  virtual unsigned getCachingFrameSize() const;
  QList<int>       getCachedFrames() const;
  int              getNumberCachedFrames() const;
  bool             isInCache(int idx) const;
  virtual void     removeFrameFromCache(int frameIndex);
  virtual void     removeAllFrameFromCache();
  
  // Raw YUV cache for HDR 10-bit mode
  // Returns true if the frame exists in raw YUV cache
  bool isInRawCache(int idx) const;
  // Get raw YUV data from cache (thread-safe copy via QByteArray COW)
  QByteArray getRawYUVFromCache(int idx) const;
  /**
   * @brief Remove and return raw YUV data from cache (ownership transfer).
   *
   * On cache hit, the frame is erased from m_rawYUVCache and moved into out.
   * Playback can then hand the buffer to the HDR renderer without retaining a
   * second full-frame reference in the cache map.
   *
   * @param idx Frame index to take.
   * @param out Output buffer receiving the cache entry on success.
   * @return true when idx was present and out is non-empty.
   */
  bool takeRawYUVFromCache(int idx, QByteArray &out);
  // Get current cache mode
  CacheMode getCurrentCacheMode() const { return m_cacheMode; }
  // Check if raw YUV caching is enabled (HDR 10-bit mode)
  virtual bool shouldUseRawYUVCache() const { return false; }

  // Get the number of bytes for one frame (RGB or YUV) with the current format (if this video
  // handler uses raw data)
  virtual int64_t getBytesPerFrame() const { return -1; }

  // The Frame size is about to change. If this happens, our local buffers all need updating.
  virtual void setFrameSize(Size size) override;

  // Same as the calculateDifference in FrameHandler. For a video we have to make sure that the
  // right frame is loaded first.
  virtual QImage calculateDifference(FrameHandler    *item2,
                                     const int        frameIndex0,
                                     const int        frameIndex1,
                                     QList<InfoItem> &differenceInfoList,
                                     const int        amplificationFactor,
                                     const bool       markDifference) override;

  // Try to guess and set the format (frameSize/srcPixelFormat) from the raw data in the right raw
  // format. If a file size is given, it is tested if the guessed format and the file size match.
  // You can overload this for any specific raw format. The default implementation does nothing.
  virtual void setFormatFromCorrelation(const QByteArray &, int64_t fileSize = -1)
  {
    (void)fileSize;
  }

  virtual void
  guessAndSetPixelFormat(const filesource::frameFormatGuess::GuessedFrameFormat &frameFormat,
                         const filesource::frameFormatGuess::FileInfoForGuess   &fileInfo) = 0;

  // The input frame buffer. After the signal signalRequestFrame(int) is emitted, the corresponding
  // frame should be in here and requestedFrame_idx should be set.
  QImage requestedFrame;
  int    requestedFrame_idx;

  // If reloading a raw file (because it changed), this function will clear all buffers (also the
  // cache). With the next drawFrame(), the data will be reloaded from file.
  void invalidateAllBuffers();

  // The user changed the frame. Do we need to load something before we can draw it? Do we need to
  // update the double buffer? loadRawValues: Do we also need to update the buffer of the raw values
  // because they will be drawn?
  virtual ItemLoadingState needsLoading(int frameIndex, bool loadRawValues);

  // The video handler want's to draw a frame but it's not cached yet and has to be loaded.
  // A sub class can change this implementation to request raw data of a certain format instead of
  // an image. After this function was called, currentFrame should contain the requested frame and
  // currentFrameIndex should be equal to frameIndex.
  virtual void loadFrame(int frameIndex, bool loadToDoubleBuffer = false);

  virtual int getCurrentImageIndex() const { return currentImageIndex; }

  // Set the image in the double buffer as the current image. After this, a new image can be loaded
  // to the double buffer.
  void activateDoubleBuffer();

  // Create the controls for this videoHandler and return a pointer to the layout (nullptr if the
  // handler has no controls). isSizeFixed: For example a YUV file does not have a fixed format (the
  // user can change this), other sources might provide a fixed format which the user cannot change
  // (HEVC file, ...)
  virtual QLayout *createVideoHandlerControls(bool isSizeFixed = false);

  // TODO: Explain better what the difference between these two is (currentFrameRawData and rawData)

  // A buffer with the raw RGB data (this is filled if signalRequestRawData() is emitted)
  QByteArray rawData;
  int        rawData_frameIndex{-1};

  // Do we need to load the raw values (because they are drawn on screen?)
  // The videoHandler will draw the pixel values (drawPixelValues()) using the 8bit QImage
  // currentImage so no loading is needed. However, the videoHandlerRGB or YUV may have to load the
  // raw values from the file. Check if the current buffer for the raw data (currentFrameRawData) is
  // up to date for the given frame index
  virtual ItemLoadingState needsLoadingRawValues(int frameIndex);

signals:

  // The video handler requests a certain frame to be loaded. After this signal is emitted, the
  // frame should be in requestedFrame.
  void signalRequestFrame(int frameIndex, bool caching);

  // This signal is emitted when the handler needs the raw data for a specific frame. After the
  // signal is emitted, the requested data should be in rawYUVData and rawYUVData_frameIndex should
  // be identical to frameIndex. caching will signal if this call comes from a caching thread or
  // not. If it does come from a caching thread, the result must be ready when the call to this
  // function returns.
  void signalRequestRawData(int frameIndex, bool caching);

public:
  /**
   * @brief Callback type for direct parallel data reading
   * 
   * PERFORMANCE OPTIMIZATION: This callback enables parallel frame loading
   * by bypassing the shared rawData buffer and requestDataMutex.
   * 
   * Each caching thread can provide its own buffer and read directly,
   * eliminating the serialization bottleneck.
   * 
   * @param frameIndex Frame index to load
   * @param targetBuffer Output buffer (caller-owned, thread-local)
   * @return Number of bytes read, or 0 on failure
   */
  using DirectReadCallback = std::function<int64_t(int frameIndex, QByteArray& targetBuffer)>;

  /**
   * @brief Set callback for direct parallel data reading
   * 
   * When set, loadRawFrameForCaching can use this callback to read data
   * directly into a thread-local buffer, bypassing the shared buffer.
   * 
   * @param callback The direct read callback function
   */
  void setDirectReadCallback(DirectReadCallback callback) { m_directReadCallback = callback; }

  /**
   * @brief Check if direct parallel reading is available
   */
  bool hasDirectReadCallback() const { return m_directReadCallback != nullptr; }

protected:
  // Callback for parallel data reading (bypasses requestDataMutex)
  DirectReadCallback m_directReadCallback;
  // --- Drawing: The current frame is kept in the FrameHandler::currentImage. But if
  // currentImageIndex is not identical to the requested frame in the draw event, we will have to
  // update currentImage.
  int currentImageIndex{-1};

  // As the FrameHandler implementations, we get the pixel values from currentImage. For a video,
  // however, we have to first check if currentImage contains the correct frame.
  virtual QRgb getPixelVal(int x, int y) override;

  // The video handler wants to cache a frame. After the operation the frameToCache should contain
  // the requested frame. No other internal state of the specific video format handler should be
  // changed. currentFrame/currentFrameIndex is still the frame on screen. This is called from a
  // background thread.
  virtual void loadFrameForCaching(int frameIndex, QImage &frameToCache);

  // Only one thread at a time should request something to be loaded.
  QMutex requestDataMutex;

  // We might need to update the currentImage
  int currentImage_frameIndex{-1};

  // Don't let the background loading thread set the image while we are drawing it.
  QMutex currentImageSetMutex;

  // Double buffering
  QImage doubleBufferImage;
  int    doubleBufferImageFrameIndex{-1};

  // The buffer of the raw data (RGB or YUV) of the current frame (and its frame index)
  // Before using the currentFrameRawData, you have to check if the currentFrameRawData_frameIndex
  // is correct. If not, you have to call loadFrame() to load the frame and set it correctly.
  QByteArray currentFrameRawData;
  int        currentFrameRawData_frameIndex{-1};

  // Set the cache to be invalid until a call to removefromCache(-1) clears it.
  void setCacheInvalid() { cacheValid = false; }

  // --- Caching
  QMutex mutable imageCacheAccess;
  QMap<int, QImage> imageCache;
  // Is the cache valid? The cache can be ivalid in the following scenario:
  // Somethign about how an item is shown changes (e.g. the resolution) but caching of the item is
  // currently performed. If we just cleared the cache, the wrong (currently being cached) frames
  // would still end up in the cache. So we emit SignalItemChanged with 'recache' set to true. The
  // video cache will stop, clear the cache of this item and recache everything. Until then,
  // however, the items that are in the cache (or are being put into the cache by the still running
  // threads) are invalid.
  bool cacheValid{true};
  
  // Raw YUV data cache for HDR 10-bit mode optimization
  // This cache stores original YUV data instead of converted RGB images
  // Benefits: 1) ~25% smaller memory footprint (YUV 4:2:0 vs RGBA)
  //           2) No CPU conversion overhead during caching
  //           3) GPU performs YUV->RGB conversion in realtime
  QMutex mutable m_rawCacheAccess;
  QMap<int, QByteArray> m_rawYUVCache;
  CacheMode m_cacheMode{CacheMode::ConvertedRGB};
  
  // Cached QSettings value to avoid disk I/O per frame
  // This is updated when cache mode changes or at initialization
  mutable std::atomic<bool> m_enable10BitDisplayCached{false};
  mutable std::atomic<bool> m_settingsCacheValid{false};
  
  // Load raw YUV data for caching (no CPU conversion)
  virtual void loadRawFrameForCaching(int frameIndex, QByteArray &rawDataToCache);

private slots:
  // Override the slotVideoControlChanged slot. For a videoHandler, also the number of frames might
  // have changed.
  void slotVideoControlChanged() override;
};

} // namespace video
