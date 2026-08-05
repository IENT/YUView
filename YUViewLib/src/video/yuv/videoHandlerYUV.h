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

#include <common/EnumMapper.h>
#include <video/videoHandler.h>
#include <video/yuv/PixelFormatYUV.h>
#include <video/yuv/YUVConversionTypes.h>
#include <video/hdr/HDRRenderingManager.h>
#include "ui_videoHandlerYUV.h"

#include <QMutex>
#include <QWindow>

namespace video::yuv
{

// Types yuv_t, ComponentDisplayMode, ComponentDisplayModeMapper, and ConversionSettings
// are now defined in YUVConversionTypes.h for use by other modules (YUVPixelRenderer, etc.)

/** The videoHandlerYUV can be used in any playlistItem to read/display YUV data. A playlistItem
 * could even provide multiple YUV videos. A videoHandlerYUV supports handling of YUV data and can
 * return a specific frame as a image by calling getOneFrame. All conversions from the various YUV
 * formats to RGB are performed and handled here.
 */
class videoHandlerYUV : public videoHandler
{
  Q_OBJECT

public:
  videoHandlerYUV();
  ~videoHandlerYUV();

  unsigned getCachingFrameSize() const override;

  // HDR 10-bit optimization - check if raw YUV caching should be used
  // Returns true when: 1) HDR 10-bit display is enabled AND 2) source is 10-bit YUV
  // This bypasses CPU-side YUV->RGB conversion, letting GPU do it at render time
  bool shouldUseRawYUVCache() const override;

         // The format is valid if the frame width/height/pixel format are set
  virtual bool isFormatValid() const override
  {
    return (FrameHandler::isFormatValid() && this->srcPixelFormat.canConvertToRGB(frameSize));
  }

         // Certain settings for a YUV source are invalid. In this case we will draw an error message
         // instead of the image.
  virtual void
  drawFrame(QPainter *painter, int frameIdx, double zoomFactor, bool drawRawData) override;

         // Return the YUV values for the given pixel
         // If a second item is provided, return the difference values to that item at the given position.
         // If th second item cannot be cast to a videoHandlerYUV, we call the FrameHandler::getPixelValues
         // function.
  virtual QStringPairList getPixelValues(const QPoint &pixelPos,
                                         int           frameIdx,
                                         FrameHandler *item2     = nullptr,
                                         const int     frameIdx1 = 0) override;

         // Overload from playlistItemVideo. Calculate the difference of this playlistItemYuvSource
         // to another playlistItemVideo. If item2 cannot be converted to a playlistItemYuvSource,
         // we will use the playlistItemVideo::calculateDifference function to calculate the difference
         // using the RGB values.
  virtual QImage calculateDifference(FrameHandler    *item2,
                                     const int        frameIdxItem0,
                                     const int        frameIdxItem1,
                                     QList<InfoItem> &differenceInfoList,
                                     const int        amplificationFactor,
                                     const bool       markDifference) override;

         // Get the number of bytes for one YUV frame with the current format
  virtual int64_t getBytesPerFrame() const override
  {
    return srcPixelFormat.bytesPerFrame(frameSize);
  }

  void
  guessAndSetPixelFormat(const filesource::frameFormatGuess::GuessedFrameFormat &frameFormat,
                         const filesource::frameFormatGuess::FileInfoForGuess   &fileInfo) override;

         // Try to guess and set the format (frameSize/srcPixelFormat) from the raw YUV data.
         // If a file size is given, it is tested if the YUV format and the file size match.
  virtual void setFormatFromCorrelation(const QByteArray &rawYUVData,
                                        int64_t           fileSize = -1) override;

  virtual QString getFormatAsString() const override
  {
    return FrameHandler::getFormatAsString() + ";YUV;" +
           QString::fromStdString(this->srcPixelFormat.getName());
  }
  virtual bool setFormatFromString(QString format) override;

         // Create the YUV controls and return a pointer to the layout.
         // yuvFormatFixed: For example a YUV file does not have a fixed format (the user can change this),
         // other sources might provide a fixed format which the user cannot change (HEVC file, ...)
  virtual QLayout *createVideoHandlerControls(bool isSizeAndFormatFixed = false) override;

  // Clear restart notice after restart (PRD Requirements 5.1 & 5.3)
  void clearHDRRestartNotice();

         // Get the name of the currently selected YUV pixel format
  virtual QString getRawPixelFormatYUVName() const
  {
    return QString::fromStdString(srcPixelFormat.getName());
  }
  // Set the current YUV format and update the control. Only emit a signalHandlerChanged signal
  // if emitSignal is true.
  virtual void setPixelFormatYUV(const PixelFormatYUV &fmt, bool emitSignal = false);
  virtual void setPixelFormatYUVByName(const QString &name, bool emitSignal = false)
  {
    this->setPixelFormatYUV(PixelFormatYUV(name.toStdString()), emitSignal);
  }
  virtual void setYUVColorConversion(ColorConversion conversion);

         // When loading a videoHandlerYUV from playlist file, this can be used to set all the parameters
         // at once
  void loadValues(Size frameSize, const QString &sourcePixelFormat);

         // Draw the pixel values of the visible pixels in the center of each pixel. Only draw values for
         // the given range of pixels. Overridden from playlistItemVideo. This is a YUV source, so we can
         // draw the YUV values.
  virtual void drawPixelValues(QPainter     *painter,
                               const int     frameIdx,
                               const QRect  &videoRect,
                               const double  zoomFactor,
                               FrameHandler *item2          = nullptr,
                               const bool    markDifference = false,
                               const int     frameIdxItem1  = 0) override;

         // Load the given frame and convert it to image. After this, currentFrameRawYUVData and
         // currentFrame will contain the frame with the given frame index.
  virtual void loadFrame(int frameIndex, bool loadToDoubleBuffer = false) override;

  /**
   * @brief Determine whether a frame load is required for the current playback mode.
   *
   * In HDR raw-cache mode this bypasses double-buffer/image-cache checks and only
   * validates raw YUV availability (raw cache or current raw-frame buffer). This
   * avoids scheduling CPU-side YUV->RGB preloading work that is never consumed by
   * the HDR rendering path.
   */
  ItemLoadingState needsLoading(int frameIndex, bool loadRawValues) override;

         // If this is set, the pixel values drawn in the drawPixels function will be scaled according to
         // the bit depth. E.g: The bit depth is 8 and the pixel value is 127, then the value shown will be
         // -1.
  bool showPixelValuesAsDiff{false};

  QByteArray     getDiffYUV() const { return this->diffYUV; };
  PixelFormatYUV getDiffYUVFormat() const { return this->diffYUVFormat; }

  bool isDiffReady() const { return this->diffReady; }

  virtual void savePlaylist(YUViewDomElement &root) const override;
  virtual void loadPlaylist(const YUViewDomElement &root) override;

  // HDR window access for UI integration (delegated to HDRRenderingManager)
  QWindow* getHDRWindow() const;
  bool isHDRRenderingActive() const;


  // Get current frame as QImage for HDR rendering
  QImage getCurrentFrameAsImage();

  /**
   * @brief Convert only a small rectangular region of the current frame from YUV to RGB.
   *
   * This avoids the full-frame CPU YUV->RGB conversion that the HDR overlay ZoomBox
   * previously triggered once per playback frame. The returned image has the exact
   * size of the clipped region (i.e. region.size() after clamping to the frame),
   * in Format_ARGB32_Premultiplied with standard BT.601/709/2020 limited/full-range
   * math (same as the main CPU path), suitable for direct blit by QPainter.
   *
   * Typical use: the HDR overlay needs a 5x5 ZoomBox preview; calling this with
   * a 5x5 region converts only ~25 luma + up to 9 chroma samples instead of the
   * whole frame.
   *
   * Returns a null QImage when the current raw YUV buffer is unavailable or the
   * region does not overlap the frame.
   *
   * Thread-safety: takes an internal snapshot of the live raw YUV buffer, so it is
   * safe to call concurrently with background loaders.
   */
  QImage getCurrentFramePatchAsImage(const QRect &regionInPixels);

  /**
   * @brief Sample a single YUV pixel from the current raw frame buffer.
   *
   * Thread-safe: snapshots the live raw buffer before sampling. Used by the HDR
   * overlay pixel provider so the renderer does not need a duplicate full-frame
   * YUV copy for zoom-box / raw-value overlays.
   *
   * @param pixelPos Pixel coordinates in frame space.
   * @param value Output Y/U/V sample on success.
   * @return true when a valid raw frame is available and the pixel is in range.
   */
  bool getYuvPixelValueAt(const QPoint &pixelPos, yuv_t &value) const;

  // Expose whether current source is a 10-bit YUV candidate for HDR
  bool isHDR10Candidate() const {
    return srcPixelFormat.getBitsPerSample() == 10;
  }

signals:
         // TODO: Add working signals for zoom and playback control when proper implementation is found

  // HDR rendering state change signals

protected:
  ConversionSettings conversionSettings{};

         // The currently selected YUV format
  PixelFormatYUV srcPixelFormat;

  virtual yuv_t getPixelValue(const QPoint &pixelPos) const;

         // Load the given frame and return it for caching. The current buffers (currentFrameRawYUVData and
         // currentFrame) will not be modified.
  virtual void loadFrameForCaching(int frameIndex, QImage &frameToCache) override;

  // HDR 10-bit optimization - load raw YUV data for caching
  // This skips CPU-side YUV->RGB conversion, caching raw YUV data instead
  // GPU will perform color conversion at render time using shaders
  void loadRawFrameForCaching(int frameIndex, QByteArray &rawDataToCache) override;

private:
  /**
   * @brief Apply the default color-conversion matrix for the display mode.
   *
   * SDR (hdrEnabled=false) uses BT.709 Limited Range;
   * HDR (hdrEnabled=true) uses BT.2020 Limited Range.
   * Updates UI / cache only when the target matrix differs from the current value.
   *
   * @param hdrEnabled Whether HDR display is intended (10-bit HDR checkbox).
   */
  void applyDefaultColorConversionForDisplayMode(bool hdrEnabled);

  /**
   * @brief Return a thread-safe snapshot of the current raw YUV frame buffer.
   *
   * The returned QByteArray uses Qt implicit sharing, so this operation is cheap
   * and guarantees stable storage for downstream conversion while other threads
   * may update the live raw buffer.
   *
   * @param frameIndex Expected frame index of the live buffer.
   * @param snapshot Output snapshot buffer when available.
   * @return true if a valid snapshot was captured for frameIndex.
   */
  bool getCurrentRawFrameSnapshot(int frameIndex, QByteArray &snapshot) const;

  // Load the raw YUV data for the given frame index into currentFrameRawYUVData.
  // Return false is loading failed.
  bool loadRawYUVData(int frameIndex);

  // Protect currentFrameRawData/currentFrameRawData_frameIndex against concurrent access.
  mutable QMutex m_currentFrameRawDataMutex;

         // Set the new pixel format thread save (lock the mutex). We should also emit that something
         // changed (can be disabled).
  void setSrcPixelFormat(PixelFormatYUV newFormat, bool emitChangedSignal = true);
  // Check the given format against the file size. Set the format if this is a match.
  bool checkAndSetFormat(const PixelFormatYUV format, const Size frameSize, const int64_t fileSize);

  bool setFormatFromSizeAndNamePlanar(
    QString name, const Size size, int bitDepth, Subsampling subsampling, int64_t fileSize);
  bool setFormatFromSizeAndNamePacked(
    QString name, const Size size, int bitDepth, Subsampling subsampling, int64_t fileSize);

  bool markDifferencesYUVPlanarToRGB(const QByteArray     &sourceBuffer,
                                     unsigned char        *targetBuffer,
                                     const Size            frameSize,
                                     const PixelFormatYUV &sourceBufferFormat) const;

  SafeUi<Ui::videoHandlerYUV> ui;

  bool           diffReady{};
  QByteArray     diffYUV;
  PixelFormatYUV diffYUVFormat{};

  static std::vector<PixelFormatYUV> formatPresetList;

         // Component managers for separated concerns
  HDRRenderingManager* m_hdrRenderingManager;

  // HDR timing fix members
  QImage m_pendingHDRFrame;
  int m_pendingHDRFrameIndex{-1};
  QMetaObject::Connection m_hdrWidgetInitConnection;
  bool m_isHDRCandidate{false};

  /**
   * @brief Synchronize HDR exposure controls with the selected render mode.
   * @param hdrModeIndex Current HDR mode combo-box index.
   * @param hdrEnabled Whether HDR output is enabled in the UI.
   * @param exposureNits Exposure target in nits.
   */
  void updateExposureControls(int hdrModeIndex, bool hdrEnabled, int exposureNits);

  /**
   * @brief Persist and apply the current HDR exposure target.
   * @param exposureNits Exposure target in nits.
   */
  void applyExposureNits(int exposureNits);

  /**
   * @brief Push the current frame into the HDR window without requiring a QPainter fallback.
   * @param frameIndex Frame index to bootstrap into the HDR renderer.
   * @return true if a frame payload was forwarded to the HDR renderer.
   */
  bool pushCurrentFrameToHDR(int frameIndex);

  /**
   * @brief Resolved raw YUV frame ready for HDR GPU upload (shared/COW buffer).
   */
  struct ResolvedHDRYUVFrame
  {
    QByteArray yuvData;
  };

  /**
   * @brief Resolve raw YUV for HDR: borrow from raw cache → live snapshot → load.
   *
   * Cache entries are borrowed (QByteArray COW), never taken/erased, so
   * VideoCache::cacheLevelCurrent stays consistent and looped playback keeps
   * lookahead frames.
   *
   * @param frameIndex Frame to resolve.
   * @param out Output payload on success.
   * @return true when out.yuvData is non-empty.
   */
  bool resolveRawYUVForHDR(int frameIndex, ResolvedHDRYUVFrame &out);

  /**
   * @brief Push a YUV frame into the HDR renderer (shared-buffer upload path).
   *
   * @param frameIndex Frame index to render.
   * @return true when the HDR renderer accepted a GPU YUV payload.
   */
  bool pushFrameToHDR(int frameIndex);

  /**
   * @brief Return true when the current format can use the GPU planar YUV path.
   */
  bool canUseGPUYUVHDRPath() const;

private slots:

         // All the valueChanged() signals from the controls are connected here.
  void slotYUVControlChanged();
  // The YUV format combo box was changed
  void slotYUVFormatControlChanged(int idx);
  // The 10-bit display checkbox was changed
  void slot10BitDisplayChanged();

  /**
   * @brief Apply ISP mode / Bayer pattern / cell-size UI changes.
   */
  void slotISPModeChanged();

  // The HDR mode combo box was changed (PQ/HLG selection)
  void slotHDRModeChanged(int index);

  // The tone-mapping slider uses a normalized logarithmic range.
  void slotToneMapNitsChanged(int value);

  // Exact tone-mapping luminance input changed.
  void slotToneMapNitsInputChanged(int value);

  // Check if 10-bit display should be available based on current YUV format
  void updateHDRAvailability();

  /**
   * @brief Enable/disable ISP Bayer controls when the source is YUV400.
   */
  void updateISPModeAvailability();

         // HDR rendering state change handler
  void onHDRRenderingStateChangedWindow(bool enabled, QWindow* window);
  // Show restart notice when HDR setting changes (PRD Requirements 5.1 & 5.3)
  void showHDRRestartNotice(bool hdrEnabled);

  // HDR detection failed handler - resets checkbox and shows error message
  void onHDRDetectionFailed(const QString& error);

  // Restart the application immediately to apply HDR setting
  void slotRestartNow();

private:
};

} // namespace video::yuv
