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

class yuvPixelFormat
{
public:
  yuvPixelFormat()
  {
    name = "";
    bitsPerSample = 8;
    bitsPerPixelNominator = 32;
    bitsPerPixelDenominator = 1;
    subsamplingHorizontal = 1;
    subsamplingVertical = 1;
    bytePerComponentSample = 1;
    planar = true;
  }

  // Get the number of bytes for a frame with the given size and this yuvPixelFormat
  qint64 bytesPerFrame(QSize frameSize);

  void setParams(QString name, int bitsPerSample, int bitsPerPixelNominator, int bitsPerPixelDenominator, int subsamplingHorizontal, int subsamplingVertical, bool isPlanar, int bytePerComponent = 1)
  {
    this->name = name;
    this->bitsPerSample = bitsPerSample;
    this->bitsPerPixelNominator = bitsPerPixelNominator;
    this->bitsPerPixelDenominator = bitsPerPixelDenominator;
    this->subsamplingHorizontal = subsamplingHorizontal;
    this->subsamplingVertical = subsamplingVertical;
    planar = isPlanar;
    bytePerComponentSample = bytePerComponent;
  }

  QString name;
  int bitsPerSample;
  int bitsPerPixelNominator;
  int bitsPerPixelDenominator;
  int subsamplingHorizontal;
  int subsamplingVertical;
  int bytePerComponentSample;
  bool planar;
};

typedef std::map<YUVCPixelFormatType, yuvPixelFormat> PixelFormatMapType;

/** Virtual class.
  * The YUVSource can be anything that provides raw YUV data. This can be a file or any kind of decoder or maybe a network source ...
  * A YUV sources supports handling of YUV data and can return a specific frame as a pixmap by calling getOneFrame.
  * So this class can perform all conversions from YUV to RGB.
  */
class yuvSource
{
public:
  explicit yuvSource();
  virtual ~yuvSource();

  // Get/set the size of each frame (width, height in pixels) 
  // If unknown, an invalud QSize will be returned.
  virtual QSize getYUVFrameSize() { return frameSize; }
  virtual void  setYUVFrameSize(QSize size) { frameSize = size; }
  
  // Set/get the yuv pixel format
  virtual void setYUVPixelFormat(YUVCPixelFormatType pixelFormat);
  YUVCPixelFormatType getPixelFormat() { return srcPixelFormat; }

  // The format is valid if the frame width/height/pixel format are set
  virtual bool isFormatValid() { return (frameSize.isValid() && srcPixelFormat != YUVC_UnknownPixelFormat); }

  // reads one frame in YUV444 into target byte array
  //virtual void getOneFrame(QByteArray &targetByteArray, unsigned int frameIdx) = 0;

  // Get the number of bytes for one YUV frame with the current format
  virtual qint64 getBytesPerYUVFrame() { return (isFormatValid()) ? bytesPerFrame( getYUVFrameSize(), getPixelFormat() ) : -1; }

  // Set/get interpolation mode (conversion function from YUV to RGB)
  void setInterpolationMode(InterpolationMode newMode) { interpolationMode = newMode; }
  InterpolationMode getInterpolationMode() { return interpolationMode; }

  // Static functions for handling of pixel formats

  // Get a list of all supported YUV formats
  static PixelFormatMapType pixelFormatList();

  static int verticalSubSampling(YUVCPixelFormatType pixelFormat);
  static int horizontalSubSampling(YUVCPixelFormatType pixelFormat);
  static int bitsPerSample(YUVCPixelFormatType pixelFormat);
  static qint64 bytesPerFrame(QSize frameSize, YUVCPixelFormatType pixelFormat);
  static bool isPlanar(YUVCPixelFormatType pixelFormat);
  static int  bytePerComponent(YUVCPixelFormatType pixelFormat);
  
  static PixelFormatMapType g_pixelFormatList;
    
protected:
  // YUV to RGB conversion
  YUVCPixelFormatType srcPixelFormat;
  InterpolationMode interpolationMode;

  // Convert one frame from p_srcPixelFormat to YUV444 
  void convert2YUV444(QByteArray &sourceBuffer, int lumaWidth, int lumaHeight, QByteArray &targetBuffer);

  QSize frameSize;

  // Temporaray buffer for conversion to YUV444
  QByteArray tmpBufferYUV;
};

#endif