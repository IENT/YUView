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

#ifndef VIDEOHANDLERRGB_H
#define VIDEOHANDLERRGB_H

#include "typedef.h"
#include <map>
#include <QString>
#include <QSize>
#include <QList>
#include <QPixmap>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QMutex>
#include "videoHandler.h"
#include "ui_videoHandlerRGB.h"
#include "ui_videoHandlerRGB_CustomFormatDialog.h"

class videoHandlerRGB_CustomFormatDialog : public QDialog, public Ui::CustomRGBFormatDialog
{
  Q_OBJECT
public:
  videoHandlerRGB_CustomFormatDialog(QString rgbFormat, int bitDepth, bool planar, bool alpha);
  QString getRGBFormat() { return rgbOrderComboBox->currentText(); }
  int getBitDepth() { return bitDepthSpinBox->value(); }
  bool getPlanar() { return planarCheckBox->isChecked(); }
  bool getAlphaChannel() { return alphaChannelCheckBox->isChecked(); }
};

/** The videoHandlerRGB can be used in any playlistItem to read/display RGB data. A playlistItem could even provide multiple RGB videos.
  * A videoHandlerRGB supports handling of RGB data and can return a specific frame as a pixmap by calling getOneFrame.
  * All conversions from the various raw RGB formats to pixmap are performed and hadeled here. 
  */
class videoHandlerRGB : public videoHandler
{
  Q_OBJECT

public:
  videoHandlerRGB();
  virtual ~videoHandlerRGB();

  // The format is valid if the frame width/height/pixel format are set
  virtual bool isFormatValid() Q_DECL_OVERRIDE { return (frameSize.isValid() && srcPixelFormat != "Unknown Pixel Format"); }

  // Return the RGB values for the given pixel
  virtual ValuePairList getPixelValues(QPoint pixelPos) Q_DECL_OVERRIDE;

  // Get the number of bytes for one RGB frame with the current format
  virtual qint64 getBytesPerFrame() Q_DECL_OVERRIDE { return srcPixelFormat.bytesPerFrame(frameSize); }

  // Try to guess and set the format (frameSize/srcPixelFormat) from the raw RGB data.
  // If a file size is given, it is tested if the RGB format and the file size match.
  virtual void setFormatFromCorrelation(QByteArray rawRGBData, qint64 fileSize=-1) Q_DECL_OVERRIDE {};

  // Create the rgb controls and return a pointer to the layout.
  // rgbFormatFixed: For example a RGB file does not have a fixed format (the user can change this),
  // other sources might provide a fixed format which the user cannot change.
  virtual QLayout *createVideoHandlerControls(QWidget *parentWidget, bool isSizeFixed=false);

  // Get the name of the currently selected RGB pixel format
  virtual QString getRawSrcPixelFormatName() Q_DECL_OVERRIDE { return srcPixelFormat.getName(); }
  // Set the current raw format and update the control. Only emit a signalHandlerChanged signal
  // if emitSignal is true.
  virtual void setSrcPixelFormatByName(QString name, bool emitSignal=false) Q_DECL_OVERRIDE { srcPixelFormat.setFromName(name); if (emitSignal) emit signalHandlerChanged(true, true); }
  
  // The Frame size is about to change. If this happens, our local buffers all need updating.
  virtual void setFrameSize(QSize size, bool emitSignal = false) Q_DECL_OVERRIDE ;

  // If you know the frame size of the video, the file size (and optionally the bit depth) we can guess
  // the remaining values. The rate value is set if a matching format could be found.
  // The sub format can be one of: "RGB", "GBR" or "BGR"
  virtual void setFormatFromSize(QSize size, int bitDepth, qint64 fileSize, QString subFormat) Q_DECL_OVERRIDE;

  // Draw the pixel values of the visible pixels in the center of each pixel. Only draw values for the given range of pixels.
  // Overridden from playlistItemVideo. This is a RGB source, so we can draw the source RGB values from the source data.
  virtual void drawPixelValues(QPainter *painter, unsigned int xMin, unsigned int xMax, unsigned int yMin, unsigned int yMax, double zoomFactor, videoHandler *item2=NULL) Q_DECL_OVERRIDE;

  // The buffer of the raw RGB data of the current frame (and its frame index)
  // Before using the currentFrameRawRGBData, you have to check if the currentFrameRawRGBData_frameIdx is correct. If not,
  // you have to call loadFrame(idx) to load the frame and set it correctly.
  QByteArray currentFrameRawRGBData;
  int        currentFrameRawRGBData_frameIdx;

  // Overload from playlistItemVideo. Calculate the difference of this videoHandlerRGB
  // to another videoHandlerRGB. If item2 cannot be converted to a videoHandlerRGB,
  // we will use the videoHandler::calculateDifference function to calculate the difference
  // using the 8bit RGB values.
  virtual QPixmap calculateDifference(videoHandler *item2, int frame, QList<infoItem> &conversionInfoList, int amplificationFactor, bool markDifference) Q_DECL_OVERRIDE { /* TODO */ return QPixmap(); }
  
protected:

  // Which components should we display
  typedef enum
  {
    DisplayAll,
    DisplayR,
    DisplayG,
    DisplayB
  } ComponentDisplayMode;
  ComponentDisplayMode componentDisplayMode;

  // This struct defines a specific rgb format with all properties like order of R/G/B, bitsPerValue, planarity...
  class rgbPixelFormat
  {
  public:
    // The default constructor (will create an "Unknown Pixel Format")
    rgbPixelFormat() : posR(0), posG(1), posB(2), bitsPerValue(0), planar(false), alphaChannel(false) {}
    // Convenience constructor that takes all the values.
    rgbPixelFormat(int bitsPerValue, bool planar, bool alphaChannel, int posR=0, int posG=1, int posB=2)
                   : posR(posR), posG(posG), posB(posB), bitsPerValue(bitsPerValue), planar(planar), alphaChannel(alphaChannel) 
    { Q_ASSERT_X(posR != posG && posR != posB && posG != posB, "rgbPixelFormat", "Invalid RGB format set"); }
    bool operator==(const rgbPixelFormat& a) const { return getName() == a.getName(); } // Comparing names should be enough since you are not supposed to create your own rgbPixelFormat instances anyways.
    bool operator!=(const rgbPixelFormat& a) const { return getName()!= a.getName(); }
    bool operator==(const QString& a) const { return getName() == a; }
    bool operator!=(const QString& a) const { return getName() != a; }
    bool isValid() { return bitsPerValue != 0 && posR != posG && posR != posB && posG != posB; }
    // Get a name representation of this item (this will be unique for the set parameters)
    QString getName() const;
    void setFromName(QString name);
    // Get/Set the RGB format from string (accepted string are: "RGB", "BGR", ...)
    QString getRGBFormatString() const;
    void setRGBFormatFromString(QString sFormat);
    // Get the number of bytes for a frame with this rgbPixelFormat and the given size
    qint64 bytesPerFrame( QSize frameSize );
    // The order of each component (E.g. for GBR this is posR=2,posG=0,posB=1)
    int posR, posG, posB;
    int bitsPerValue;
    bool planar;
    bool alphaChannel;
  };

  // A (static) convenience QList class that handels the preset rgbPixelFormats
  class RGBFormatList : public QList<rgbPixelFormat>
  {
  public:
    // Default constructor. Fill the list with all the supported YUV formats.
    RGBFormatList();
    // Get all the YUV formats as a formatted list (for the dropdonw control)
    QStringList getFormatedNames();
    // Get the yuvPixelFormat with the given name
    rgbPixelFormat getFromName(QString name);
  };
  static RGBFormatList rgbPresetList;

  QStringList orderRGBList;
  QStringList bitDepthList;

  // The currently selected rgb format
  rgbPixelFormat srcPixelFormat;

  // Parameters for the RGB transformation (like scaling, invert)
  int  componentScale[3];
  bool componentInvert[3];
  
  // Get the RGB values for the given pixel.
  virtual void getPixelValue(QPoint pixelPos, unsigned int &R, unsigned int &G, unsigned int &B);

  // Load the given frame and convert it to pixmap. After this, currentFrameRawRGBData and currentFrame will
  // contain the frame with the given frame index.
  virtual void loadFrame(int frameIndex) Q_DECL_OVERRIDE;

  // Load the given frame and return it for caching. The current buffers (currentFrameRawRGBData and currentFrame)
  // will not be modified.
  virtual void loadFrameForCaching(int frameIndex, QPixmap &frameToCache) Q_DECL_OVERRIDE;
    
private:

  // Load the raw RGB data for the given frame index into currentFrameRawRGBData.
  // Return false is loading failed.
  bool loadRawRGBData(int frameIndex);

  // Convert from RGB (which ever format is selected) to pixmap (RGB-888)
  void convertRGBToPixmap(QByteArray sourceBuffer, QPixmap &outputPixmap, QByteArray &tmpRGBBuffer);

  // Set the new pixel format thread save (lock the mutex)
  void setSrcPixelFormat( rgbPixelFormat newFormat ) { cachingMutex.lock(); srcPixelFormat = newFormat; cachingMutex.unlock(); }

#if SSE_CONVERSION
  // TODO - SSE RGB conversion
  byteArrayAligned tmpBufferRGB;
  byteArrayAligned tmpBufferRGBCaching;
  byteArrayAligned tmpBufferRawRGBDataCaching;
#else
  // Convert one frame from the current pixel format to RGB888
  void convertSourceToRGB888(QByteArray &sourceBuffer, QByteArray &targetBuffer);
  QByteArray tmpBufferRGB;
  QByteArray tmpBufferRGBCaching;
  QByteArray tmpBufferRawRGBDataCaching;
#endif

  // Have the controls been created already?
  bool controlsCreated;

  // When a caching job is running in the background it will lock this mutex, so that
  // the main thread does not change the rgb format while this is happening.
  QMutex cachingMutex;

  Ui::videoHandlerRGB *ui;

private slots:

  // One of the controls for the RGB format changed.
  void slotRGBFormatControlChanged();
  // One of the controls for the rgb display settings changed.
  void slotDisplayOptionsChanged();

};

#endif // VIDEOHANDLERRGB_H
