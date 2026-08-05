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

#include <video/rgb/PixelFormatRGB.h>
#include <video/videoHandler.h>

#include "ui_videoHandlerRGB.h"

namespace video::rgb
{

enum class ComponentDisplayMode
{
  RGBA,
  RGB,
  R,
  G,
  B,
  A
};

/** The videoHandlerRGB can be used in any playlistItem to read/display RGB data. A playlistItem
 * could even provide multiple RGB videos. A videoHandlerRGB supports handling of RGB data and can
 * return a specific frame as a image by calling getOneFrame. All conversions from the various raw
 * RGB formats to image are performed and handled here.
 */
class videoHandlerRGB : public videoHandler
{
  Q_OBJECT

public:
  videoHandlerRGB();
  virtual ~videoHandlerRGB();

  // The format is valid if the frame width/height/pixel format are set
  virtual bool isFormatValid() const override
  {
    return (FrameHandler::isFormatValid() && srcPixelFormat.isValid());
  }

  unsigned getCachingFrameSize() const override;

  // Return the RGB values for the given pixel
  virtual QStringPairList getPixelValues(const QPoint &pixelPos,
                                         int           frameIdx,
                                         FrameHandler *item2,
                                         const int     frameIdx1 = 0) override;

  // Get the number of bytes for one RGB frame with the current format
  virtual int64_t getBytesPerFrame() const override
  {
    return srcPixelFormat.bytesPerFrame(frameSize);
  }

  // Try to guess and set the format (frameSize/srcPixelFormat) from the raw RGB data.
  // If a file size is given, it is tested if the RGB format and the file size match.
  virtual void setFormatFromCorrelation(const QByteArray &rawRGBData,
                                        int64_t           fileSize = -1) override;

  virtual QString getFormatAsString() const override
  {
    return FrameHandler::getFormatAsString() + ";RGB;" +
           QString::fromStdString(this->srcPixelFormat.getName());
  }
  virtual bool setFormatFromString(QString format) override;

  // Create the RGB controls and return a pointer to the layout.
  // rgbFormatFixed: For example a RGB file does not have a fixed format (the user can change this),
  // other sources might provide a fixed format which the user cannot change.
  virtual QLayout *createVideoHandlerControls(bool isSizeFixed = false) override;
  // Enable / disable controls if pixel format has alpha component
  void updateControlsForNewPixelFormat();

  // Get the name of the currently selected RGB pixel format
  virtual QString getRawRGBPixelFormatName() const
  {
    return QString::fromStdString(srcPixelFormat.getName());
  }
  // Set the current raw format and update the control. Only emit a signalHandlerChanged signal
  // if emitSignal is true.
  virtual void setRGBPixelFormat(const rgb::PixelFormatRGB &format, bool emitSignal = false)
  {
    setSrcPixelFormat(format);
    if (emitSignal)
      emit signalHandlerChanged(true, RECACHE_NONE);
  }
  virtual void setRGBPixelFormatByName(const QString &name, bool emitSignal = false)
  {
    this->setRGBPixelFormat(rgb::PixelFormatRGB(name.toStdString()), emitSignal);
  }

  void
  guessAndSetPixelFormat(const filesource::frameFormatGuess::GuessedFrameFormat &frameFormat,
                         const filesource::frameFormatGuess::FileInfoForGuess   &fileInfo) override;

  // Draw the pixel values of the visible pixels in the center of each pixel. Only draw values for
  // the given range of pixels. Overridden from playlistItemVideo. This is a RGB source, so we can
  // draw the source RGB values from the source data.
  virtual void drawPixelValues(QPainter     *painter,
                               const int     frameIdx,
                               const QRect  &videoRect,
                               const double  zoomFactor,
                               FrameHandler *item2          = nullptr,
                               const bool    markDifference = false,
                               const int     frameIdxItem1  = 0) override;

  // Overload from playlistItemVideo. Calculate the difference of this videoHandlerRGB
  // to another videoHandlerRGB. If item2 cannot be converted to a videoHandlerRGB,
  // we will use the videoHandler::calculateDifference function to calculate the difference
  // using the 8bit RGB values.
  virtual QImage calculateDifference(FrameHandler    *item2,
                                     const int        frameIdxItem0,
                                     const int        frameIdxItem1,
                                     QList<InfoItem> &differenceInfoList,
                                     const int        amplificationFactor,
                                     const bool       markDifference) override;

  // Load the given frame and convert it to image. After this, currentFrameRawRGBData and
  // currentFrame will contain the frame with the given frame index.
  virtual void loadFrame(int frameIndex, bool loadToDoubleBuffer = false) override;

  virtual void savePlaylist(YUViewDomElement &root) const override;
  virtual void loadPlaylist(const YUViewDomElement &root) override;

protected:
  ComponentDisplayMode componentDisplayMode{ComponentDisplayMode::RGBA};

  static std::vector<PixelFormatRGB> formatPresetList;

  // The currently selected RGB format
  PixelFormatRGB srcPixelFormat;

  // Parameters for the RGBA transformation (like scaling, invert)
  int  componentScale[4]{1, 1, 1, 1};
  bool componentInvert[4]{};
  bool limitedRange{};

  // Get the RGB values for the given pixel.
  virtual rgb::rgba_t getPixelValue(const QPoint &pixelPos) const;

  // Load the given frame and return it for caching. The current buffers (currentFrameRawRGBData and
  // currentFrame) will not be modified.
  virtual void loadFrameForCaching(int frameIndex, QImage &frameToCache) override;

private:
  // Load the raw RGB data for the given frame index into currentFrameRawRGBData.
  // Return false is loading failed.
  bool loadRawRGBData(int frameIndex);

  // Convert from RGB (which ever format is selected) to a QImage in the platform QImage format
  // (platformImageFormat)
  void convertRGBToImage(const QByteArray &sourceBuffer, QImage &outputImage);

  // Set the new pixel format thread save (lock the mutex)
  void setSrcPixelFormat(const rgb::PixelFormatRGB &newFormat);

  // Convert one frame from the current pixel format to RGB888
  void       convertSourceToRGBA32Bit(const QByteArray &sourceBuffer,
                                      unsigned char    *targetBuffer,
                                      QImage::Format    imageFormat);
  QByteArray tmpBufferRawRGBDataCaching;

  // When a caching job is running in the background it will lock this mutex, so that
  // the main thread does not change the RGB format while this is happening.
  QMutex rgbFormatMutex;

  SafeUi<Ui::videoHandlerRGB> ui;

private slots:

  void slotRGBFormatControlChanged(int selectionIndex);
  void slotDisplayOptionsChanged();
};

} // namespace video::rgb
