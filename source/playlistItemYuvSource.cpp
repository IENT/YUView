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

#include "playlistItemYuvSource.h"
#include <QtEndian>
#include <QTime>
#include <QLabel>
#include <QGroupBox>
#include "stdio.h"
#include <QDebug>
#include <QPainter>

#define MIN(a,b) ((a)>(b)?(b):(a))
#define MAX(a,b) ((a)<(b)?(b):(a))

/* Get the number of bytes for a frame with this yuvPixelFormat and the given size
*/
qint64 playlistItemYuvSource::yuvPixelFormat::bytesPerFrame(QSize frameSize)
{
  if (name == "" || !frameSize.isValid())
    return 0;

  qint64 numSamples = frameSize.height() * frameSize.width();
  unsigned remainder = numSamples % bitsPerPixelDenominator;
  qint64 bits = numSamples / bitsPerPixelDenominator;
  if (remainder == 0) {
    bits *= bitsPerPixelNominator;
  }
  else {
    qDebug() << "warning: pixels not divisable by bpp denominator for pixel format: " << name << " - rounding up\n";
    bits = (bits + 1) * bitsPerPixelNominator;
  }
  if (bits % 8 != 0) {
    qDebug() << "warning: bits not divisible by 8 for pixel format: " << name << " - rounding up\n";
    bits += 8;
  }

  return bits / 8;
}

/* The default constructor of the YUVFormatList will fill the list with all supported YUV file formats.
 * Don't forget to implement actual support for all of them in the conversion functions.
*/
playlistItemYuvSource::YUVFormatList::YUVFormatList()
{
  append( playlistItemYuvSource::yuvPixelFormat()); // "Unknown Pixel Format"
  append( playlistItemYuvSource::yuvPixelFormat("GBR 12-bit planar", 12, 48, 1, 1, 1, true, 2) );
  append( playlistItemYuvSource::yuvPixelFormat("RGBA 8-bit", 8, 32, 1, 1, 1, false) );
  append( playlistItemYuvSource::yuvPixelFormat("RGB 8-bit", 8, 24, 1, 1, 1, false) );
  append( playlistItemYuvSource::yuvPixelFormat("BGR 8-bit", 8, 24, 1, 1, 1, false) );
  append( playlistItemYuvSource::yuvPixelFormat("4:4:4 Y'CbCr 16-bit LE planar", 16, 48, 1, 1, 1, true, 2) );
  append( playlistItemYuvSource::yuvPixelFormat("4:4:4 Y'CbCr 16-bit BE planar", 16, 48, 1, 1, 1, true, 2) );
  append( playlistItemYuvSource::yuvPixelFormat("4:4:4 Y'CbCr 12-bit LE planar", 12, 48, 1, 1, 1, true, 2) );
  append( playlistItemYuvSource::yuvPixelFormat("4:4:4 Y'CbCr 12-bit BE planar", 12, 48, 1, 1, 1, true, 2) );
  append( playlistItemYuvSource::yuvPixelFormat("4:4:4 Y'CbCr 10-bit LE planar", 10, 48, 1, 1, 1, true, 2) );
  append( playlistItemYuvSource::yuvPixelFormat("4:4:4 Y'CbCr 10-bit BE planar", 10, 48, 1, 1, 1, true, 2) );
  append( playlistItemYuvSource::yuvPixelFormat("4:4:4 Y'CbCr 8-bit planar", 8, 24, 1, 1, 1, true) );
  append( playlistItemYuvSource::yuvPixelFormat("4:4:4 Y'CrCb 8-bit planar", 8, 24, 1, 1, 1, true) );
  append( playlistItemYuvSource::yuvPixelFormat("4:2:2 Y'CbCr 8-bit planar", 8, 16, 1, 2, 1, true) );
  append( playlistItemYuvSource::yuvPixelFormat("4:2:2 Y'CrCb 8-bit planar", 8, 16, 1, 2, 1, true) );
  append( playlistItemYuvSource::yuvPixelFormat("4:2:2 8-bit packed", 8, 16, 1, 2, 1, false) );
  append( playlistItemYuvSource::yuvPixelFormat("4:2:2 10-bit packed 'v210'", 10, 128, 6, 2, 1, false, 2) );
  append( playlistItemYuvSource::yuvPixelFormat("4:2:2 10-bit packed (UYVY)", 10, 128, 6, 2, 1, true, 2) );
  append( playlistItemYuvSource::yuvPixelFormat("4:2:0 Y'CbCr 10-bit LE planar", 10, 24, 1, 2, 2, true, 2) );
  append( playlistItemYuvSource::yuvPixelFormat("4:2:0 Y'CbCr 8-bit planar", 8, 12, 1, 2, 2, true) );
  append( playlistItemYuvSource::yuvPixelFormat("4:1:1 Y'CbCr 8-bit planar", 8, 12, 1, 4, 1, true) );
  append( playlistItemYuvSource::yuvPixelFormat("4:0:0 8-bit", 8, 8, 1, 0, 0, true) );
}

/* Put all the names of the yuvPixelFormats into a list and return it
*/
QStringList playlistItemYuvSource::YUVFormatList::getFormatedNames()
{
  QStringList l;
  for (int i = 0; i < count(); i++)
  {
    l.append( at(i).name );
  }
  return l;
}

playlistItemYuvSource::yuvPixelFormat playlistItemYuvSource::YUVFormatList::getFromName(QString name)
{
  for (int i = 0; i < count(); i++)
  {
    if ( at(i) == name )
      return at(i);
  }
  // If the format could not be found, we return the "Unknown Pixel Format" format (which is index 0)
  return at(0);
}

// Initialize the static yuvFormatList
playlistItemYuvSource::YUVFormatList playlistItemYuvSource::yuvFormatList;

playlistItemYuvSource::playlistItemYuvSource(QString itemNameOrFileName) : playlistItemVideo(itemNameOrFileName)
{
  // preset internal values
  srcPixelFormat = yuvFormatList.getFromName("Unknown Pixel Format");
  interpolationMode = NearestNeighborInterpolation;
  componentDisplayMode = DisplayAll;
  yuvColorConversionType = YUVC709ColorConversionType;
  lumaScale = 1;
  lumaOffset = 125;
  chromaScale = 1;
  chromaOffset = 128;
  lumaInvert = false;
  chromaInvert = false;
}

playlistItemYuvSource::~playlistItemYuvSource()
{

}

void playlistItemYuvSource::convertYUVBufferToPixmap(QByteArray &sourceBuffer, QPixmap &targetPixmap)
{
  // First, convert the buffer to YUV 444
  tmpBufferOriginal = sourceBuffer;
  convert2YUV444(sourceBuffer, tmpBufferYUV444);

  // Apply transformations to the YUV components (if any are set)
  // TODO: Shouldn't this be done before the conversion to 444?
  applyYUVTransformation( tmpBufferYUV444 );

  // Convert to RGB888
  convertYUV4442RGB(tmpBufferYUV444, tmpBufferRGB);

  // Convert the image in tmpBufferRGB to a QPixmap using a QImage intermediate.
  // TODO: Isn't there a faster way to do this? Maybe load a pixmap from "BMP"-like data?
  QImage tmpImage((unsigned char*)tmpBufferRGB.data(), frameSize.width(), frameSize.height(), QImage::Format_RGB888);

  // TODO: if targetPixmap gets deleted, because the YUVFile is deleted, this gives a nasty segmentation fault
  targetPixmap.convertFromImage(tmpImage);
}

void playlistItemYuvSource::getPixelValue(QPoint pixelPos, int &Y, int &U, int &V)
{
  // Get the YUV data from the tmpBufferOriginal
  const unsigned int offsetCoordinateY  = frameSize.width() * pixelPos.y() + pixelPos.x();
  const unsigned int offsetCoordinateUV = (frameSize.width() / srcPixelFormat.subsamplingHorizontal * pixelPos.y() / srcPixelFormat.subsamplingVertical) + pixelPos.x() / srcPixelFormat.subsamplingHorizontal;
  const unsigned int planeLengthY  = frameSize.width() * frameSize.height();
  const unsigned int planeLengthUV = frameSize.width() / srcPixelFormat.subsamplingHorizontal * frameSize.height() / srcPixelFormat.subsamplingVertical;
  if (srcPixelFormat.bitsPerSample > 8)
  {
    // Two bytes per value
    char* poi = tmpBufferOriginal.data();
    unsigned short* point = (unsigned short*) poi;
    Y = point[offsetCoordinateY];
    U = point[planeLengthY + offsetCoordinateUV];
    V = point[planeLengthY + planeLengthUV + offsetCoordinateUV];
  }
  else
  {
    // One byte per value
    unsigned char *poi = (unsigned char*)tmpBufferOriginal.data();
    Y = poi[offsetCoordinateY];
    U = poi[planeLengthY + offsetCoordinateUV];
    V = poi[planeLengthY + planeLengthUV + offsetCoordinateUV];
  }
}

ValuePairList playlistItemYuvSource::getPixelValues(QPoint pixelPos)
{
  // TODO: For now we get the YUV values from the converted YUV444 array. This is correct as long 
  // as we use sample and hold interpolation. However, for all other kinds of U/V interpolation
  // this is wrong! This function should directly load the values from the source format.

  int Y,U,V;
  getPixelValue(pixelPos, Y, U, V);

  ValuePairList values;

  values.append( ValuePair("Y", QString::number(Y)) );
  values.append( ValuePair("U", QString::number(U)) );
  values.append( ValuePair("V", QString::number(V)) );

  return values;
}

/// --- Convert from the current YUV input format to YUV 444

inline quint32 SwapInt32(quint32 arg) {
  quint32 result;
  result = ((arg & 0xFF) << 24) | ((arg & 0xFF00) << 8) | ((arg >> 8) & 0xFF00) | ((arg >> 24) & 0xFF);
  return result;
}

inline quint16 SwapInt16(quint16 arg) {
  quint16 result;
  result = (quint16)(((arg << 8) & 0xFF00) | ((arg >> 8) & 0xFF));
  return result;
}

inline quint32 SwapInt32BigToHost(quint32 arg) {
#if __BIG_ENDIAN__ || IS_BIG_ENDIAN
  return arg;
#else
  return SwapInt32(arg);
#endif
}

inline quint32 SwapInt32LittleToHost(quint32 arg) {
#if __LITTLE_ENDIAN__ || IS_LITTLE_ENDIAN
  return arg;
#else
  return SwapInt32(arg);
#endif
}

inline quint16 SwapInt16LittleToHost(quint16 arg) {
#if __LITTLE_ENDIAN__
  return arg;
#else
  return SwapInt16(arg);
#endif
}

void playlistItemYuvSource::convert2YUV444(QByteArray &sourceBuffer, QByteArray &targetBuffer)
{
  if (srcPixelFormat == "Unknown Pixel Format") {
    // Unknown format. We cannot convert this.
    return;
  }

  const int componentWidth = frameSize.width();
  const int componentHeight = frameSize.height();
  // TODO: make this compatible with 10bit sequences
  const int componentLength = componentWidth * componentHeight; // number of bytes per luma frames
  const int horiSubsampling = srcPixelFormat.subsamplingHorizontal;
  const int vertSubsampling = srcPixelFormat.subsamplingVertical;
  const int chromaWidth = horiSubsampling == 0 ? 0 : componentWidth / horiSubsampling;
  const int chromaHeight = vertSubsampling == 0 ? 0 : componentHeight / vertSubsampling;
  const int chromaLength = chromaWidth * chromaHeight; // number of bytes per chroma frame
  // make sure target buffer is big enough (YUV444 means 3 byte per sample)
  int targetBufferLength = 3 * componentWidth * componentHeight * srcPixelFormat.bytePerComponentSample;
  if (targetBuffer.size() != targetBufferLength)
    targetBuffer.resize(targetBufferLength);

  // TODO: keep unsigned char for 10bit? use short?
  if (chromaLength == 0) {
    const unsigned char *srcY = (unsigned char*)sourceBuffer.data();
    unsigned char *dstY = (unsigned char*)targetBuffer.data();
    unsigned char *dstU = dstY + componentLength;
    memcpy(dstY, srcY, componentLength);
    memset(dstU, 128, 2 * componentLength);
  }
  else if (srcPixelFormat == "4:2:2 8-bit packed") {
    const unsigned char *srcY = (unsigned char*)sourceBuffer.data();
    unsigned char *dstY = (unsigned char*)targetBuffer.data();
    unsigned char *dstU = dstY + componentLength;
    unsigned char *dstV = dstU + componentLength;

    int y;
#pragma omp parallel for default(none) shared(dstY,dstU,dstV,srcY)
    for (y = 0; y < componentHeight; y++) {
      for (int x = 0; x < componentWidth; x++) {
        dstY[x + y*componentWidth] = srcY[((x + y*componentWidth) << 1) + 1];
        dstU[x + y*componentWidth] = srcY[((((x >> 1) << 1) + y*componentWidth) << 1)];
        dstV[x + y*componentWidth] = srcY[((((x >> 1) << 1) + y*componentWidth) << 1) + 2];
      }
    }
  }
  else if (srcPixelFormat == "4:2:2 10-bit packed (UYVY)") {
    const quint32 *srcY = (quint32*)sourceBuffer.data();
    quint16 *dstY = (quint16*)targetBuffer.data();
    quint16 *dstU = dstY + componentLength;
    quint16 *dstV = dstU + componentLength;

    int i;
#define BIT_INCREASE 6
#pragma omp parallel for default(none) shared(dstY,dstU,dstV,srcY)
    for (i = 0; i < ((componentLength + 5) / 6); i++) {
      const int srcPos = i * 4;
      const int dstPos = i * 6;
      quint32 srcVal;
      srcVal = SwapInt32BigToHost(srcY[srcPos]);
      dstV[dstPos] = dstV[dstPos + 1] = (srcVal & 0xffc00000) >> (22 - BIT_INCREASE);
      dstY[dstPos] = (srcVal & 0x003ff000) >> (12 - BIT_INCREASE);
      dstU[dstPos] = dstU[dstPos + 1] = (srcVal & 0x00000ffc) << (BIT_INCREASE - 2);
      srcVal = SwapInt32BigToHost(srcY[srcPos + 1]);
      dstY[dstPos + 1] = (srcVal & 0xffc00000) >> (22 - BIT_INCREASE);
      dstV[dstPos + 2] = dstV[dstPos + 3] = (srcVal & 0x003ff000) >> (12 - BIT_INCREASE);
      dstY[dstPos + 2] = (srcVal & 0x00000ffc) << (BIT_INCREASE - 2);
      srcVal = SwapInt32BigToHost(srcY[srcPos + 2]);
      dstU[dstPos + 2] = dstU[dstPos + 3] = (srcVal & 0xffc00000) >> (22 - BIT_INCREASE);
      dstY[dstPos + 3] = (srcVal & 0x003ff000) >> (12 - BIT_INCREASE);
      dstV[dstPos + 4] = dstV[dstPos + 5] = (srcVal & 0x00000ffc) << (BIT_INCREASE - 2);
      srcVal = SwapInt32BigToHost(srcY[srcPos + 3]);
      dstY[dstPos + 4] = (srcVal & 0xffc00000) >> (22 - BIT_INCREASE);
      dstU[dstPos + 4] = dstU[dstPos + 5] = (srcVal & 0x003ff000) >> (12 - BIT_INCREASE);
      dstY[dstPos + 5] = (srcVal & 0x00000ffc) << (BIT_INCREASE - 2);
    }
  }
  else if (srcPixelFormat == "4:2:2 10-bit packed 'v210'") {
    const quint32 *srcY = (quint32*)sourceBuffer.data();
    quint16 *dstY = (quint16*)targetBuffer.data();
    quint16 *dstU = dstY + componentLength;
    quint16 *dstV = dstU + componentLength;

    int i;
#define BIT_INCREASE 6
#pragma omp parallel for default(none) shared(dstY,dstU,dstV,srcY)
    for (i = 0; i < ((componentLength + 5) / 6); i++) {
      const int srcPos = i * 4;
      const int dstPos = i * 6;
      quint32 srcVal;
      srcVal = SwapInt32LittleToHost(srcY[srcPos]);
      dstV[dstPos] = dstV[dstPos + 1] = (srcVal & 0x3ff00000) >> (20 - BIT_INCREASE);
      dstY[dstPos] = (srcVal & 0x000ffc00) >> (10 - BIT_INCREASE);
      dstU[dstPos] = dstU[dstPos + 1] = (srcVal & 0x000003ff) << BIT_INCREASE;
      srcVal = SwapInt32LittleToHost(srcY[srcPos + 1]);
      dstY[dstPos + 1] = (srcVal & 0x000003ff) << BIT_INCREASE;
      dstU[dstPos + 2] = dstU[dstPos + 3] = (srcVal & 0x000ffc00) >> (10 - BIT_INCREASE);
      dstY[dstPos + 2] = (srcVal & 0x3ff00000) >> (20 - BIT_INCREASE);
      srcVal = SwapInt32LittleToHost(srcY[srcPos + 2]);
      dstU[dstPos + 4] = dstU[dstPos + 5] = (srcVal & 0x3ff00000) >> (20 - BIT_INCREASE);
      dstY[dstPos + 3] = (srcVal & 0x000ffc00) >> (10 - BIT_INCREASE);
      dstV[dstPos + 2] = dstV[dstPos + 3] = (srcVal & 0x000003ff) << BIT_INCREASE;
      srcVal = SwapInt32LittleToHost(srcY[srcPos + 3]);
      dstY[dstPos + 4] = (srcVal & 0x000003ff) << BIT_INCREASE;
      dstV[dstPos + 4] = dstV[dstPos + 5] = (srcVal & 0x000ffc00) >> (10 - BIT_INCREASE);
      dstY[dstPos + 5] = (srcVal & 0x3ff00000) >> (20 - BIT_INCREASE);
    }
  }
  else if (srcPixelFormat == "4:2:0 Y'CbCr 8-bit planar" && interpolationMode == BiLinearInterpolation) {
    // vertically midway positioning - unsigned rounding
    const unsigned char *srcY = (unsigned char*)sourceBuffer.data();
    const unsigned char *srcU = srcY + componentLength;
    const unsigned char *srcV = srcU + chromaLength;
    const unsigned char *srcUV[2] = { srcU, srcV };
    unsigned char *dstY = (unsigned char*)targetBuffer.data();
    unsigned char *dstU = dstY + componentLength;
    unsigned char *dstV = dstU + componentLength;
    unsigned char *dstUV[2] = { dstU, dstV };

    const int dstLastLine = (componentHeight - 1)*componentWidth;
    const int srcLastLine = (chromaHeight - 1)*chromaWidth;

    memcpy(dstY, srcY, componentLength);

    int c;
    for (c = 0; c < 2; c++) {
      //NSLog(@"%i", omp_get_num_threads());
      // first line
      dstUV[c][0] = srcUV[c][0];
      int i;
#pragma omp parallel for default(none) shared(dstUV,srcUV) firstprivate(c)
      for (i = 0; i < chromaWidth - 1; i++) {
        dstUV[c][i * 2 + 1] = (((int)(srcUV[c][i]) + (int)(srcUV[c][i + 1]) + 1) >> 1);
        dstUV[c][i * 2 + 2] = srcUV[c][i + 1];
      }
      dstUV[c][componentWidth - 1] = dstUV[c][componentWidth - 2];

      int j;
#pragma omp parallel for default(none) shared(dstUV,srcUV) firstprivate(c)
      for (j = 0; j < chromaHeight - 1; j++) {
        const int dstTop = (j * 2 + 1)*componentWidth;
        const int dstBot = (j * 2 + 2)*componentWidth;
        const int srcTop = j*chromaWidth;
        const int srcBot = (j + 1)*chromaWidth;
        dstUV[c][dstTop] = ((3 * (int)(srcUV[c][srcTop]) + (int)(srcUV[c][srcBot]) + 2) >> 2);
        dstUV[c][dstBot] = (((int)(srcUV[c][srcTop]) + 3 * (int)(srcUV[c][srcBot]) + 2) >> 2);
        for (int i = 0; i < chromaWidth - 1; i++) {
          const int tl = srcUV[c][srcTop + i];
          const int tr = srcUV[c][srcTop + i + 1];
          const int bl = srcUV[c][srcBot + i];
          const int br = srcUV[c][srcBot + i + 1];
          dstUV[c][dstTop + i * 2 + 1] = ((6 * tl + 6 * tr + 2 * bl + 2 * br + 8) >> 4);
          dstUV[c][dstBot + i * 2 + 1] = ((2 * tl + 2 * tr + 6 * bl + 6 * br + 8) >> 4);
          dstUV[c][dstTop + i * 2 + 2] = ((3 * tr + br + 2) >> 2);
          dstUV[c][dstBot + i * 2 + 2] = ((tr + 3 * br + 2) >> 2);
        }
        dstUV[c][dstTop + componentWidth - 1] = dstUV[c][dstTop + componentWidth - 2];
        dstUV[c][dstBot + componentWidth - 1] = dstUV[c][dstBot + componentWidth - 2];
      }

      dstUV[c][dstLastLine] = srcUV[c][srcLastLine];
#pragma omp parallel for default(none) shared(dstUV,srcUV) firstprivate(c)
      for (i = 0; i < chromaWidth - 1; i++) {
        dstUV[c][dstLastLine + i * 2 + 1] = (((int)(srcUV[c][srcLastLine + i]) + (int)(srcUV[c][srcLastLine + i + 1]) + 1) >> 1);
        dstUV[c][dstLastLine + i * 2 + 2] = srcUV[c][srcLastLine + i + 1];
      }
      dstUV[c][dstLastLine + componentWidth - 1] = dstUV[c][dstLastLine + componentWidth - 2];
    }
  }
  else if (srcPixelFormat == "4:2:0 Y'CbCr 8-bit planar" && interpolationMode == InterstitialInterpolation) {
    // interstitial positioning - unsigned rounding, takes 2 times as long as nearest neighbour
    const unsigned char *srcY = (unsigned char*)sourceBuffer.data();
    const unsigned char *srcU = srcY + componentLength;
    const unsigned char *srcV = srcU + chromaLength;
    const unsigned char *srcUV[2] = { srcU, srcV };
    unsigned char *dstY = (unsigned char*)targetBuffer.data();
    unsigned char *dstU = dstY + componentLength;
    unsigned char *dstV = dstU + componentLength;
    unsigned char *dstUV[2] = { dstU, dstV };

    const int dstLastLine = (componentHeight - 1)*componentWidth;
    const int srcLastLine = (chromaHeight - 1)*chromaWidth;

    memcpy(dstY, srcY, componentLength);

    int c;
    for (c = 0; c < 2; c++) {
      // first line
      dstUV[c][0] = srcUV[c][0];

      int i;
#pragma omp parallel for default(none) shared(dstUV,srcUV) firstprivate(c)
      for (i = 0; i < chromaWidth - 1; i++) {
        dstUV[c][2 * i + 1] = ((3 * (int)(srcUV[c][i]) + (int)(srcUV[c][i + 1]) + 2) >> 2);
        dstUV[c][2 * i + 2] = (((int)(srcUV[c][i]) + 3 * (int)(srcUV[c][i + 1]) + 2) >> 2);
      }
      dstUV[c][componentWidth - 1] = srcUV[c][chromaWidth - 1];

      int j;
#pragma omp parallel for default(none) shared(dstUV,srcUV) firstprivate(c)
      for (j = 0; j < chromaHeight - 1; j++) {
        const int dstTop = (j * 2 + 1)*componentWidth;
        const int dstBot = (j * 2 + 2)*componentWidth;
        const int srcTop = j*chromaWidth;
        const int srcBot = (j + 1)*chromaWidth;
        dstUV[c][dstTop] = ((3 * (int)(srcUV[c][srcTop]) + (int)(srcUV[c][srcBot]) + 2) >> 2);
        dstUV[c][dstBot] = (((int)(srcUV[c][srcTop]) + 3 * (int)(srcUV[c][srcBot]) + 2) >> 2);
        for (int i = 0; i < chromaWidth - 1; i++) {
          const int tl = srcUV[c][srcTop + i];
          const int tr = srcUV[c][srcTop + i + 1];
          const int bl = srcUV[c][srcBot + i];
          const int br = srcUV[c][srcBot + i + 1];
          dstUV[c][dstTop + i * 2 + 1] = (9 * tl + 3 * tr + 3 * bl + br + 8) >> 4;
          dstUV[c][dstBot + i * 2 + 1] = (3 * tl + tr + 9 * bl + 3 * br + 8) >> 4;
          dstUV[c][dstTop + i * 2 + 2] = (3 * tl + 9 * tr + bl + 3 * br + 8) >> 4;
          dstUV[c][dstBot + i * 2 + 2] = (tl + 3 * tr + 3 * bl + 9 * br + 8) >> 4;
        }
        dstUV[c][dstTop + componentWidth - 1] = ((3 * (int)(srcUV[c][srcTop + chromaWidth - 1]) + (int)(srcUV[c][srcBot + chromaWidth - 1]) + 2) >> 2);
        dstUV[c][dstBot + componentWidth - 1] = (((int)(srcUV[c][srcTop + chromaWidth - 1]) + 3 * (int)(srcUV[c][srcBot + chromaWidth - 1]) + 2) >> 2);
      }

      dstUV[c][dstLastLine] = srcUV[c][srcLastLine];
#pragma omp parallel for default(none) shared(dstUV,srcUV) firstprivate(c)
      for (i = 0; i < chromaWidth - 1; i++) {
        dstUV[c][dstLastLine + i * 2 + 1] = ((3 * (int)(srcUV[c][srcLastLine + i]) + (int)(srcUV[c][srcLastLine + i + 1]) + 2) >> 2);
        dstUV[c][dstLastLine + i * 2 + 2] = (((int)(srcUV[c][srcLastLine + i]) + 3 * (int)(srcUV[c][srcLastLine + i + 1]) + 2) >> 2);
      }
      dstUV[c][dstLastLine + componentWidth - 1] = srcUV[c][srcLastLine + chromaWidth - 1];
    }
  } /*else if (pixelFormatType == YUVC_420YpCbCr8PlanarPixelFormat && self.chromaInterpolation == 3) {
    // interstitial positioning - correct signed rounding - takes 6/5 times as long as unsigned rounding
    const unsigned char *srcY = (unsigned char*)sourceBuffer->data();
    const unsigned char *srcU = srcY + componentLength;
    const unsigned char *srcV = srcU + chromaLength;
    unsigned char *dstY = (unsigned char*)targetBuffer->data();
    unsigned char *dstU = dstY + componentLength;
    unsigned char *dstV = dstU + componentLength;

    memcpy(dstY, srcY, componentLength);

    unsigned char *dstC = dstU;
    const unsigned char *srcC = srcU;
    int c;
    for (c = 0; c < 2; c++) {
    // first line
    unsigned char *endLine = dstC + componentWidth;
    unsigned char *endComp = dstC + componentLength;
    *dstC++ = *srcC;
    while (dstC < (endLine-1)) {
    *dstC++ = thresAddAndShift( 3*(int)(*srcC) +   (int)(*(srcC+1)), 512, 1, 2, 2);
    *dstC++ = thresAddAndShift(   (int)(*srcC) + 3*(int)(*(srcC+1)), 512, 1, 2, 2);
    srcC++;
    }
    *dstC++ = *srcC++;
    srcC -= chromaWidth;

    while (dstC < endComp - 2*componentWidth) {
    endLine = dstC + componentWidth;
    *(dstC)                = thresAddAndShift( 3*(int)(*srcC) +   (int)(*(srcC+chromaWidth)), 512, 1, 2, 2);
    *(dstC+componentWidth) = thresAddAndShift(   (int)(*srcC) + 3*(int)(*(srcC+chromaWidth)), 512, 1, 2, 2);
    dstC++;
    while (dstC < endLine-1) {
    int tl = (int)*srcC;
    int tr = (int)*(srcC+1);
    int bl = (int)*(srcC+chromaWidth);
    int br = (int)*(srcC+chromaWidth+1);
    *(dstC)                  = thresAddAndShift(9*tl + 3*tr + 3*bl +   br, 2048, 7, 8, 4);
    *(dstC+1)                = thresAddAndShift(3*tl + 9*tr +   bl + 3*br, 2048, 7, 8, 4);
    *(dstC+componentWidth)   = thresAddAndShift(3*tl +   tr + 9*bl + 3*br, 2048, 7, 8, 4);
    *(dstC+componentWidth+1) = thresAddAndShift(  tl + 3*tr + 3*bl + 9*br, 2048, 7, 8, 4);
    srcC++;
    dstC+=2;
    }
    *(dstC)                = thresAddAndShift( 3*(int)(*srcC) +   (int)(*(srcC+chromaWidth)), 512, 1, 2, 2);
    *(dstC+componentWidth) = thresAddAndShift(   (int)(*srcC) + 3*(int)(*(srcC+chromaWidth)), 512, 1, 2, 2);
    dstC++;
    srcC++;
    dstC += componentWidth;
    }

    endLine = dstC + componentWidth;
    *dstC++ = *srcC;
    while (dstC < (endLine-1)) {
    *dstC++ = thresAddAndShift( 3*(int)(*srcC) +   (int)(*(srcC+1)), 512, 1, 2, 2);
    *dstC++ = thresAddAndShift(   (int)(*srcC) + 3*(int)(*(srcC+1)), 512, 1, 2, 2);
    srcC++;
    }
    *dstC++ = *srcC++;

    dstC = dstV;
    srcC = srcV;
    }
    }*/ else if (srcPixelFormat.planar && srcPixelFormat.bitsPerSample == 8) {
    // sample and hold interpolation
    const bool reverseUV = (srcPixelFormat == "4:4:4 Y'CrCb 8-bit planar") || (srcPixelFormat == "4:2:2 Y'CrCb 8-bit planar");
    const unsigned char *srcY = (unsigned char*)sourceBuffer.data();
    const unsigned char *srcU = srcY + componentLength + (reverseUV ? chromaLength : 0);
    const unsigned char *srcV = srcY + componentLength + (reverseUV ? 0 : chromaLength);
    unsigned char *dstY = (unsigned char*)targetBuffer.data();
    unsigned char *dstU = dstY + componentLength;
    unsigned char *dstV = dstU + componentLength;
    int horiShiftTmp = 0;
    int vertShiftTmp = 0;
    while (((1 << horiShiftTmp) & horiSubsampling) != 0) horiShiftTmp++;
    while (((1 << vertShiftTmp) & vertSubsampling) != 0) vertShiftTmp++;
    const int horiShift = horiShiftTmp;
    const int vertShift = vertShiftTmp;

    memcpy(dstY, srcY, componentLength);

    if (2 == horiSubsampling && 2 == vertSubsampling) {
      int y;
#pragma omp parallel for default(none) shared(dstV,dstU,srcV,srcU)
      for (y = 0; y < chromaHeight; y++) {
        for (int x = 0; x < chromaWidth; x++) {
          dstU[2 * x + 2 * y*componentWidth] = dstU[2 * x + 1 + 2 * y*componentWidth] = srcU[x + y*chromaWidth];
          dstV[2 * x + 2 * y*componentWidth] = dstV[2 * x + 1 + 2 * y*componentWidth] = srcV[x + y*chromaWidth];
        }
        memcpy(&dstU[(2 * y + 1)*componentWidth], &dstU[(2 * y)*componentWidth], componentWidth);
        memcpy(&dstV[(2 * y + 1)*componentWidth], &dstV[(2 * y)*componentWidth], componentWidth);
      }
    }
    else if ((1 << horiShift) == horiSubsampling && (1 << vertShift) == vertSubsampling) {
      int y;
#pragma omp parallel for default(none) shared(dstV,dstU,srcV,srcU)
      for (y = 0; y < componentHeight; y++) {
        for (int x = 0; x < componentWidth; x++) {
          //dstY[x + y*componentWidth] = srcY[x + y*componentWidth];
          dstU[x + y*componentWidth] = srcU[(x >> horiShift) + (y >> vertShift)*chromaWidth];
          dstV[x + y*componentWidth] = srcV[(x >> horiShift) + (y >> vertShift)*chromaWidth];
        }
      }
    }
    else {
      int y;
#pragma omp parallel for default(none) shared(dstV,dstU,srcV,srcU)
      for (y = 0; y < componentHeight; y++) {
        for (int x = 0; x < componentWidth; x++) {
          //dstY[x + y*componentWidth] = srcY[x + y*componentWidth];
          dstU[x + y*componentWidth] = srcU[x / horiSubsampling + y / vertSubsampling*chromaWidth];
          dstV[x + y*componentWidth] = srcV[x / horiSubsampling + y / vertSubsampling*chromaWidth];
        }
      }
    }
  }
    else if (srcPixelFormat == "4:2:0 Y'CbCr 10-bit LE planar") {
      // TODO: chroma interpolation for 4:2:0 10bit planar
      const unsigned short *srcY = (unsigned short*)sourceBuffer.data();
      const unsigned short *srcU = srcY + componentLength;
      const unsigned short *srcV = srcU + chromaLength;
      unsigned short *dstY = (unsigned short*)targetBuffer.data();
      unsigned short *dstU = dstY + componentLength;
      unsigned short *dstV = dstU + componentLength;

      int y;
#pragma omp parallel for default(none) shared(dstY,dstV,dstU,srcY,srcV,srcU)
      for (y = 0; y < componentHeight; y++) {
        for (int x = 0; x < componentWidth; x++) {
          //dstY[x + y*componentWidth] = MIN(1023, CFSwapInt16LittleToHost(srcY[x + y*componentWidth])) << 6; // clip value for data which exceeds the 2^10-1 range
          //     dstY[x + y*componentWidth] = SwapInt16LittleToHost(srcY[x + y*componentWidth])<<6;
          //    dstU[x + y*componentWidth] = SwapInt16LittleToHost(srcU[x/2 + (y/2)*chromaWidth])<<6;
          //    dstV[x + y*componentWidth] = SwapInt16LittleToHost(srcV[x/2 + (y/2)*chromaWidth])<<6;

          dstY[x + y*componentWidth] = qFromLittleEndian(srcY[x + y*componentWidth]);
          dstU[x + y*componentWidth] = qFromLittleEndian(srcU[x / 2 + (y / 2)*chromaWidth]);
          dstV[x + y*componentWidth] = qFromLittleEndian(srcV[x / 2 + (y / 2)*chromaWidth]);
        }
      }
    }
    else if (srcPixelFormat == "4:4:4 Y'CbCr 12-bit BE planar"
      || srcPixelFormat == "4:4:4 Y'CbCr 16-bit BE planar")
    {
      // Swap the input data in 2 byte pairs.
      // BADC -> ABCD
      const char *src = (char*)sourceBuffer.data();
      char *dst = (char*)targetBuffer.data();
      int i;
#pragma omp parallel for default(none) shared(src,dst)
      for (i = 0; i < srcPixelFormat.bytesPerFrame( QSize(componentWidth, componentHeight) ); i+=2)
      {
        dst[i] = src[i + 1];
        dst[i + 1] = src[i];
      }
    }
    else if (srcPixelFormat == "4:4:4 Y'CbCr 10-bit LE planar")
    {
      const unsigned short *srcY = (unsigned short*)sourceBuffer.data();
      const unsigned short *srcU = srcY + componentLength;
      const unsigned short *srcV = srcU + componentLength;
      unsigned short *dstY = (unsigned short*)targetBuffer.data();
      unsigned short *dstU = dstY + componentLength;
      unsigned short *dstV = dstU + componentLength;
      int y;
#pragma omp parallel for default(none) shared(dstY,dstV,dstU,srcY,srcV,srcU)
      for (y = 0; y < componentHeight; y++)
      {
        for (int x = 0; x < componentWidth; x++)
        {
          dstY[x + y*componentWidth] = qFromLittleEndian(srcY[x + y*componentWidth]);
          dstU[x + y*componentWidth] = qFromLittleEndian(srcU[x + y*chromaWidth]);
          dstV[x + y*componentWidth] = qFromLittleEndian(srcV[x + y*chromaWidth]);
        }
      }

    }
    else if (srcPixelFormat == "4:4:4 Y'CbCr 10-bit BE planar")
    {
      const unsigned short *srcY = (unsigned short*)sourceBuffer.data();
      const unsigned short *srcU = srcY + componentLength;
      const unsigned short *srcV = srcU + chromaLength;
      unsigned short *dstY = (unsigned short*)targetBuffer.data();
      unsigned short *dstU = dstY + componentLength;
      unsigned short *dstV = dstU + componentLength;
      int y;
#pragma omp parallel for default(none) shared(dstY,dstV,dstU,srcY,srcV,srcU)
      for (y = 0; y < componentHeight; y++)
      {
        for (int x = 0; x < componentWidth; x++)
        {
          dstY[x + y*componentWidth] = qFromBigEndian(srcY[x + y*componentWidth]);
          dstU[x + y*componentWidth] = qFromBigEndian(srcU[x + y*chromaWidth]);
          dstV[x + y*componentWidth] = qFromBigEndian(srcV[x + y*chromaWidth]);
        }
      }

    }

    else {
      qDebug() << "Unhandled pixel format: " << srcPixelFormat.name << endl;
    }

    return;
}


void playlistItemYuvSource::applyYUVTransformation(QByteArray &sourceBuffer)
{
  if (lumaScale == 1 && lumaOffset == 125 && chromaScale == 1 && chromaOffset == 128 &&
      lumaInvert == false && chromaInvert == false)
    // Default values, nothing to cenvert/transform about the YUV values
    return;

  const int lumaLength = frameSize.width() * frameSize.height();
  const int singleChromaLength = lumaLength;
  const int chromaLength = 2*singleChromaLength;
  const int sourceBPS = srcPixelFormat.bitsPerSample;
  const int maxVal = (1<<sourceBPS)-1;
  const int chromaZero = (1<<(sourceBPS-1));

  // For now there is only one scale parameter for U and V. However, the transformation
  // function allows them to be treated seperately.
  int chromaUScale = chromaScale;
  int chromaVScale = chromaScale;

  typedef enum {
    YUVMathDefaultColors,
    YUVMathLumaOnly,
    YUVMathCbOnly,
    YUVMathCrOnly
  } YUVTransformationMode;

  YUVTransformationMode colorMode = YUVMathDefaultColors;

  if( lumaScale != 1 && chromaUScale == 1 && chromaUScale == 1 )
    colorMode = YUVMathLumaOnly;
  else if( lumaScale == 1 && chromaUScale != 1 && chromaVScale == 1 )
    colorMode = YUVMathCbOnly;
  else if( lumaScale == 1 && chromaUScale == 1 && chromaVScale != 1 )
    colorMode = YUVMathCrOnly;

  if (sourceBPS == 8)
  {
    const unsigned char *src = (const unsigned char*)sourceBuffer.data();
    unsigned char *dst = (unsigned char*)sourceBuffer.data();

    //int i;
    if (colorMode == YUVMathDefaultColors || colorMode == YUVMathLumaOnly)
    {
      int i;
#pragma omp parallel for default(none) shared(src,dst)
      for (i = 0; i < lumaLength; i++)
      {
        int newVal = lumaInvert ? (maxVal-(int)(src[i])):((int)(src[i]));
        newVal = (newVal - lumaOffset) * lumaScale + lumaOffset;
        newVal = MAX( 0, MIN( maxVal, newVal ) );
        dst[i] = (unsigned char)newVal;
      }
      dst += lumaLength;
    }
    src += lumaLength;

    for (int c = 0; c < 2; c++)
    {
      if (   colorMode == YUVMathDefaultColors
         || (colorMode == YUVMathCbOnly && c == 0)
         || (colorMode == YUVMathCrOnly && c == 1)
         )
      {
        int i;
        int cMultiplier = (c==0) ? chromaUScale : chromaVScale;
#pragma omp parallel for default(none) shared(src,dst,cMultiplier)
        for (i = 0; i < singleChromaLength; i++)
        {
          int newVal = chromaInvert ? (maxVal-(int)(src[i])):((int)(src[i]));
          newVal = (newVal - chromaOffset) * cMultiplier + chromaOffset;
          newVal = MAX( 0, MIN( maxVal, newVal ) );
          dst[i] = (unsigned char)newVal;
        }
        dst += singleChromaLength;
      }
      src += singleChromaLength;
    }
    if (colorMode != YUVMathDefaultColors)
    {
      // clear the chroma planes
      memset(dst, chromaZero, chromaLength);
    }
  }
  else if (sourceBPS>8 && sourceBPS<=16)
  {
    const unsigned short *src = (const unsigned short*)sourceBuffer.data();
    unsigned short *dst = (unsigned short*)sourceBuffer.data();
    //int i;
    if (colorMode == YUVMathDefaultColors || colorMode == YUVMathLumaOnly)
    {
      int i;
#pragma omp parallel for default(none) shared(src,dst)
      for (i = 0; i < lumaLength; i++) {
        int newVal = lumaInvert ? (maxVal-(int)(src[i])):((int)(src[i]));
        newVal = (newVal - lumaOffset) * lumaScale + lumaOffset;
        newVal = MAX( 0, MIN( maxVal, newVal ) );
        dst[i] = (unsigned short)newVal;
      }
      dst += lumaLength;
    }
    src += lumaLength;

    for (int c = 0; c < 2; c++) {
      if (   colorMode == YUVMathDefaultColors
         || (colorMode == YUVMathCbOnly && c == 0)
         || (colorMode == YUVMathCrOnly && c == 1)
         )
      {
        int i;
        int cMultiplier = (c==0) ? chromaUScale : chromaVScale;
#pragma omp parallel for default(none) shared(src,dst,cMultiplier)
        for (i = 0; i < singleChromaLength; i++)
        {
          int newVal = chromaInvert ? (maxVal-(int)(src[i])):((int)(src[i]));
          newVal = (newVal - chromaOffset) * cMultiplier + chromaOffset;
          newVal = MAX( 0, MIN( maxVal, newVal ) );
          dst[i] = (unsigned short)newVal;
        }
        dst += singleChromaLength;
      }
      src += singleChromaLength;
    }
    if (colorMode != YUVMathDefaultColors)
    {
      // clear the chroma planes
      int i;
      #pragma omp parallel for default(none) shared(dst)
      for (i = 0; i < chromaLength; i++)
      {
        dst[i] = chromaZero;
      }
    }
  }
  else
    qDebug() << "unsupported bitdepth, returning original data: " << sourceBPS << endl;
}

#if __STDC__ != 1
#    define restrict __restrict /* use implementation __ format */
#else
#    ifndef __STDC_VERSION__
#        define restrict __restrict /* use implementation __ format */
#    else
#        if __STDC_VERSION__ < 199901L
#            define restrict __restrict /* use implementation __ format */
#        else
#            /* all ok */
#        endif
#    endif
#endif

void playlistItemYuvSource::convertYUV4442RGB(QByteArray &sourceBuffer, QByteArray &targetBuffer)
{
  static unsigned char clp_buf[384+256+384];
  static unsigned char *clip_buf = clp_buf+384;
  static bool clp_buf_initialized = false;

  if (!clp_buf_initialized)
  {
    // Initialize clipping table. Because of the static bool, this will only be called once.
    memset(clp_buf, 0, 384);
    int i;
    for (i = 0; i < 256; i++)
      clp_buf[384+i] = i;
    memset(clp_buf+384+256, 255, 384);
    clp_buf_initialized = true;
  }

  const int bps = srcPixelFormat.bitsPerSample;

  // make sure target buffer is big enough
  int srcBufferLength = sourceBuffer.size();
  Q_ASSERT( srcBufferLength%3 == 0 ); // YUV444 has 3 bytes per pixel
  int componentLength = 0;
  //buffer size changes depending on the bit depth
  if(bps == 8)
  {
    componentLength = srcBufferLength/3;
    if( targetBuffer.size() != srcBufferLength)
      targetBuffer.resize(srcBufferLength);
  }
  else if(bps==10)
  {
    componentLength = srcBufferLength/6;
    if( targetBuffer.size() != srcBufferLength/2)
      targetBuffer.resize(srcBufferLength/2);
  }
  else
  {
    if( targetBuffer.size() != srcBufferLength)
      targetBuffer.resize(srcBufferLength);
  }

  const int yOffset = 16<<(bps-8);
  const int cZero = 128<<(bps-8);
  const int rgbMax = (1<<bps)-1;
  int yMult, rvMult, guMult, gvMult, buMult;

  unsigned char *dst = (unsigned char*)targetBuffer.data();

  if (bps == 8)
  {
    switch (yuvColorConversionType)
    {
      case YUVC601ColorConversionType:
        yMult =   76309;
        rvMult = 104597;
        guMult = -25675;
        gvMult = -53279;
        buMult = 132201;
        break;
      case YUVC2020ColorConversionType:
        yMult =   76309;
        rvMult = 110013;
        guMult = -12276;
        gvMult = -42626;
        buMult = 140363;
        break;
      case YUVC709ColorConversionType:
      default:
        yMult =   76309;
        rvMult = 117489;
        guMult = -13975;
        gvMult = -34925;
        buMult = 138438;
        break;
    }
    const unsigned char * restrict srcY = (unsigned char*)sourceBuffer.data();
    const unsigned char * restrict srcU = srcY + componentLength;
    const unsigned char * restrict srcV = srcU + componentLength;
    unsigned char * restrict dstMem = dst;

    int i;
#pragma omp parallel for default(none) private(i) shared(srcY,srcU,srcV,dstMem,yMult,rvMult,guMult,gvMult,buMult,clip_buf,componentLength)// num_threads(2)
    for (i = 0; i < componentLength; ++i)
    {
      const int Y_tmp = ((int)srcY[i] - yOffset) * yMult;
      const int U_tmp = (int)srcU[i] - cZero;
      const int V_tmp = (int)srcV[i] - cZero;

      const int R_tmp = (Y_tmp                  + V_tmp * rvMult ) >> 16;//32 to 16 bit conversion by left shifting
      const int G_tmp = (Y_tmp + U_tmp * guMult + V_tmp * gvMult ) >> 16;
      const int B_tmp = (Y_tmp + U_tmp * buMult                  ) >> 16;

      dstMem[3*i]   = clip_buf[R_tmp];
      dstMem[3*i+1] = clip_buf[G_tmp];
      dstMem[3*i+2] = clip_buf[B_tmp];
    }
  }
  else if (bps > 8 && bps <= 16)
  {
    switch (yuvColorConversionType)
    {
      case YUVC601ColorConversionType:
        yMult =   19535114;
        rvMult =  26776886;
        guMult =  -6572681;
        gvMult = -13639334;
        buMult =  33843539;
        break;
      case YUVC709ColorConversionType:
      default:
        yMult =   19535114;
        rvMult =  30077204;
        guMult =  -3577718;
        gvMult =  -8940735;
        buMult =  35440221;
    }

    if (bps < 16)
    {
      yMult  = (yMult  + (1<<(15-bps))) >> (16-bps);//32 bit values
      rvMult = (rvMult + (1<<(15-bps))) >> (16-bps);
      guMult = (guMult + (1<<(15-bps))) >> (16-bps);
      gvMult = (gvMult + (1<<(15-bps))) >> (16-bps);
      buMult = (buMult + (1<<(15-bps))) >> (16-bps);
    }
    const unsigned short *srcY = (unsigned short*)sourceBuffer.data();
    const unsigned short *srcU = srcY + componentLength;
    const unsigned short *srcV = srcU + componentLength;
    unsigned char *dstMem = dst;

    int i;
#pragma omp parallel for default(none) private(i) shared(srcY,srcU,srcV,dstMem,yMult,rvMult,guMult,gvMult,buMult,componentLength) // num_threads(2)
    for (i = 0; i < componentLength; ++i)
    {
      qint64 Y_tmp = ((qint64)srcY[i] - yOffset)*yMult;
      qint64 U_tmp = (qint64)srcU[i]- cZero ;
      qint64 V_tmp = (qint64)srcV[i]- cZero ;
      // unsigned int temp = 0, temp1=0;

      qint64 R_tmp  = (Y_tmp                  + V_tmp * rvMult) >> (8+bps);
      dstMem[i*3]   = (R_tmp<0 ? 0 : (R_tmp>rgbMax ? rgbMax : R_tmp))>>(bps-8);
      qint64 G_tmp  = (Y_tmp + U_tmp * guMult + V_tmp * gvMult) >> (8+bps);
      dstMem[i*3+1] = (G_tmp<0 ? 0 : (G_tmp>rgbMax ? rgbMax : G_tmp))>>(bps-8);
      qint64 B_tmp  = (Y_tmp + U_tmp * buMult                 ) >> (8+bps);
      dstMem[i*3+2] = (B_tmp<0 ? 0 : (B_tmp>rgbMax ? rgbMax : B_tmp))>>(bps-8);
//the commented section uses RGB 30 format (10 bits per channel)

/*
      qint64 R_tmp  = ((Y_tmp                  + V_tmp * rvMult))>>(8+bps)   ;
      temp = (R_tmp<0 ? 0 : (R_tmp>rgbMax ? rgbMax : R_tmp));
      dstMem[i*4+3]   = ((temp>>4) & 0x3F);

      qint64 G_tmp  = ((Y_tmp + U_tmp * guMult + V_tmp * gvMult))>>(8+bps);
      temp1 = (G_tmp<0 ? 0 : (G_tmp>rgbMax ? rgbMax : G_tmp));
      dstMem[i*4+2] = ((temp<<4) & 0xF0 ) | ((temp1>>6) & 0x0F);

      qint64 B_tmp  = ((Y_tmp + U_tmp * buMult                 ))>>(8+bps) ;
      temp=0;
      temp = (B_tmp<0 ? 0 : (B_tmp>rgbMax ? rgbMax : B_tmp));
      dstMem[i*4+1] = ((temp1<<2)&0xFC) | ((temp>>8) & 0x03);
      dstMem[i*4] = temp & 0xFF;
*/
    }
  }
  else
    printf("bitdepth %i not supported\n", bps);
}

QLayout *playlistItemYuvSource::createVideoControls(bool yuvFormatFixed)
{
  Ui_playlistItemYUVSource::setupUi(propertiesWidget);

  // Set all the values of the properties widget to the values of this class
  yuvFileFormatComboBox->addItems( yuvFormatList.getFormatedNames() );
  int idx = yuvFormatList.indexOf( srcPixelFormat );
  yuvFileFormatComboBox->setCurrentIndex( idx );
  yuvFileFormatComboBox->setEnabled(!yuvFormatFixed);
  colorComponentsComboBox->addItems( QStringList() << "Y'CbCr" << "Luma Only" << "Cb only" << "Cr only" );
  colorComponentsComboBox->setCurrentIndex( (int)componentDisplayMode );
  chromaInterpolationComboBox->addItems( QStringList() << "Nearest neighbour" << "Bilinear" );
  chromaInterpolationComboBox->setCurrentIndex( (int)interpolationMode );
  colorConversionComboBox->addItems( QStringList() << "ITU-R.BT709" << "ITU-R.BT601" << "ITU-R.BT202" );
  colorConversionComboBox->setCurrentIndex( (int)yuvColorConversionType );
  lumaScaleSpinBox->setValue( lumaScale );
  lumaOffsetSpinBox->setMaximum(1000);
  lumaOffsetSpinBox->setValue( lumaOffset );
  lumaInvertCheckBox->setChecked( lumaInvert );
  chromaScaleSpinBox->setValue( chromaScale );
  chromaOffsetSpinBox->setMaximum(1000);
  chromaOffsetSpinBox->setValue( chromaOffset );
  chromaInvertCheckBox->setChecked( chromaInvert );

  // Connect all the change signals from the controls to "connectWidgetSignals()"
  connect(yuvFileFormatComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotYUVControlChanged()));
  connect(colorComponentsComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotYUVControlChanged()));
  connect(chromaInterpolationComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotYUVControlChanged()));
  connect(colorConversionComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotYUVControlChanged()));
  connect(lumaScaleSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotYUVControlChanged()));
  connect(lumaOffsetSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotYUVControlChanged()));
  connect(lumaInvertCheckBox, SIGNAL(stateChanged(int)), this, SLOT(slotYUVControlChanged()));
  connect(chromaScaleSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotYUVControlChanged()));
  connect(chromaOffsetSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotYUVControlChanged()));
  connect(chromaInvertCheckBox, SIGNAL(stateChanged(int)), this, SLOT(slotYUVControlChanged()));

  return topVBoxLayout;
}

void playlistItemYuvSource::slotYUVControlChanged()
{
  // The control that caused the slot to be called
  QObject *sender = QObject::sender();

  if (sender == colorComponentsComboBox ||
           sender == chromaInterpolationComboBox ||
           sender == colorConversionComboBox ||
           sender == lumaScaleSpinBox ||
           sender == lumaOffsetSpinBox ||
           sender == lumaInvertCheckBox ||
           sender == chromaScaleSpinBox ||
           sender == chromaOffsetSpinBox ||
           sender == chromaInvertCheckBox )
  {
    componentDisplayMode = (ComponentDisplayMode)colorComponentsComboBox->currentIndex();
    interpolationMode = (InterpolationMode)chromaInterpolationComboBox->currentIndex();
    yuvColorConversionType = (YUVCColorConversionType)colorConversionComboBox->currentIndex();
    lumaScale = lumaScaleSpinBox->value();
    lumaOffset = lumaOffsetSpinBox->value();
    lumaInvert = lumaInvertCheckBox->isChecked();
    chromaScale = chromaScaleSpinBox->value();
    chromaOffset = chromaOffsetSpinBox->value();
    chromaInvert = chromaInvertCheckBox->isChecked();

    // Set the current frame in the buffer to be invalid and emit the signal that something has changed
    currentFrameIdx = -1;
    emit signalItemChanged(true);
  }
  else if (sender == yuvFileFormatComboBox)
  {
    srcPixelFormat = yuvFormatList.getFromName( yuvFileFormatComboBox->currentText() );

    // Set the current frame in the buffer to be invalid and emit the signal that something has changed
    currentFrameIdx = -1;
    emit signalItemChanged(true);
  }
}

QPixmap playlistItemYuvSource::calculateDifference(playlistItemVideo *item2, int frame, QList<infoItem> &conversionInfoList)
{
  playlistItemYuvSource *yuvItem2 = dynamic_cast<playlistItemYuvSource*>(item2);
  if (yuvItem2 == NULL)
    // The given item is not a yuv source. We cannot compare YUV values to non YUV values.
    // Call the base class comparison function to compare the items using the RGB values.
    playlistItemVideo::calculateDifference(item2, frame, conversionInfoList);

  if (srcPixelFormat.bitsPerSample != yuvItem2->srcPixelFormat.bitsPerSample)
    // The two items have different bit depths. Compare RGB values instead.
    // TODO: Or should we do this in the YUV domain somehow?
    playlistItemVideo::calculateDifference(item2, frame, conversionInfoList);

  // Load the right images, if not already loaded)
  if (currentFrameIdx != frame)
    loadFrame(frame);
  loadFrame(frame);
  if (yuvItem2->currentFrameIdx != frame)
    yuvItem2->loadFrame(frame);

  int width  = qMin(frameSize.width(), yuvItem2->frameSize.width());
  int height = qMin(frameSize.height(), yuvItem2->frameSize.height());

  const int bps = srcPixelFormat.bitsPerSample;
  const int diffZero = 128 << (bps - 8);
  const int maxVal = (1 << bps) - 1;

  // Create a YUV444 buffer for the difference
  QByteArray diff444;

  // How many values to go to the next line per input
  const unsigned int stride0 = frameSize.width();
  const unsigned int stride1 = yuvItem2->frameSize.width();
  const unsigned int dstStride = width;

  // How many values to go to the next component? (Luma,Cb,Cr)
  const unsigned int componentLength0 = frameSize.width() * frameSize.height();
  const unsigned int componentLength1 = yuvItem2->frameSize.width() * yuvItem2->frameSize.height();
  const unsigned int componentLengthDst = width * height;

  // Also calculate the MSE while we're at it (Y,U,V)
  qint64 mseAdd[3] = {0, 0, 0};

  if (bps > 8)
  {
    // Resize the difference buffer
    diff444.resize( width * height * 3 * 2 );

    // For each component (Luma,Cb,Cr)...
    for (int c = 0; c < 3; c++)
    {
      // Two bytes per value. Get a pointer to the source data.
      unsigned short* src0 = (unsigned short*)tmpBufferYUV444.data() + c * componentLength0;
      unsigned short* src1 = (unsigned short*)yuvItem2->tmpBufferYUV444.data() + c * componentLength1;
      unsigned short* dst  = (unsigned short*)diff444.data() + c * componentLengthDst;
    
      for (int y = 0; y < height; y++)
      {
        for (int x = 0; x < width; x++)
        {
          int delta = src0[x] - src1[x];
          dst[x] = clip( diffZero + delta, 0, maxVal);

          mseAdd[c] += delta * delta;
        }
        src0 += stride0;
        src1 += stride1;
        dst += dstStride;
      }
    }
  }
  else
  {
    // Resize the difference buffer
    diff444.resize( width * height * 3 );

    // For each component (Luma,Cb,Cr)...
    for (int c = 0; c < 3; c++)
    {
      // Get a pointer to the source data
      unsigned char* src0 = (unsigned char*)tmpBufferYUV444.data() + c * componentLength0;
      unsigned char* src1 = (unsigned char*)yuvItem2->tmpBufferYUV444.data() + c * componentLength1;
      unsigned char* dst  = (unsigned char*)diff444.data() + c * componentLengthDst;

      for (int y = 0; y < height; y++)
      {
        for (int x = 0; x < width; x++)
        {
          int delta = src0[x] - src1[x];
          dst[x] = clip( diffZero + delta, 0, maxVal);

          mseAdd[c] += delta * delta;
        }
        src0 += stride0;
        src1 += stride1;
        dst += dstStride;
      }
    }
  }

  // Convert to RGB888
  QByteArray tmpDiffBufferRGB;
  convertYUV4442RGB(diff444, tmpDiffBufferRGB);

  // Append the conversion information that will be returned
  conversionInfoList.append( infoItem("Difference Type","YUV 444") );
  double mse[4];
  mse[0] = double(mseAdd[0]) / (width * height);
  mse[1] = double(mseAdd[1]) / (width * height);
  mse[2] = double(mseAdd[2]) / (width * height);
  mse[3] = mse[0] + mse[1] + mse[2];
  conversionInfoList.append( infoItem("MSE Y",QString("%1").arg(mse[0])) );
  conversionInfoList.append( infoItem("MSE U",QString("%1").arg(mse[1])) );
  conversionInfoList.append( infoItem("MSE V",QString("%1").arg(mse[2])) );
  conversionInfoList.append( infoItem("MSE All",QString("%1").arg(mse[3])) );

  // Convert the image in tmpDiffBufferRGB to a QPixmap using a QImage intermediate.
  // TODO: Isn't there a faster way to do this? Maybe load a pixmap from "BMP"-like data?
  QImage tmpImage((unsigned char*)tmpDiffBufferRGB.data(), width, height, QImage::Format_RGB888);
  QPixmap retPixmap;
  retPixmap.convertFromImage(tmpImage);
  return retPixmap;
}

ValuePairList playlistItemYuvSource::getPixelValuesDifference(playlistItemVideo *item2, QPoint pixelPos)
{
  playlistItemYuvSource *yuvItem2 = dynamic_cast<playlistItemYuvSource*>(item2);
  if (yuvItem2 == NULL)
    // The given item is not a yuv source. We cannot compare YUV values to non YUV values.
    // Call the base class comparison function to compare the items using the RGB values.
    return playlistItemVideo::getPixelValuesDifference(item2, pixelPos);

  if (srcPixelFormat.bitsPerSample != yuvItem2->srcPixelFormat.bitsPerSample)
    // The two items have different bit depths. Compare RGB values instead.
    // TODO: Or should we do this in the YUV domain somehow?
    return playlistItemVideo::getPixelValuesDifference(item2, pixelPos);

  int width  = qMin(frameSize.width(), yuvItem2->frameSize.width());
  int height = qMin(frameSize.height(), yuvItem2->frameSize.height());

  if (pixelPos.x() < 0 || pixelPos.x() >= width || pixelPos.y() < 0 || pixelPos.y() >= height)
    return ValuePairList();

  // How many values to go to the next line per input
  const unsigned int stride0 = frameSize.width();
  const unsigned int stride1 = yuvItem2->frameSize.width();

  // How many values to go to the next component? (Luma,Cb,Cr)
  const unsigned int componentLength0 = frameSize.width() * frameSize.height();
  const unsigned int componentLength1 = yuvItem2->frameSize.width() * yuvItem2->frameSize.height();

  const int bps = srcPixelFormat.bitsPerSample;

  ValuePairList values("Difference Values (A,B,A-B)");

  if (bps > 8)
  {
    // For each component (Luma,Cb,Cr)...
    for (int c = 0; c < 3; c++)
    {
      // Two bytes per value. Get a pointer to the source data.
      unsigned short* src0 = (unsigned short*)tmpBufferYUV444.data() + c * componentLength0 + pixelPos.y() * stride0;
      unsigned short* src1 = (unsigned short*)yuvItem2->tmpBufferYUV444.data() + c * componentLength1 + pixelPos.y() * stride1;
      
      int val0 = src0[pixelPos.x()];
      int val1 = src1[pixelPos.x()];
      int diff = val0 - val1;

      values.append( ValuePair( (c==0) ? "Y" : (c==1) ? "U" : "V", QString("%1,%2,%3").arg(val0).arg(val1).arg(diff)) );
    }
  }
  else
  {
    // For each component (Luma,Cb,Cr)...
    for (int c = 0; c < 3; c++)
    {
      // Two bytes per value. Get a pointer to the source data.
      unsigned char* src0 = (unsigned char*)tmpBufferYUV444.data() + c * componentLength0 + pixelPos.y() * stride0;
      unsigned char* src1 = (unsigned char*)yuvItem2->tmpBufferYUV444.data() + c * componentLength1 + pixelPos.y() * stride1;
      
      int val0 = src0[pixelPos.x()];
      int val1 = src1[pixelPos.x()];
      int diff = val0 - val1;

      values.append( ValuePair( (c==0) ? "Y" : (c==1) ? "U" : "V", QString("%1,%2,%3").arg(val0).arg(val1).arg(diff)) );
    }
  }

  return values;
}

void playlistItemYuvSource::drawPixelValues(QPainter *painter, unsigned int xMin, unsigned int xMax, unsigned int yMin, unsigned int yMax, double zoomFactor)
{
  // The center point of the pixel (0,0).
  QPoint centerPointZero = ( QPoint(-frameSize.width(), -frameSize.height()) * zoomFactor + QPoint(zoomFactor,zoomFactor) ) / 2;
  // This rect has the size of one pixel and is moved on top of each pixel to draw the text
  QRect pixelRect;
  pixelRect.setSize( QSize(zoomFactor, zoomFactor) );

  // We might change the pen doing this so backup the current pen and reset it later
  QPen backupPen = painter->pen();

  // If the Y is below this value, use white text, otherwise black text
  int whiteLimit = 1 << (srcPixelFormat.bitsPerSample - 1);

  if (srcPixelFormat.subsamplingHorizontal == 1 && srcPixelFormat.subsamplingVertical == 1)
  {
    // YUV 444 format. We draw all values in the center of each pixel
    for (unsigned int x = xMin; x <= xMax; x++)
    {
      for (unsigned int y = yMin; y <= yMax; y++)
      {
        // Calculate the center point of the pixel. (Each pixel is of size (zoomFactor,zoomFactor)) and move the pixelRect to that point.
        QPoint pixCenter = centerPointZero + QPoint(x * zoomFactor, y * zoomFactor);
        pixelRect.moveCenter(pixCenter);
     
        // Get the text to show
        int Y,U,V;
        getPixelValue(QPoint(x,y), Y, U, V);
        QString valText = QString("Y%1\nU%2\nV%3").arg(Y).arg(U).arg(V);
        
        painter->setPen( (Y < whiteLimit) ? Qt::white : Qt::black );
        painter->drawText(pixelRect, Qt::AlignCenter, valText);
      }
    }
  }
  else if (srcPixelFormat.subsamplingHorizontal <= 2 && srcPixelFormat.subsamplingVertical <= 2)
  {
    // Non YUV 444 format. The Y values go into the center of each pixel, but the U and V values go somewhere else.
    for (unsigned int x = xMin - 1; x <= xMax; x++)
    {
      for (unsigned int y = yMin - 1; y <= yMax; y++)
      {
        // Calculate the center point of the pixel. (Each pixel is of size (zoomFactor,zoomFactor)) and move the pixelRect to that point.
        QPoint pixCenter = centerPointZero + QPoint(x * zoomFactor, y * zoomFactor);
        pixelRect.moveCenter(pixCenter);
     
        // Get the text to show
        int Y,U,V;
        getPixelValue(QPoint(x,y), Y, U, V);
        QString valText = QString("Y%1").arg(Y);
              
        painter->setPen( (Y < whiteLimit) ? Qt::white : Qt::black );
        painter->drawText(pixelRect, Qt::AlignCenter, valText);

        if (srcPixelFormat.subsamplingHorizontal == 2 && srcPixelFormat.subsamplingVertical == 1 && (x % 2) == 0)
        {
          // Horizontal sub sampling by 2 and x is even. Draw the U and V values in the center of the two horizontal pixels (x and x+1)
          pixelRect.translate(zoomFactor/2, 0);
          valText = QString("U%1\nV%2").arg(U).arg(V);
          painter->setPen( (Y < whiteLimit) ? Qt::white : Qt::black );
          painter->drawText(pixelRect, Qt::AlignCenter, valText);
        }
        if (srcPixelFormat.subsamplingHorizontal == 1 && srcPixelFormat.subsamplingVertical == 2 && (y % 2) == 0)
        {
          // Vertical sub sampling by 2 and y is even. Draw the U and V values in the center of the two vertical pixels (y and y+1)
          pixelRect.translate(0, zoomFactor/2);
          valText = QString("U%1\nV%2").arg(U).arg(V);
          painter->setPen( (Y < whiteLimit) ? Qt::white : Qt::black );
          painter->drawText(pixelRect, Qt::AlignCenter, valText);
        }
        if (srcPixelFormat.subsamplingHorizontal == 2 && srcPixelFormat.subsamplingVertical == 2 && (x % 2) == 0 && (y % 2) == 0)
        {
          // Horizontal and vertical sub sampling by 2 and x and y are even. Draw the U and V values in the center of the four pixels
          pixelRect.translate(zoomFactor/2, zoomFactor/2);
          valText = QString("U%1\nV%2").arg(U).arg(V);
          painter->setPen( (Y < whiteLimit) ? Qt::white : Qt::black );
          painter->drawText(pixelRect, Qt::AlignCenter, valText);
        }
      }
    }
  }
  else
  {
    // Other subsamplings than 2 in either direction are not supported yet. 
    // (Are there any YUV formats like this?)
  }

  // Reset pen
  painter->setPen(backupPen);
}

// Append the properties of the playlistItemYuvSource to the QDomElement.
// For a yuv source that is just the pixel format
void playlistItemYuvSource::appendItemProperties(QDomElementYUV &root)
{
  root.appendProperiteChild( "pixelFormat", srcPixelFormat.name );

  // Walk up the inheritance hierarchie and append the base class properties.
  playlistItemVideo::appendItemProperties(root);
}

void playlistItemYuvSource::parseProperties(QDomElementYUV root)
{
  QString sourcePixelFormat = root.findChildValue("pixelFormat");
  srcPixelFormat = yuvFormatList.getFromName( sourcePixelFormat );

  // Walk up the inhritance tree and let the base classes parse their properites from the file
  playlistItemVideo::parseProperties(root);
}