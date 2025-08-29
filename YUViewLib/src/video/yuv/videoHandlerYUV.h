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
#include <video/HDRRenderingManager.h>
#include <video/yuv/DistortionPlaybackController.h>

#include "ui_videoHandlerYUV.h"
#include <ui/PlaybackController.h>

#include <map>
#include <QTimer>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>

namespace video::yuv
{

struct yuv_t
{
  unsigned int Y, U, V;
};

enum class ComponentDisplayMode
{
  DisplayAll,
  DisplayY,
  DisplayCb,
  DisplayCr
};

const EnumMapper<ComponentDisplayMode, 4> ComponentDisplayModeMapper = {
    std::make_pair(ComponentDisplayMode::DisplayAll, "Y'CbCr"),
    std::make_pair(ComponentDisplayMode::DisplayY, "Luma (Y) Only"),
    std::make_pair(ComponentDisplayMode::DisplayCb, "Cb only"),
    std::make_pair(ComponentDisplayMode::DisplayCr, "Cr only")};

struct ConversionSettings
{
  ChromaInterpolation  chromaInterpolation{ChromaInterpolation::NearestNeighbor};
  ComponentDisplayMode componentDisplayMode{ComponentDisplayMode::DisplayAll};
  ColorConversion      colorConversion{ColorConversion::BT709_LimitedRange};
  // Parameters for the YUV transformation (like scaling, invert, offset). For Luma ([0]) and
  // chroma([1]).
  std::map<Component, MathParameters> mathParameters;
};

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

  // If this is set, the pixel values drawn in the drawPixels function will be scaled according to
  // the bit depth. E.g: The bit depth is 8 and the pixel value is 127, then the value shown will be
  // -1.
  bool showPixelValuesAsDiff{false};

  QByteArray     getDiffYUV() const { return this->diffYUV; };
  PixelFormatYUV getDiffYUVFormat() const { return this->diffYUVFormat; }

  bool isDiffReady() const { return this->diffReady; }

  virtual void savePlaylist(YUViewDomElement &root) const override;
  virtual void loadPlaylist(const YUViewDomElement &root) override;
  
  // HDR widget access for UI integration (delegated to HDRRenderingManager)
  HDR_VideoWidget* getHDRWidget() const;
  bool isHDRRenderingActive() const;
  
  // Create HDR widget with proper parent for UI integration (called by main UI)  
  HDR_VideoWidget* createHDRWidget(QWidget* parent = nullptr);
  
  // Get HDR rendered frame as QImage for QPainter integration
  QImage getHDRRenderedImage();
  
  // Get current frame as QImage for HDR rendering
  QImage getCurrentFrameAsImage();
  
  // Distortion control access (delegated to DistortionPlaybackController)
  bool isDistortionActive() const;
  int getCurrentDistortionLevel() const;

signals:
  // TODO: Add working signals for zoom and playback control when proper implementation is found
  
  // HDR rendering state change signals
  void hdrRenderingStateChanged(bool enabled, HDR_VideoWidget* widget);
  void hdrWidgetNeedsDisplay(HDR_VideoWidget* widget, bool show);

protected:
  ConversionSettings conversionSettings{};

  // The currently selected YUV format
  PixelFormatYUV srcPixelFormat;

  virtual yuv_t getPixelValue(const QPoint &pixelPos) const;

  // Load the given frame and return it for caching. The current buffers (currentFrameRawYUVData and
  // currentFrame) will not be modified.
  virtual void loadFrameForCaching(int frameIndex, QImage &frameToCache) override;

private:
  // Load the raw YUV data for the given frame index into currentFrameRawYUVData.
  // Return false is loading failed.
  bool loadRawYUVData(int frameIndex);

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
  DistortionPlaybackController* m_distortionController;
  
  // Original file tracking (for distortion analysis)
  QString oriFilePath;
  bool isShowingOriFile;
  
  // HDR timing fix members
  QImage m_pendingHDRFrame;
  QMetaObject::Connection m_hdrWidgetInitConnection;

private slots:

  // All the valueChanged() signals from the controls are connected here.
  void slotYUVControlChanged();
  // The YUV format combo box was changed
  void slotYUVFormatControlChanged(int idx);
  // The 10-bit display checkbox was changed
  void slot10BitDisplayChanged();
  
  // Check if 10-bit display should be available based on current YUV format
  void updateHDRAvailability();

  // HDR rendering state change handler
  void onHDRRenderingStateChanged(bool enabled, HDR_VideoWidget* widget);
  // Show restart notice when HDR setting changes (PRD Requirements 5.1 & 5.3)
  void showHDRRestartNotice(bool hdrEnabled);
  
  // HDR detection failed handler - resets checkbox and shows error message
  void onHDRDetectionFailed(const QString& error);

  // Distortion analysis button slots (now delegate to DistortionPlaybackController)
  void slotFirstLevelDistortion();
  void slotSecondLevelDistortion();
  
  // Revert button slots (now delegate to DistortionPlaybackController)
  void on_revertButton_L1_clicked();
  void on_revertButton_L2_clicked();
  
  // Original file helper functions (still needed for distortion analysis)
  QString getCurrentFilePath() const;
  bool isCurrentFileOri(const QString &filePath) const;
  QString findOriFile(const QString &currentFilePath) const;
  bool validateOriFile(const QString &oriFilePath, const QString &currentFilePath) const;
  bool isMouseHoveringOverOriElement() const;
  void toggleOriComparison();
  void showOriNotification(const QString &message) const;

private:
};

} // namespace video::yuv
