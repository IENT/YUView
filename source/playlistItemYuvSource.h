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

#ifndef PLAYLISTITEMYUVSOURCE_H
#define PLAYLISTITEMYUVSOURCE_H

#include "typedef.h"
#include <map>
#include <QString>
#include <QSize>
#include <QList>
#include <QPixmap>
#include <QCheckBox>
#include <QVBoxLayout>

#include "source/playlistItemVideo.h"
#include "ui_playlistItemYUVSource.h"

/** Virtual class.
  * The YUVSource can be anything that provides raw YUV data. This can be a file or any kind of decoder or maybe a network source ...
  * A YUV sources supports handling of YUV data and can return a specific frame as a pixmap by calling getOneFrame.
  * So this class can perform all conversions from YUV to RGB.
  */
class playlistItemYuvSource : public playlistItemVideo, Ui_playlistItemYUVSource
{
  Q_OBJECT

public:

  explicit playlistItemYuvSource(QString itemNameOrFileName);
  virtual ~playlistItemYuvSource();
    
  // The format is valid if the frame width/height/pixel format are set
  virtual bool isFormatValid() { return (frameSize.isValid() && srcPixelFormat != "Unknown Pixel Format"); }

  virtual ValuePairList getPixelValues(QPoint pixelPos) Q_DECL_OVERRIDE;

  // Overload from playlistItemVideo. Calculate the difference of this playlistItemYuvSource 
  // to another playlistItemVideo. If item2 cannot be converted to a playlistItemYuvSource,
  // we will use the playlistItemVideo::calculateDifference function to calculate the difference
  // using the RGB values.
  virtual QPixmap calculateDifference(playlistItemVideo *item2, int frame, QList<infoItem> &conversionInfoList) Q_DECL_OVERRIDE;
  // For the difference item: Return values of this item, the other item and the difference at
  // the given pixel position. Call playlistItemVideo::getPixelValuesDifference if the given
  // item cannot be cast to a playlistItemYuvSource.
  virtual ValuePairList getPixelValuesDifference(playlistItemVideo *item2, QPoint pixelPos) Q_DECL_OVERRIDE;
    
protected:

  // Get the number of bytes for one YUV frame with the current format
  virtual qint64 getBytesPerYUVFrame() { return srcPixelFormat.bytesPerFrame(frameSize); }

  // Create the yuv controls and return a pointer to the layout. This can be used by 
  // inherited classes to create a properties widget.
  // yuvFormatFixed: For example a YUV file does not have a fixed format (the user can change this),
  // other sources might provide a fixed format which the user cannot change (HEVC file, ...)
  virtual QLayout *createVideoControls(bool yuvFormatFixed=false);

  // The interpolation mode for conversion from non YUV444 to YUV 444
  typedef enum
  {
    NearestNeighborInterpolation,
    BiLinearInterpolation,
    InterstitialInterpolation
  } InterpolationMode;
  InterpolationMode       interpolationMode;

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
    yuvPixelFormat() : name("Unknown Pixel Format") , bitsPerSample(0) , bitsPerPixelDenominator(0)
                   , subsamplingHorizontal(0), subsamplingVertical(0), planar(false), bytePerComponentSample(1) {}
    // Convenience constructor that takes all the values.
    yuvPixelFormat(QString name, int bitsPerSample, int bitsPerPixelNominator, int bitsPerPixelDenominator, 
                   int subsamplingHorizontal, int subsamplingVertical, bool planar, int bytePerComponentSample = 1) 
                   : name(name) , bitsPerSample(bitsPerSample), bitsPerPixelNominator(bitsPerPixelNominator)
                   , bitsPerPixelDenominator(bitsPerPixelDenominator), subsamplingHorizontal(subsamplingHorizontal)
                   , subsamplingVertical(subsamplingVertical), planar(planar), bytePerComponentSample(bytePerComponentSample) {}
    bool operator==(const yuvPixelFormat& a) const { return name == a.name; } // Comparing names should be enough since you are not supposed to create your own yuvPixelFormat instances anyways.
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
  QByteArray tmpBufferYUV444;
  QByteArray tmpBufferRGB;

  // 
  void convertYUVBufferToPixmap(QByteArray &sourceBuffer, QPixmap &targetPixmap);

private:

  // Convert one frame from the current pixel format to YUV444 
  void convert2YUV444(QByteArray &sourceBuffer, QByteArray &targetBuffer);
  // Apply transformations to the luma/chroma components
  void applyYUVTransformation(QByteArray &sourceBuffer);
  // Convert one frame from YUV 444 to RGB
  void convertYUV4442RGB(QByteArray &sourceBuffer, QByteArray &targetBuffer);

private slots:

  // All the valueChanged() signals from the controls are connected here.
  void slotYUVControlChanged();

};

#endif // PLAYLISTITEMYUVSOURCE_H
