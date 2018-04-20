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

#ifndef VIDEOHANDLERRGB_H
#define VIDEOHANDLERRGB_H

#include "ui_videoHandlerRGB.h"
#include "ui_videoHandlerRGB_CustomFormatDialog.h"
#include "videoHandler.h"

class videoHandlerRGB_CustomFormatDialog : public QDialog, private Ui::CustomRGBFormatDialog
{
  Q_OBJECT

public:
  videoHandlerRGB_CustomFormatDialog(const QString &rgbFormat, int bitDepth, bool planar, bool alpha);
  QString getRGBFormat() const { return rgbOrderComboBox->currentText(); }
  int getBitDepth() const { return bitDepthSpinBox->value(); }
  bool getPlanar() const { return planarCheckBox->isChecked(); }
  bool getAlphaChannel() const { return alphaChannelCheckBox->isChecked(); }
};

/** The videoHandlerRGB can be used in any playlistItem to read/display RGB data. A playlistItem could even provide multiple RGB videos.
  * A videoHandlerRGB supports handling of RGB data and can return a specific frame as a image by calling getOneFrame.
  * All conversions from the various raw RGB formats to image are performed and handled here.
  */
class videoHandlerRGB : public videoHandler
{
  Q_OBJECT

public:
  videoHandlerRGB();
  virtual ~videoHandlerRGB();

  // The format is valid if the frame width/height/pixel format are set
  virtual bool isFormatValid() const Q_DECL_OVERRIDE { return (frameHandler::isFormatValid() && srcPixelFormat != "Unknown Pixel Format"); }

  // Return the RGB values for the given pixel
  virtual ValuePairList getPixelValues(const QPoint &pixelPos, int frameIdx, frameHandler *item2, const int frameIdx1 = 0) Q_DECL_OVERRIDE;

  // Get the number of bytes for one RGB frame with the current format
  virtual qint64 getBytesPerFrame() const { return srcPixelFormat.bytesPerFrame(frameSize); }

  // Try to guess and set the format (frameSize/srcPixelFormat) from the raw RGB data.
  // If a file size is given, it is tested if the RGB format and the file size match.
  virtual void setFormatFromCorrelation(const QByteArray &rawRGBData, qint64 fileSize=-1) Q_DECL_OVERRIDE { /* TODO */ Q_UNUSED(rawRGBData); Q_UNUSED(fileSize); }

  // Create the RGB controls and return a pointer to the layout.
  // rgbFormatFixed: For example a RGB file does not have a fixed format (the user can change this),
  // other sources might provide a fixed format which the user cannot change.
  virtual QLayout *createRGBVideoHandlerControls(bool isSizeFixed=false);

  // Get the name of the currently selected RGB pixel format
  virtual QString getRawRGBPixelFormatName() const { return srcPixelFormat.getName(); }
  // Set the current raw format and update the control. Only emit a signalHandlerChanged signal
  // if emitSignal is true.
  virtual void setRGBPixelFormatByName(const QString &name, bool emitSignal=false) { srcPixelFormat.setFromName(name); if (emitSignal) emit signalHandlerChanged(true, RECACHE_NONE); }

  // The Frame size is about to change. If this happens, our local buffers all need updating.
  virtual void setFrameSize(const QSize &size) Q_DECL_OVERRIDE ;

  // If you know the frame size of the video, the file size (and optionally the bit depth) we can guess
  // the remaining values. The rate value is set if a matching format could be found.
  // The sub format can be one of: "RGB", "GBR" or "BGR"
  virtual void setFormatFromSizeAndName(const QSize size, int bitDepth, qint64 fileSize, const QFileInfo &fileInfo) Q_DECL_OVERRIDE;

  // Draw the pixel values of the visible pixels in the center of each pixel. Only draw values for the given range of pixels.
  // Overridden from playlistItemVideo. This is a RGB source, so we can draw the source RGB values from the source data.
  virtual void drawPixelValues(QPainter *painter, const int frameIdx, const QRect &videoRect, const double zoomFactor, frameHandler *item2 = nullptr, const bool markDifference = false, const int frameIdxItem1 = 0) Q_DECL_OVERRIDE;

  // The buffer of the raw RGB data of the current frame (and its frame index)
  // Before using the currentFrameRawRGBData, you have to check if the currentFrameRawRGBData_frameIdx is correct. If not,
  // you have to call loadFrame() to load the frame and set it correctly.
  QByteArray currentFrameRawRGBData;
  int        currentFrameRawRGBData_frameIdx;

  // Overload from playlistItemVideo. Calculate the difference of this videoHandlerRGB
  // to another videoHandlerRGB. If item2 cannot be converted to a videoHandlerRGB,
  // we will use the videoHandler::calculateDifference function to calculate the difference
  // using the 8bit RGB values.
  virtual QImage calculateDifference(frameHandler *item2, const int frameIdxItem0, const int frameIdxItem1, QList<infoItem> &differenceInfoList, const int amplificationFactor, const bool markDifference) Q_DECL_OVERRIDE;

  // A buffer with the raw RGB data (this is filled if signalRequestRawData() is emitted)
  QByteArray rawRGBData;
  int        rawRGBData_frameIdx;

  // Invalidate all RGB related buffers. Then call the videoHandler::invalidateAllBuffers() function
  virtual void invalidateAllBuffers() Q_DECL_OVERRIDE;

  // Load the given frame and convert it to image. After this, currentFrameRawRGBData and currentFrame will
  // contain the frame with the given frame index.
  virtual void loadFrame(int frameIndex, bool loadToDoubleBuffer=false) Q_DECL_OVERRIDE;

signals:

  // This signal is emitted when the handler needs the raw data for a specific frame. After the signal
  // is emitted, the requested data should be in rawData and rawData_frameIdx should be identical to
  // frameIndex. caching will signal if this call comes from a caching thread or not. If it does come
  // from a caching thread, the result must be ready when the call to this function returns.
  void signalRequestRawData(int frameIndex, bool caching);

protected:

  // Check if the current buffer for the raw RGB data (currentFrameRawYUVData) is up to date for the given frame index
  virtual itemLoadingState needsLoadingRawValues(int frameIdx) Q_DECL_OVERRIDE { return (currentFrameRawRGBData_frameIdx == frameIdx) ? LoadingNotNeeded : LoadingNeeded; }

  // Which components should we display
  typedef enum
  {
    DisplayAll,
    DisplayR,
    DisplayG,
    DisplayB
  } ComponentDisplayMode;
  ComponentDisplayMode componentDisplayMode;

  // This class defines a specific RGB format with all properties like order of R/G/B, bitsPerValue, planarity...
  class rgbPixelFormat
  {
  public:
    // The default constructor (will create an "Unknown Pixel Format")
    rgbPixelFormat() : posR(0), posG(1), posB(2), bitsPerValue(0), planar(false), alphaChannel(false) {}
    // Convenience constructor that takes all the values.
    rgbPixelFormat(int bitsPerValue, bool planar, bool alphaChannel, int posR=0, int posG=1, int posB=2)
                   : posR(posR), posG(posG), posB(posB), bitsPerValue(bitsPerValue), planar(planar), alphaChannel(alphaChannel)
    { Q_ASSERT_X(posR != posG && posR != posB && posG != posB, "rgbPixelFormat", "Invalid RGB format set"); }
    bool operator==(const rgbPixelFormat &a) const { return getName() == a.getName(); } // Comparing names should be enough since you are not supposed to create your own rgbPixelFormat instances anyways.
    bool operator!=(const rgbPixelFormat &a) const { return getName()!= a.getName(); }
    bool operator==(const QString &a) const { return getName() == a; }
    bool operator!=(const QString &a) const { return getName() != a; }
    bool isValid() { return bitsPerValue != 0 && posR != posG && posR != posB && posG != posB; }
    // Get a name representation of this item (this will be unique for the set parameters)
    QString getName() const;
    void setFromName(const QString &name);
    // Get/Set the RGB format from string (accepted string are: "RGB", "BGR", ...)
    QString getRGBFormatString() const;
    void setRGBFormatFromString(const QString &sFormat);
    // Get the number of bytes for a frame with this rgbPixelFormat and the given size
    qint64 bytesPerFrame(const QSize &frameSize) const;
    // The order of each component (E.g. for GBR this is posR=2,posG=0,posB=1)
    int posR, posG, posB;
    int bitsPerValue;
    bool planar;
    bool alphaChannel;
  };

  // A (static) convenience QList class that handles the preset rgbPixelFormats
  class RGBFormatList : public QList<rgbPixelFormat>
  {
  public:
    // Default constructor. Fill the list with all the supported YUV formats.
    RGBFormatList();
    // Get all the YUV formats as a formatted list (for the drop-down control)
    QStringList getFormattedNames() const;
    // Get the yuvPixelFormat with the given name
    rgbPixelFormat getFromName(const QString &name) const;
  };
  static RGBFormatList rgbPresetList;

  QStringList orderRGBList;
  QStringList bitDepthList;

  // The currently selected RGB format
  rgbPixelFormat srcPixelFormat;

  // Parameters for the RGB transformation (like scaling, invert)
  int  componentScale[3];
  bool componentInvert[3];

  // Get the RGB values for the given pixel.
  virtual void getPixelValue(const QPoint &pixelPos, unsigned int &R, unsigned int &G, unsigned int &B);

  // Load the given frame and return it for caching. The current buffers (currentFrameRawRGBData and currentFrame)
  // will not be modified.
  virtual void loadFrameForCaching(int frameIndex, QImage &frameToCache) Q_DECL_OVERRIDE;

private:

  // Load the raw RGB data for the given frame index into currentFrameRawRGBData.
  // Return false is loading failed.
  bool loadRawRGBData(int frameIndex);

  // Convert from RGB (which ever format is selected) to a QImage in the platform QImage format (platformImageFormat)
  void convertRGBToImage(const QByteArray &sourceBuffer, QImage &outputImage);

  // Set the new pixel format thread save (lock the mutex)
  void setSrcPixelFormat(const rgbPixelFormat &newFormat) { rgbFormatMutex.lock(); srcPixelFormat = newFormat; rgbFormatMutex.unlock(); }

  // Convert one frame from the current pixel format to RGB888
  void convertSourceToRGBA32Bit(const QByteArray &sourceBuffer, unsigned char *targetBuffer);
  QByteArray tmpBufferRawRGBDataCaching;

  // When a caching job is running in the background it will lock this mutex, so that
  // the main thread does not change the RGB format while this is happening.
  QMutex rgbFormatMutex;

  SafeUi<Ui::videoHandlerRGB> ui;

private slots:

  // One of the controls for the RGB format changed.
  void slotRGBFormatControlChanged();
  // One of the controls for the RGB display settings changed.
  void slotDisplayOptionsChanged();

};

#endif // VIDEOHANDLERRGB_H
