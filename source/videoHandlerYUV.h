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

#ifndef VIDEOHANDLERYUV_H
#define VIDEOHANDLERYUV_H

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
#include "ui_videoHandlerYUV.h"

/** The videoHandlerYUV can be used in any playlistItem to read/display YUV data. A playlistItem could even provide multiple YUV videos.
  * A videoHandlerYUV supports handling of YUV data and can return a specific frame as a pixmap by calling getOneFrame.
  * All conversions from the various YUV formats to RGB are performed and hadeled here. 
  */
class videoHandlerYUV : public videoHandler
{
  Q_OBJECT

public:
  videoHandlerYUV();
  virtual ~videoHandlerYUV();

  // The format is valid if the frame width/height/pixel format are set
  virtual bool isFormatValid() Q_DECL_OVERRIDE { return (frameHandler::isFormatValid() && srcPixelFormat != "Unknown Pixel Format"); }

  // Return the YUV values for the given pixel
  // If a second item is provided, return the difference values to that item at the given position. If th second item
  // cannot be cast to a videoHandlerYUV, we call the frameHandler::getPixelValues function.
  virtual ValuePairList getPixelValues(QPoint pixelPos, int frameIdx, frameHandler *item2=NULL) Q_DECL_OVERRIDE;
  
  // Overload from playlistItemVideo. Calculate the difference of this playlistItemYuvSource
  // to another playlistItemVideo. If item2 cannot be converted to a playlistItemYuvSource,
  // we will use the playlistItemVideo::calculateDifference function to calculate the difference
  // using the RGB values.
  virtual QPixmap calculateDifference(frameHandler *item2, int frame, QList<infoItem> &conversionInfoList, int amplificationFactor, bool markDifference) Q_DECL_OVERRIDE;

  // Get the number of bytes for one YUV frame with the current format
  virtual qint64 getBytesPerFrame() { return srcPixelFormat.bytesPerFrame(frameSize); }

  // If you know the frame size of the video, the file size (and optionally the bit depth) we can guess
  // the remaining values. The rate value is set if a matching format could be found.
  // If the sub format is "444" we will assume 4:4:4 input. Otherwise 4:2:0 will be assumed.
  virtual void setFormatFromSize(QSize size, int bitDepth, qint64 fileSize, QString subFormat) Q_DECL_OVERRIDE;

  // Try to guess and set the format (frameSize/srcPixelFormat) from the raw YUV data.
  // If a file size is given, it is tested if the YUV format and the file size match.
  virtual void setFormatFromCorrelation(QByteArray rawYUVData, qint64 fileSize=-1);

  // Create the yuv controls and return a pointer to the layout.
  // yuvFormatFixed: For example a YUV file does not have a fixed format (the user can change this),
  // other sources might provide a fixed format which the user cannot change (HEVC file, ...)
  virtual QLayout *createYUVVideoHandlerControls(QWidget *parentWidget, bool isSizeFixed=false);

  // Get the name of the currently selected YUV pixel format
  virtual QString getRawYUVPixelFormatName() { return srcPixelFormat.name; }
  // Set the current yuv format and update the control. Only emit a signalHandlerChanged signal
  // if emitSignal is true.
  virtual void setYUVPixelFormatByName(QString name, bool emitSignal=false);

  // When loading a videoHandlerYUV from playlist file, this can be used to set all the parameters at once
  void loadValues(QSize frameSize, QString sourcePixelFormat);

  // Draw the pixel values of the visible pixels in the center of each pixel. Only draw values for the given range of pixels.
  // Overridden from playlistItemVideo. This is a YUV source, so we can draw the YUV values.
  virtual void drawPixelValues(QPainter *painter, int frameIdx, QRect videoRect, double zoomFactor, frameHandler *item2=NULL) Q_DECL_OVERRIDE;

  // The Frame size is about to change. If this happens, our local buffers all need updating.
  virtual void setFrameSize(QSize size, bool emitSignal = false) Q_DECL_OVERRIDE ;

  // The buffer of the raw YUV data of the current frame (and its frame index)
  // Before using the currentFrameRawYUVData, you have to check if the currentFrameRawYUVData_frameIdx is correct. If not,
  // you have to call loadFrame(idx) to load the frame and set it correctly.
  QByteArray currentFrameRawYUVData;
  int        currentFrameRawYUVData_frameIdx;

  // A buffer with the raw YUV data (this is filled if signalRequesRawData() is emitted)
  QByteArray rawYUVData;
  int        rawYUVData_frameIdx;

  // Invalidate all YUV related buffers. Then call the videoHandler::invalidateAllBuffers() function
  virtual void invalidateAllBuffers() Q_DECL_OVERRIDE;

signals:
  
  // This signal is emitted when the handler needs the raw data for a specific frame. After the signal
  // is emitted, the requested data should be in rawData and rawData_frameIdx should be identical to
  // frameIndex.
  void signalRequesRawData(int frameIndex);

protected:

  // The interpolation mode for conversion from non YUV444 to YUV 444
  typedef enum
  {
    NearestNeighborInterpolation,
    BiLinearInterpolation,
    InterstitialInterpolation
  } InterpolationMode;
  InterpolationMode interpolationMode;

  // Which components should we display
  typedef enum
  {
    DisplayAll,
    DisplayY,
    DisplayCb,
    DisplayCr
  } ComponentDisplayMode;
  ComponentDisplayMode    componentDisplayMode;

  // How to convert from YUV to RGB
  /*
  kr/kg/kb matrix (Rec. ITU-T H.264 03/2010, p. 379):
  R = Y                  + V*(1-Kr)
  G = Y - U*(1-Kb)*Kb/Kg - V*(1-Kr)*Kr/Kg
  B = Y + U*(1-Kb)

  To respect value range of Y in [16:235] and U/V in [16:240], the matrix entries need to be scaled by 255/219 for Y and 255/112 for U/V
  In this software color conversion is performed with 16bit precision. Thus, further scaling with 2^16 is performed to get all factors as integers.
  */
  typedef enum {
    YUVC709ColorConversionType,
    YUVC601ColorConversionType,
    YUVC2020ColorConversionType
  } YUVCColorConversionType;
  YUVCColorConversionType yuvColorConversionType;

    // This struct defines a specific yuv format with all properties like pixels per sample, subsampling of chroma
  // components and so on.
  struct yuvPixelFormat
  {
    // The default constructor (will create an "Unknown Pixel Format")
    yuvPixelFormat() : name("Unknown Pixel Format"), bitsPerSample(0), bitsPerPixelNominator(0), bitsPerPixelDenominator(0),
                       subsamplingHorizontal(0), subsamplingVertical(0), planar(false), bytePerComponentSample(0) {}
    // Convenience constructor that takes all the values.
    yuvPixelFormat(QString name, int bitsPerSample, int bitsPerPixelNominator, int bitsPerPixelDenominator,
                   int subsamplingHorizontal, int subsamplingVertical, bool planar, int bytePerComponentSample = 1)
                   : name(name) , bitsPerSample(bitsPerSample), bitsPerPixelNominator(bitsPerPixelNominator)
                   , bitsPerPixelDenominator(bitsPerPixelDenominator), subsamplingHorizontal(subsamplingHorizontal)
                   , subsamplingVertical(subsamplingVertical), planar(planar), bytePerComponentSample(bytePerComponentSample) {}
    bool operator==(const yuvPixelFormat& a) const { return name == a.name; } // Comparing names should be enough since you are not supposed to create your own yuvPixelFormat instances anyways.
    bool operator!=(const yuvPixelFormat& a) const { return name != a.name; }
    bool operator==(const QString& a) const { return name == a; }
    bool operator!=(const QString& a) const { return name != a; }
    // Get the number of bytes for a frame with this yuvPixelFormat and the given size
    qint64 bytesPerFrame( QSize frameSize );
    QString name;
    int bitsPerSample;
    int bitsPerPixelNominator;
    int bitsPerPixelDenominator;
    int subsamplingHorizontal;
    int subsamplingVertical;
    bool planar;
    int bytePerComponentSample;
  };

  // The currently selected yuv format
  yuvPixelFormat srcPixelFormat;

  // A (static) convenience QList class that handels all supported yuvPixelFormats
  class YUVFormatList : public QList<yuvPixelFormat>
  {
  public:
    // Default constructor. Fill the list with all the supported YUV formats.
    YUVFormatList();
    // Get all the YUV formats as a formatted list (for the dropdonw control)
    QStringList getFormatedNames();
    // Get the yuvPixelFormat with the given name
    yuvPixelFormat getFromName(QString name);
  };
  static YUVFormatList yuvFormatList;

  // Parameters for the YUV transformation (like scaling, invert, offset)
  int lumaScale, lumaOffset, chromaScale, chromaOffset;
  bool lumaInvert, chromaInvert;

  // Temporaray buffers for intermediate conversions
#if SSE_CONVERSION
  byteArrayAligned tmpBufferYUV444;
  byteArrayAligned tmpBufferRGB;
#else
  QByteArray tmpBufferYUV444;
  QByteArray tmpBufferRGB;
  // Caching
  QByteArray tmpBufferYUV444Caching;
  QByteArray tmpBufferRGBCaching;
  QByteArray tmpBufferRawYUVDataCaching;
#endif

  // Get the YUV values for the given pixel.
  virtual void getPixelValue(QPoint pixelPos, int frameIdx, unsigned int &Y, unsigned int &U, unsigned int &V);

  // Load the given frame and convert it to pixmap. After this, currentFrameRawYUVData and currentFrame will
  // contain the frame with the given frame index.
  virtual void loadFrame(int frameIndex) Q_DECL_OVERRIDE;

  // Load the given frame and return it for caching. The current buffers (currentFrameRawYUVData and currentFrame)
  // will not be modified.
  virtual void loadFrameForCaching(int frameIndex, QPixmap &frameToCache) Q_DECL_OVERRIDE;

  // Do we need to apply any transform to the raw YUV data before conversion to RGB?
  bool yuvMathRequired() { return lumaScale != 1 || lumaOffset != 125 || chromaScale != 1 || chromaOffset != 128 || lumaInvert || chromaInvert || componentDisplayMode != DisplayAll; }

private:

  // Load the raw YUV data for the given frame index into currentFrameRawYUVData.
  // Return false is loading failed.
  bool loadRawYUVData(int frameIndex);

  // Convert from YUV (which ever format is selected) to pixmap (RGB-888)
  void convertYUVToPixmap(QByteArray sourceBuffer, QPixmap &outputPixmap, QByteArray &tmpRGBBuffer, QByteArray &tmpYUV444Buffer);

  // Set the new pixel format thread save (lock the mutex)
  void setSrcPixelFormat( yuvPixelFormat newFormat ) { yuvFormatMutex.lock(); srcPixelFormat = newFormat; yuvFormatMutex.unlock(); }

#if SSE_CONVERSION
  // Convert one frame from the current pixel format to YUV444
  void convert2YUV444(byteArrayAligned &sourceBuffer, byteArrayAligned &targetBuffer);
  // Apply transformations to the luma/chroma components
  void applyYUVTransformation(byteArrayAligned &sourceBuffer);
  // Convert one frame from YUV 444 to RGB
  void convertYUV4442RGB(byteArrayAligned &sourceBuffer, byteArrayAligned &targetBuffer);
  // Directly convert from YUV 420 to RGB (do not apply YUV math)
  void convertYUV420ToRGB(byteArrayAligned &sourceBuffer, byteArrayAligned &targetBuffer);
#else
  // Convert one frame from the current pixel format to YUV444
  void convert2YUV444(QByteArray &sourceBuffer, QByteArray &targetBuffer);
  // Apply transformations to the luma/chroma components
  void applyYUVTransformation(QByteArray &sourceBuffer);
  // Convert one frame from YUV 444 to RGB
  void convertYUV4442RGB(QByteArray &sourceBuffer, QByteArray &targetBuffer);
  // Directly convert from YUV 420 to RGB (do not apply YUV math) (use the given size if valid)
  void convertYUV420ToRGB(QByteArray &sourceBuffer, QByteArray &targetBuffer, QSize size=QSize());
#endif

#if SSE_CONVERSION_420_ALT
  void yuv420_to_argb8888(quint8 *yp, quint8 *up, quint8 *vp,
                          quint32 sy, quint32 suv,
                           int width, int height,
                           quint8 *rgb, quint32 srgb );
#endif

  bool controlsCreated;    ///< Have the controls been created already?

  // When a caching job is running in the background it will lock this mutex, so that
  // the main thread does not change the yuv format while this is happening.
  QMutex yuvFormatMutex;

  Ui::videoHandlerYUV *ui;

private slots:

  // All the valueChanged() signals from the controls are connected here.
  void slotYUVControlChanged();

};

#endif // VIDEOHANDLERYUV_H
