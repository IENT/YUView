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

#include "YUVPixelFormat.h"
#include "videoHandler.h"

#include "ui_videoHandlerYUV.h"

struct yuv_t
{
  unsigned int Y, U, V;
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
  videoHandlerYUV(videoHandlerYUV *clone);
  ~videoHandlerYUV();

  // The format is valid if the frame width/height/pixel format are set
  virtual bool isFormatValid() const override
  {
    return (frameHandler::isFormatValid() && this->srcPixelFormat.canConvertToRGB(frameSize));
  }

  // Certain settings for a YUV source are invalid. In this case we will draw an error message
  // instead of the image.
  virtual void
  drawFrame(QPainter *painter, int frameIdx, double zoomFactor, bool drawRawData) override;

  // Return the YUV values for the given pixel
  // If a second item is provided, return the difference values to that item at the given position.
  // If th second item cannot be cast to a videoHandlerYUV, we call the frameHandler::getPixelValues
  // function.
  virtual QStringPairList getPixelValues(const QPoint &pixelPos,
                                         int           frameIdx,
                                         frameHandler *item2     = nullptr,
                                         const int     frameIdx1 = 0) override;

  // Overload from playlistItemVideo. Calculate the difference of this playlistItemYuvSource
  // to another playlistItemVideo. If item2 cannot be converted to a playlistItemYuvSource,
  // we will use the playlistItemVideo::calculateDifference function to calculate the difference
  // using the RGB values.
  virtual QImage calculateDifference(frameHandler *   item2,
                                     const int        frameIdxItem0,
                                     const int        frameIdxItem1,
                                     QList<infoItem> &differenceInfoList,
                                     const int        amplificationFactor,
                                     const bool       markDifference) override;

  // Get the number of bytes for one YUV frame with the current format
  virtual int64_t getBytesPerFrame() const override
  {
    return srcPixelFormat.bytesPerFrame(frameSize);
  }

  // If you know the frame size of the video, the file size (and optionally the bit depth) we can
  // guess the remaining values. The rate value is set if a matching format could be found.
  virtual void setFormatFromSizeAndName(const Size       size,
                                        int              bitDepth,
                                        bool             packed,
                                        int64_t          fileSize,
                                        const QFileInfo &fileInfo) override;

  // Try to guess and set the format (frameSize/srcPixelFormat) from the raw YUV data.
  // If a file size is given, it is tested if the YUV format and the file size match.
  virtual void setFormatFromCorrelation(const QByteArray &rawYUVData,
                                        int64_t           fileSize = -1) override;

  virtual QString getFormatAsString() const override
  {
    return frameHandler::getFormatAsString() + ";YUV;" +
           QString::fromStdString(this->srcPixelFormat.getName());
  }
  virtual bool setFormatFromString(QString format) override;

  // Create the YUV controls and return a pointer to the layout.
  // yuvFormatFixed: For example a YUV file does not have a fixed format (the user can change this),
  // other sources might provide a fixed format which the user cannot change (HEVC file, ...)
  virtual QLayout *createVideoHandlerControls(bool isSizeFixed = false) override;

  // Get the name of the currently selected YUV pixel format
  virtual QString getRawYUVPixelFormatName() const
  {
    return QString::fromStdString(srcPixelFormat.getName());
  }
  // Set the current YUV format and update the control. Only emit a signalHandlerChanged signal
  // if emitSignal is true.
  virtual void setYUVPixelFormat(const YUV_Internals::YUVPixelFormat &fmt, bool emitSignal = false);
  virtual void setYUVPixelFormatByName(const QString &name, bool emitSignal = false)
  {
    this->setYUVPixelFormat(YUV_Internals::YUVPixelFormat(name.toStdString()), emitSignal);
  }
  virtual void setYUVColorConversion(YUV_Internals::ColorConversion conversion);

  // When loading a videoHandlerYUV from playlist file, this can be used to set all the parameters
  // at once
  void loadValues(Size frameSize, const QString &sourcePixelFormat);

  // Draw the pixel values of the visible pixels in the center of each pixel. Only draw values for
  // the given range of pixels. Overridden from playlistItemVideo. This is a YUV source, so we can
  // draw the YUV values.
  virtual void drawPixelValues(QPainter *    painter,
                               const int     frameIdx,
                               const QRect & videoRect,
                               const double  zoomFactor,
                               frameHandler *item2          = nullptr,
                               const bool    markDifference = false,
                               const int     frameIdxItem1  = 0) override;

  // Load the given frame and convert it to image. After this, currentFrameRawYUVData and
  // currentFrame will contain the frame with the given frame index.
  virtual void loadFrame(int frameIndex, bool loadToDoubleBuffer = false) override;

  // If this is set, the pixel values drawn in the drawPixels function will be scaled according to
  // the bit depth. E.g: The bit depth is 8 and the pixel value is 127, then the value shown will be
  // -1.
  bool showPixelValuesAsDiff{false};

  QByteArray getDiffYUV() const;

  YUV_Internals::YUVPixelFormat getDiffYUVFormat() const;

  bool getIs_YUV_diff() const;

protected:
  // How do we perform interpolation for the subsampled YUV formats?
  YUV_Internals::ChromaInterpolation chromaInterpolation{
      YUV_Internals::ChromaInterpolation::NearestNeighbor};

  // Which components should we display
  typedef enum
  {
    DisplayAll,
    DisplayY,
    DisplayCb,
    DisplayCr
  } ComponentDisplayMode;
  ComponentDisplayMode componentDisplayMode;

  // How to convert from YUV to RGB
  // How to convert from YUV to RGB
  /*
  kr/kg/kb matrix (Rec. ITU-T H.264 03/2010, p. 379):
  R = Y                  + V*(1-Kr)
  G = Y - U*(1-Kb)*Kb/Kg - V*(1-Kr)*Kr/Kg
  B = Y + U*(1-Kb)
  To respect value range of Y in [16:235] and U/V in [16:240], the matrix entries need to be scaled
  by 255/219 for Y and 255/112 for U/V In this software color conversion is performed with 16bit
  precision. Thus, further scaling with 2^16 is performed to get all factors as integers.
  */
  YUV_Internals::ColorConversion yuvColorConversionType;

  // Parameters for the YUV transformation (like scaling, invert, offset). For Luma ([0]) and
  // chroma([1]).
  QMap<YUV_Internals::Component, YUV_Internals::MathParameters> mathParameters;

  // The currently selected YUV format
  YUV_Internals::YUVPixelFormat srcPixelFormat;

  virtual yuv_t getPixelValue(const QPoint &pixelPos) const;

  // Load the given frame and return it for caching. The current buffers (currentFrameRawYUVData and
  // currentFrame) will not be modified.
  virtual void loadFrameForCaching(int frameIndex, QImage &frameToCache) override;

private:
  // Load the raw YUV data for the given frame index into currentFrameRawYUVData.
  // Return false is loading failed.
  bool loadRawYUVData(int frameIndex);

  // Convert from YUV (which ever format is selected) to image (RGB-888)
  void convertYUVToImage(const QByteArray &                   sourceBuffer,
                         QImage &                             outputImage,
                         const YUV_Internals::YUVPixelFormat &yuvFormat,
                         const Size &                         curFrameSize);

  // Set the new pixel format thread save (lock the mutex). We should also emit that something
  // changed (can be disabled).
  void setSrcPixelFormat(YUV_Internals::YUVPixelFormat newFormat, bool emitChangedSignal = true);
  // Check the given format against the file size. Set the format if this is a match.
  bool checkAndSetFormat(const YUV_Internals::YUVPixelFormat format,
                         const Size                          frameSize,
                         const int64_t                       fileSize);

  bool setFormatFromSizeAndNamePlanar(QString                    name,
                                      const Size                 size,
                                      int                        bitDepth,
                                      YUV_Internals::Subsampling subsampling,
                                      int64_t                    fileSize);
  bool setFormatFromSizeAndNamePacked(QString                    name,
                                      const Size                 size,
                                      int                        bitDepth,
                                      YUV_Internals::Subsampling subsampling,
                                      int64_t                    fileSize);

#if SSE_CONVERSION
  bool convertYUV420ToRGB(const byteArrayAligned &sourceBuffer, byteArrayAligned &targetBuffer);
#else
  bool convertYUV420ToRGB(const QByteArray &                  sourceBuffer,
                          unsigned char *                     targetBuffer,
                          const Size                          size,
                          const YUV_Internals::YUVPixelFormat format);
#endif

  bool convertYUVPlanarToRGB(const QByteArray &                   sourceBuffer,
                             unsigned char *                      targetBuffer,
                             const Size                           frameSize,
                             const YUV_Internals::YUVPixelFormat &sourceBufferFormat) const;
  bool markDifferencesYUVPlanarToRGB(const QByteArray &                   sourceBuffer,
                                     unsigned char *                      targetBuffer,
                                     const Size                           frameSize,
                                     const YUV_Internals::YUVPixelFormat &sourceBufferFormat) const;

#if SSE_CONVERSION_420_ALT
  void yuv420_to_argb8888(quint8 *yp,
                          quint8 *up,
                          quint8 *vp,
                          quint32 sy,
                          quint32 suv,
                          int     width,
                          int     height,
                          quint8 *rgb,
                          quint32 srgb);
#endif

  SafeUi<Ui::videoHandlerYUV> ui;

  bool                          is_YUV_diff;
  QByteArray                    diffYUV;
  YUV_Internals::YUVPixelFormat diffYUVFormat;

  QList<YUV_Internals::YUVPixelFormat> presetList;

private slots:

  // All the valueChanged() signals from the controls are connected here.
  void slotYUVControlChanged();
  // The YUV format combo box was changed
  void slotYUVFormatControlChanged(int idx);
};
