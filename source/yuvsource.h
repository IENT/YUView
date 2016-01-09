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

#ifndef YUVSOURCE_H
#define YUVSOURCE_H

#include "typedef.h"
#include <map>
#include <QString>
#include <QSize>
#include <QList>

/** Virtual class.
  * The YUVSource can be anything that provides raw YUV data. This can be a file or any kind of decoder or maybe a network source ...
  * A YUV sources supports handling of YUV data and can return a specific frame as a pixmap by calling getOneFrame.
  * So this class can perform all conversions from YUV to RGB.
  */
class yuvSource
{
public:
  typedef enum
  {
    NearestNeighborInterpolation,
    BiLinearInterpolation,
    InterstitialInterpolation
  } InterpolationMode;

  typedef enum
  {
    DisplayAll,
    DisplayY,
    DisplayCb,
    DisplayCr
  } ComponentDisplayMode;

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

  explicit yuvSource();
  virtual ~yuvSource();

  // Get/set the size of each frame (width, height in pixels) 
  // If unknown, an invalud QSize will be returned.
  virtual QSize getYUVFrameSize() { return frameSize; }
  
  // The format is valid if the frame width/height/pixel format are set
  virtual bool isFormatValid() { return (frameSize.isValid() && srcPixelFormat != "Unknown Pixel Format"); }

  // reads one frame in YUV444 into target byte array
  //virtual void getOneFrame(QByteArray &targetByteArray, unsigned int frameIdx) = 0;

  // Get the number of bytes for one YUV frame with the current format
  virtual qint64 getBytesPerYUVFrame() { return srcPixelFormat.bytesPerFrame(frameSize); }

  // Set/get interpolation mode (conversion function from YUV to RGB)
  void setInterpolationMode(InterpolationMode newMode) { interpolationMode = newMode; }
  InterpolationMode getInterpolationMode() { return interpolationMode; }

 /// ------- YUV format list handling -----

  struct yuvPixelFormat
  {
    // The default constructor (will create an "Unknown Pixel Format")
    yuvPixelFormat() : name("Unknown Pixel Format") , bitsPerSample(0) , bitsPerPixelDenominator(0)
                   , subsamplingHorizontal(0), subsamplingVertical(0), planar(false), bytePerComponentSample(1) {}
    // Convenience constructor that takes all the values.
    yuvPixelFormat(QString name, int bitsPerSample, int bitsPerPixelNominator, int bitsPerPixelDenominator, 
                   int subsamplingHorizontal, int subsamplingVertical, bool planar, int bytePerComponentSample = 1) 
                   : name(name) , bitsPerSample(bitsPerSample) , bitsPerPixelDenominator(bitsPerPixelDenominator)
                   , subsamplingHorizontal(subsamplingHorizontal), subsamplingVertical(subsamplingVertical)
                   , planar(planar), bytePerComponentSample(bytePerComponentSample) {}
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
    
protected:
  // YUV to RGB conversion
  yuvPixelFormat          srcPixelFormat;
  InterpolationMode       interpolationMode;
  ComponentDisplayMode    componentDisplayMode;
  YUVCColorConversionType yuvColorConversionType;

  int lumaScale, lumaOffset, chromaScale, chromaOffset;
  bool lumaInvert, chromaInvert;

  // Convert one frame from p_srcPixelFormat to YUV444 
  void convert2YUV444(QByteArray &sourceBuffer, int lumaWidth, int lumaHeight, QByteArray &targetBuffer);

  QSize frameSize;

  // Temporaray buffer for conversion to YUV444
  QByteArray tmpBufferYUV;
};

#endif