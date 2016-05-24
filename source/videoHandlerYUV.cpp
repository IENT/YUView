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

#include "videoHandlerYUV.h"
#include <QtEndian>
#include <QTime>
#include <QLabel>
#include <QGroupBox>
#include "stdio.h"
#include <QPainter>
#include <xmmintrin.h>
#include <QDebug>

#define MIN(a,b) ((a)>(b)?(b):(a))
#define MAX(a,b) ((a)<(b)?(b):(a))

// Activate this if you want to know when wich buffer is loaded/converted to pixmap and so on.
#define VIDEOHANDLERYUV_DEBUG_LOADING 0
#if VIDEOHANDLERYUV_DEBUG_LOADING
#define DEBUG_YUV qDebug
#else
#define DEBUG_YUV(fmt,...) ((void)0)
#endif

/* Get the number of bytes for a frame with this yuvPixelFormat and the given size
*/
qint64 videoHandlerYUV::yuvPixelFormat::bytesPerFrame(QSize frameSize)
{
  if (name == "Unknown Pixel Format" || !frameSize.isValid())
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
videoHandlerYUV::YUVFormatList::YUVFormatList()
{
  append( videoHandlerYUV::yuvPixelFormat()); // "Unknown Pixel Format"
  append( videoHandlerYUV::yuvPixelFormat("4:4:4 Y'CbCr 16-bit LE planar", 16, 48, 1, 1, 1, true, 2) );
  append( videoHandlerYUV::yuvPixelFormat("4:4:4 Y'CbCr 16-bit BE planar", 16, 48, 1, 1, 1, true, 2) );
  append( videoHandlerYUV::yuvPixelFormat("4:4:4 Y'CbCr 12-bit LE planar", 12, 48, 1, 1, 1, true, 2) );
  append( videoHandlerYUV::yuvPixelFormat("4:4:4 Y'CbCr 12-bit BE planar", 12, 48, 1, 1, 1, true, 2) );
  append( videoHandlerYUV::yuvPixelFormat("4:4:4 Y'CbCr 10-bit LE planar", 10, 48, 1, 1, 1, true, 2) );
  append( videoHandlerYUV::yuvPixelFormat("4:4:4 Y'CbCr 10-bit BE planar", 10, 48, 1, 1, 1, true, 2) );
  append( videoHandlerYUV::yuvPixelFormat("4:4:4 Y'CbCr 8-bit planar", 8, 24, 1, 1, 1, true) );
  append( videoHandlerYUV::yuvPixelFormat("4:4:4 Y'CrCb 8-bit planar", 8, 24, 1, 1, 1, true) );
  append( videoHandlerYUV::yuvPixelFormat("4:2:2 Y'CbCr 8-bit planar", 8, 16, 1, 2, 1, true) );
  append( videoHandlerYUV::yuvPixelFormat("4:2:2 Y'CrCb 8-bit planar", 8, 16, 1, 2, 1, true) );
  append( videoHandlerYUV::yuvPixelFormat("4:2:2 8-bit packed", 8, 16, 1, 2, 1, false) );
  append( videoHandlerYUV::yuvPixelFormat("4:2:2 10-bit packed 'v210'", 10, 128, 6, 2, 1, false, 2) );
  append( videoHandlerYUV::yuvPixelFormat("4:2:2 10-bit packed (UYVY)", 10, 128, 6, 2, 1, true, 2) );
  append( videoHandlerYUV::yuvPixelFormat("4:2:0 Y'CbCr 10-bit LE planar", 10, 24, 1, 2, 2, true, 2) );
  append( videoHandlerYUV::yuvPixelFormat("4:2:0 Y'CbCr 8-bit planar", 8, 12, 1, 2, 2, true) );
  append( videoHandlerYUV::yuvPixelFormat("4:1:1 Y'CbCr 8-bit planar", 8, 12, 1, 4, 1, true) );
  append( videoHandlerYUV::yuvPixelFormat("4:0:0 8-bit", 8, 8, 1, 0, 0, true) );
}

/* Put all the names of the yuvPixelFormats into a list and return it
*/
QStringList videoHandlerYUV::YUVFormatList::getFormatedNames()
{
  QStringList l;
  for (int i = 0; i < count(); i++)
  {
    l.append( at(i).name );
  }
  return l;
}

videoHandlerYUV::yuvPixelFormat videoHandlerYUV::YUVFormatList::getFromName(QString name)
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
videoHandlerYUV::YUVFormatList videoHandlerYUV::yuvFormatList;

videoHandlerYUV::videoHandlerYUV() : videoHandler(),
  ui(new Ui::videoHandlerYUV)
{
  // preset internal values
  setSrcPixelFormat( yuvFormatList.getFromName("Unknown Pixel Format") );
  interpolationMode = NearestNeighborInterpolation;
  componentDisplayMode = DisplayAll;
  yuvColorConversionType = YUVC709ColorConversionType;
  lumaScale = 1;
  lumaOffset = 125;
  chromaScale = 1;
  chromaOffset = 128;
  lumaInvert = false;
  chromaInvert = false;
  controlsCreated = false;
  currentFrameRawYUVData_frameIdx = -1;
}

void videoHandlerYUV::loadValues(QSize newFramesize, QString sourcePixelFormat)
{
  frameSize = newFramesize;
  setSrcPixelFormat( yuvFormatList.getFromName(sourcePixelFormat) );
}

videoHandlerYUV::~videoHandlerYUV()
{
  // This will cause a "QMutex: destroying locked mutex" warning by Qt.
  // However, here this is on purpose.
  cachingMutex.lock();
  delete ui;
}

ValuePairList videoHandlerYUV::getPixelValues(QPoint pixelPos)
{
  unsigned int Y,U,V;
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

#if SSE_CONVERSION_420_ALT
void videoHandlerYUV::yuv420_to_argb8888(quint8 *yp, quint8 *up, quint8 *vp, quint32 sy, quint32 suv, int width, int height, quint8 *rgb, quint32 srgb)
{
  __m128i y0r0, y0r1, u0, v0;
  __m128i y00r0, y01r0, y00r1, y01r1;
  __m128i u00, u01, v00, v01;
  __m128i rv00, rv01, gu00, gu01, gv00, gv01, bu00, bu01;
  __m128i r00, r01, g00, g01, b00, b01;
  __m128i rgb0123, rgb4567, rgb89ab, rgbcdef;
  __m128i gbgb;
  __m128i ysub, uvsub;
  __m128i zero, facy, facrv, facgu, facgv, facbu;
  __m128i *srcy128r0, *srcy128r1;
  __m128i *dstrgb128r0, *dstrgb128r1;
  __m64   *srcu64, *srcv64;


  //    Implement the following conversion:
  //    B = 1.164(Y - 16)                   + 2.018(U - 128)
  //    G = 1.164(Y - 16) - 0.813(V - 128)  - 0.391(U - 128)
  //    R = 1.164(Y - 16) + 1.596(V - 128)

  int x, y;
  // constants
  ysub  = _mm_set1_epi32( 0x00100010 ); // value 16 for subtraction
  uvsub = _mm_set1_epi32( 0x00800080 ); // value 128

  // multiplication factors bitshifted by 6
  facy  = _mm_set1_epi32( 0x004a004a );
  facrv = _mm_set1_epi32( 0x00660066 );
  facgu = _mm_set1_epi32( 0x00190019 );
  facgv = _mm_set1_epi32( 0x00340034 );
  facbu = _mm_set1_epi32( 0x00810081 );

  zero  = _mm_set1_epi32( 0x00000000 );

  for( y = 0; y < height; y += 2 )
  {
    srcy128r0 = (__m128i *)(yp + sy*y);
    srcy128r1 = (__m128i *)(yp + sy*y + sy);
    srcu64 = (__m64 *)(up + suv*(y/2));
    srcv64 = (__m64 *)(vp + suv*(y/2));

    // dst row 0 and row 1
    dstrgb128r0 = (__m128i *)(rgb + srgb*y);
    dstrgb128r1 = (__m128i *)(rgb + srgb*y + srgb);

    for( x = 0; x < width; x += 16 )
    {
      u0 = _mm_loadl_epi64( (__m128i *)srcu64 ); srcu64++;
      v0 = _mm_loadl_epi64( (__m128i *)srcv64 ); srcv64++;

      y0r0 = _mm_load_si128( srcy128r0++ );
      y0r1 = _mm_load_si128( srcy128r1++ );

      // expand to 16 bit, subtract and multiply constant y factors
      y00r0 = _mm_mullo_epi16( _mm_sub_epi16( _mm_unpacklo_epi8( y0r0, zero ), ysub ), facy );
      y01r0 = _mm_mullo_epi16( _mm_sub_epi16( _mm_unpackhi_epi8( y0r0, zero ), ysub ), facy );
      y00r1 = _mm_mullo_epi16( _mm_sub_epi16( _mm_unpacklo_epi8( y0r1, zero ), ysub ), facy );
      y01r1 = _mm_mullo_epi16( _mm_sub_epi16( _mm_unpackhi_epi8( y0r1, zero ), ysub ), facy );

      // expand u and v so they're aligned with y values
      u0  = _mm_unpacklo_epi8( u0,  zero );
      u00 = _mm_sub_epi16( _mm_unpacklo_epi16( u0, u0 ), uvsub );
      u01 = _mm_sub_epi16( _mm_unpackhi_epi16( u0, u0 ), uvsub );

      v0  = _mm_unpacklo_epi8( v0,  zero );
      v00 = _mm_sub_epi16( _mm_unpacklo_epi16( v0, v0 ), uvsub );
      v01 = _mm_sub_epi16( _mm_unpackhi_epi16( v0, v0 ), uvsub );

      // common factors on both rows.
      rv00 = _mm_mullo_epi16( facrv, v00 );
      rv01 = _mm_mullo_epi16( facrv, v01 );
      gu00 = _mm_mullo_epi16( facgu, u00 );
      gu01 = _mm_mullo_epi16( facgu, u01 );
      gv00 = _mm_mullo_epi16( facgv, v00 );
      gv01 = _mm_mullo_epi16( facgv, v01 );
      bu00 = _mm_mullo_epi16( facbu, u00 );
      bu01 = _mm_mullo_epi16( facbu, u01 );

      // add together and bitshift to the right
      r00 = _mm_srai_epi16( _mm_add_epi16( y00r0, rv00 ), 6 );
      r01 = _mm_srai_epi16( _mm_add_epi16( y01r0, rv01 ), 6 );
      g00 = _mm_srai_epi16( _mm_sub_epi16( _mm_sub_epi16( y00r0, gu00 ), gv00 ), 6 );
      g01 = _mm_srai_epi16( _mm_sub_epi16( _mm_sub_epi16( y01r0, gu01 ), gv01 ), 6 );
      b00 = _mm_srai_epi16( _mm_add_epi16( y00r0, bu00 ), 6 );
      b01 = _mm_srai_epi16( _mm_add_epi16( y01r0, bu01 ), 6 );

      r00 = _mm_packus_epi16( r00, r01 );
      g00 = _mm_packus_epi16( g00, g01 );
      b00 = _mm_packus_epi16( b00, b01 );

      // shuffle back together to lower 0rgb0rgb...
      r01     = _mm_unpacklo_epi8(  r00,  zero ); // 0r0r...
      gbgb    = _mm_unpacklo_epi8(  b00,  g00 );  // gbgb...
      rgb0123 = _mm_unpacklo_epi16( gbgb, r01 );  // lower 0rgb0rgb...
      rgb4567 = _mm_unpackhi_epi16( gbgb, r01 );  // upper 0rgb0rgb...

      // shuffle back together to upper 0rgb0rgb...
      r01     = _mm_unpackhi_epi8(  r00,  zero );
      gbgb    = _mm_unpackhi_epi8(  b00,  g00 );
      rgb89ab = _mm_unpacklo_epi16( gbgb, r01 );
      rgbcdef = _mm_unpackhi_epi16( gbgb, r01 );

      // write to dst
      _mm_store_si128( dstrgb128r0++, rgb0123 );
      _mm_store_si128( dstrgb128r0++, rgb4567 );
      _mm_store_si128( dstrgb128r0++, rgb89ab );
      _mm_store_si128( dstrgb128r0++, rgbcdef );

      // row 1
      r00 = _mm_srai_epi16( _mm_add_epi16( y00r1, rv00 ), 6 );
      r01 = _mm_srai_epi16( _mm_add_epi16( y01r1, rv01 ), 6 );
      g00 = _mm_srai_epi16( _mm_sub_epi16( _mm_sub_epi16( y00r1, gu00 ), gv00 ), 6 );
      g01 = _mm_srai_epi16( _mm_sub_epi16( _mm_sub_epi16( y01r1, gu01 ), gv01 ), 6 );
      b00 = _mm_srai_epi16( _mm_add_epi16( y00r1, bu00 ), 6 );
      b01 = _mm_srai_epi16( _mm_add_epi16( y01r1, bu01 ), 6 );

      r00 = _mm_packus_epi16( r00, r01 );
      g00 = _mm_packus_epi16( g00, g01 );
      b00 = _mm_packus_epi16( b00, b01 );

      r01     = _mm_unpacklo_epi8(  r00,  zero );
      gbgb    = _mm_unpacklo_epi8(  b00,  g00 );
      rgb0123 = _mm_unpacklo_epi16( gbgb, r01 );
      rgb4567 = _mm_unpackhi_epi16( gbgb, r01 );

      r01     = _mm_unpackhi_epi8(  r00,  zero );
      gbgb    = _mm_unpackhi_epi8(  b00,  g00 );
      rgb89ab = _mm_unpacklo_epi16( gbgb, r01 );
      rgbcdef = _mm_unpackhi_epi16( gbgb, r01 );

      _mm_store_si128( dstrgb128r1++, rgb0123 );
      _mm_store_si128( dstrgb128r1++, rgb4567 );
      _mm_store_si128( dstrgb128r1++, rgb89ab );
      _mm_store_si128( dstrgb128r1++, rgbcdef );

    }
  }
}
#endif

#if SSE_CONVERSION
void videoHandlerYUV::convert2YUV444(byteArrayAligned &sourceBuffer, byteArrayAligned &targetBuffer)
#else
void videoHandlerYUV::convert2YUV444(QByteArray &sourceBuffer, QByteArray &targetBuffer)
#endif
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

#if SSE_CONVERSION
void videoHandlerYUV::applyYUVTransformation(byteArrayAligned &sourceBuffer)
#else
void videoHandlerYUV::applyYUVTransformation(QByteArray &sourceBuffer)
#endif
{
  // TODO: Check this for 10-bit Unput
  // TODO: If luma only is selected, the U and V components are set to chromaZero, but a little color
  //       can still be seen. Should we do this in the conversion functions? They have to be
  //       resorted anyways.

  if (!yuvMathRequired())
    return;

  const int lumaLength = frameSize.width() * frameSize.height();
  const int singleChromaLength = lumaLength;
  //const int chromaLength = 2*singleChromaLength;
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
    // Process 8 bit input
    const unsigned char *src = (const unsigned char*)sourceBuffer.data();
    unsigned char *dst = (unsigned char*)sourceBuffer.data();

    // Process Luma
    if (componentDisplayMode == DisplayCb || componentDisplayMode == DisplayCr)
    {
      // Set the Y component to 0 since we are not displayling it
      memset(dst, 0, lumaLength * sizeof(unsigned char));
    }
    else if (colorMode == YUVMathDefaultColors || colorMode == YUVMathLumaOnly)
    {
      // The Y component is displayed and a Y transformation has to be applied
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

    // Process chroma components
    for (int c = 0; c < 2; c++)
    {
      if (   componentDisplayMode == DisplayY ||
           ( componentDisplayMode == DisplayCb && c == 1 ) ||
           ( componentDisplayMode == DisplayCr && c == 0 ) )
      {
        // This chroma component is not displayed. Set it to zero.
        memset(dst, chromaZero, singleChromaLength * sizeof(unsigned char) );
      }
      else if (    colorMode == YUVMathDefaultColors
               || (colorMode == YUVMathCbOnly && c == 0)
               || (colorMode == YUVMathCrOnly && c == 1)
               )
      {
        // This chroma component needs to be transformed
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

  }
  else if (sourceBPS>8 && sourceBPS<=16)
  {
    // Process 9 to 16 bit input
    const unsigned short *src = (const unsigned short*)sourceBuffer.data();
    unsigned short *dst = (unsigned short*)sourceBuffer.data();

    // Process Luma
    if (componentDisplayMode == DisplayCb || componentDisplayMode == DisplayCr)
    {
      // Set the Y component to 0 since we are not displayling it
      memset(dst, 0, lumaLength * sizeof(unsigned short));
    }
    else if (colorMode == YUVMathDefaultColors || colorMode == YUVMathLumaOnly)
    {
      // The Y component is displayed and a Y transformation has to be applied
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

    // Process chroma components
    for (int c = 0; c < 2; c++) {
      if (   componentDisplayMode != DisplayY ||
           ( componentDisplayMode == DisplayCb && c == 1 ) ||
           ( componentDisplayMode == DisplayCr && c == 0 ) )
      {
        // This chroma component is not displayed. Set it to zero.
        memset(dst, chromaZero, singleChromaLength * sizeof(unsigned char) );
      }
      else if (   colorMode == YUVMathDefaultColors
         || (colorMode == YUVMathCbOnly && c == 0)
         || (colorMode == YUVMathCrOnly && c == 1)
         )
      {
        // This chroma component needs to be transformed
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

#if SSE_CONVERSION
void videoHandlerYUV::convertYUV4442RGB(byteArrayAligned &sourceBuffer, byteArrayAligned &targetBuffer)
#else
void videoHandlerYUV::convertYUV4442RGB(QByteArray &sourceBuffer, QByteArray &targetBuffer)
#endif
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

QLayout *videoHandlerYUV::createVideoHandlerControls(QWidget *parentWidget, bool isSizeFixed)
{

  // Absolutely always only call this function once!
  assert(!controlsCreated);
  controlsCreated = true;

  QVBoxLayout *newVBoxLayout;
  if (!isSizeFixed)
  {
    // Our parent (videoHandler) also has controls to add. Create a new vBoxLayout and append the parent controls
    // and our controls into that layout, seperated by a line. Return that layout
    newVBoxLayout = new QVBoxLayout;
    newVBoxLayout->addLayout( videoHandler::createVideoHandlerControls(parentWidget, isSizeFixed) );
  
    QFrame *line = new QFrame(parentWidget);
    line->setObjectName(QStringLiteral("line"));
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    newVBoxLayout->addWidget(line);
  }
  
  ui->setupUi(parentWidget);

  // Set all the values of the properties widget to the values of this class
  ui->yuvFileFormatComboBox->addItems( yuvFormatList.getFormatedNames() );
  int idx = yuvFormatList.indexOf( srcPixelFormat );
  ui->yuvFileFormatComboBox->setCurrentIndex( idx );
  ui->yuvFileFormatComboBox->setEnabled(!isSizeFixed);
  ui->colorComponentsComboBox->addItems( QStringList() << "Y'CbCr" << "Luma Only" << "Cb only" << "Cr only" );
  ui->colorComponentsComboBox->setCurrentIndex( (int)componentDisplayMode );
  ui->chromaInterpolationComboBox->addItems( QStringList() << "Nearest neighbour" << "Bilinear" );
  ui->chromaInterpolationComboBox->setCurrentIndex( (int)interpolationMode );
  ui->colorConversionComboBox->addItems( QStringList() << "ITU-R.BT709" << "ITU-R.BT601" << "ITU-R.BT202" );
  ui->colorConversionComboBox->setCurrentIndex( (int)yuvColorConversionType );
  ui->lumaScaleSpinBox->setValue( lumaScale );
  ui->lumaOffsetSpinBox->setMaximum(1000);
  ui->lumaOffsetSpinBox->setValue( lumaOffset );
  ui->lumaInvertCheckBox->setChecked( lumaInvert );
  ui->chromaScaleSpinBox->setValue( chromaScale );
  ui->chromaOffsetSpinBox->setMaximum(1000);
  ui->chromaOffsetSpinBox->setValue( chromaOffset );
  ui->chromaInvertCheckBox->setChecked( chromaInvert );

  // Connect all the change signals from the controls to "connectWidgetSignals()"
  connect(ui->yuvFileFormatComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotYUVControlChanged()));
  connect(ui->colorComponentsComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotYUVControlChanged()));
  connect(ui->chromaInterpolationComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotYUVControlChanged()));
  connect(ui->colorConversionComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotYUVControlChanged()));
  connect(ui->lumaScaleSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotYUVControlChanged()));
  connect(ui->lumaOffsetSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotYUVControlChanged()));
  connect(ui->lumaInvertCheckBox, SIGNAL(stateChanged(int)), this, SLOT(slotYUVControlChanged()));
  connect(ui->chromaScaleSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotYUVControlChanged()));
  connect(ui->chromaOffsetSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotYUVControlChanged()));
  connect(ui->chromaInvertCheckBox, SIGNAL(stateChanged(int)), this, SLOT(slotYUVControlChanged()));

  if (!isSizeFixed)
    newVBoxLayout->addLayout(ui->topVBoxLayout);

  return (isSizeFixed) ? ui->topVBoxLayout : newVBoxLayout;
}

void videoHandlerYUV::slotYUVControlChanged()
{
  // The control that caused the slot to be called
  QObject *sender = QObject::sender();

  if (sender == ui->colorComponentsComboBox ||
           sender == ui->chromaInterpolationComboBox ||
           sender == ui->colorConversionComboBox ||
           sender == ui->lumaScaleSpinBox ||
           sender == ui->lumaOffsetSpinBox ||
           sender == ui->lumaInvertCheckBox ||
           sender == ui->chromaScaleSpinBox ||
           sender == ui->chromaOffsetSpinBox ||
           sender == ui->chromaInvertCheckBox )
  {
    componentDisplayMode = (ComponentDisplayMode)ui->colorComponentsComboBox->currentIndex();
    interpolationMode = (InterpolationMode)ui->chromaInterpolationComboBox->currentIndex();
    yuvColorConversionType = (YUVCColorConversionType)ui->colorConversionComboBox->currentIndex();
    lumaScale = ui->lumaScaleSpinBox->value();
    lumaOffset = ui->lumaOffsetSpinBox->value();
    lumaInvert = ui->lumaInvertCheckBox->isChecked();
    chromaScale = ui->chromaScaleSpinBox->value();
    chromaOffset = ui->chromaOffsetSpinBox->value();
    chromaInvert = ui->chromaInvertCheckBox->isChecked();

    // Set the current frame in the buffer to be invalid and clear the cache.
    // Emit that this item needs redraw and the cache needs updating.
    currentFrameIdx = -1;
    if (pixmapCache.count() > 0)
      pixmapCache.clear();
    emit signalHandlerChanged(true, true);
  }
  else if (sender == ui->yuvFileFormatComboBox)
  {
    setSrcPixelFormat( yuvFormatList.getFromName( ui->yuvFileFormatComboBox->currentText() ) );

    // Check if the new format changed the number of frames in the sequence
    emit signalUpdateFrameLimits();

    // Set the current frame in the buffer to be invalid and clear the cache.
    // Emit that this item needs redraw and the cache needs updating.
    currentFrameIdx = -1;
    if (pixmapCache.count() > 0)
      pixmapCache.clear();
    emit signalHandlerChanged(true, true);
  }
}

QPixmap videoHandlerYUV::calculateDifference(videoHandler *item2, int frame, QList<infoItem> &conversionInfoList, int amplificationFactor, bool markDifference)
{
  videoHandlerYUV *yuvItem2 = dynamic_cast<videoHandlerYUV*>(item2);
  if (yuvItem2 == NULL)
    // The given item is not a yuv source. We cannot compare YUV values to non YUV values.
    // Call the base class comparison function to compare the items using the RGB values.
    videoHandler::calculateDifference(item2, frame, conversionInfoList, amplificationFactor, markDifference);

  if (srcPixelFormat.bitsPerSample != yuvItem2->srcPixelFormat.bitsPerSample)
    // The two items have different bit depths. Compare RGB values instead.
    // TODO: Or should we do this in the YUV domain somehow?
    videoHandler::calculateDifference(item2, frame, conversionInfoList, amplificationFactor, markDifference);

  // Load the right raw YUV data (if not already loaded).
  // This will just update the raw YUV data. No conversion to pixmap (RGB) is performed. This is either
  // done on request if the frame is actually shown or has already been done by the caching process.
  if (!loadRawYUVData(frame))
    return QPixmap();  // Loading failed
  if (!yuvItem2->loadRawYUVData(frame))
    return QPixmap();  // Loading failed

  int width  = qMin(frameSize.width(), yuvItem2->frameSize.width());
  int height = qMin(frameSize.height(), yuvItem2->frameSize.height());

  const int bps = srcPixelFormat.bitsPerSample;
  const int diffZero = 128 << (bps - 8);
  const int maxVal = (1 << bps) - 1;

  QString diffType;

  // Create a YUV444 buffer for the difference
#if SSE_CONVERSION
  byteArrayAligned diffYUV;
  byteArrayAligned tmpDiffBufferRGB;
#else
  QByteArray diffYUV;
  QByteArray tmpDiffBufferRGB;
#endif

  // Also calculate the MSE while we're at it (Y,U,V)
  // TODO: Bug: MSE is not scaled correctly in all YUV format cases
  qint64 mseAdd[3] = {0, 0, 0};

  if (srcPixelFormat.name == "4:2:0 Y'CbCr 8-bit planar" && yuvItem2->srcPixelFormat.name == "4:2:0 Y'CbCr 8-bit planar")
  {
    // Both items are YUV 4:2:0 8-bit
    // We can directly subtract the YUV 4:2:0 values
    diffType = "YUV 4:2:0";

    // How many values to go to the next line per input
    unsigned int stride0 = frameSize.width();
    unsigned int stride1 = yuvItem2->frameSize.width();

    // Resize the difference buffer
    diffYUV.resize( width * height + (width / 2) * (height / 2) * 2 );  // YUV 4:2:0

    unsigned char* src0 = (unsigned char*)currentFrameRawYUVData.data();
    unsigned char* src1 = (unsigned char*)yuvItem2->currentFrameRawYUVData.data();

    if (markDifference)
    {
      // We don't want to see the actual difference but just where differences are.
      // To be even faster, we will directly draw into the RGB buffer

      // Resize the RGB buffer
      tmpDiffBufferRGB.resize( width * height * 3 );
      unsigned char* dst = (unsigned char*)tmpDiffBufferRGB.data();

      // Get pointers to U/V ...
      unsigned char* src0U = src0 + (width * height);
      unsigned char* src1U = src1 + (width * height);
      unsigned char* src0V = src0U + (width / 2 * height / 2);
      unsigned char* src1V = src1U + (width / 2 * height / 2);

      unsigned int stride0UV = stride0 / 2;
      unsigned int stride1UV = stride1 / 2;
      unsigned int dstStride = width * 3;

      for (int y = 0; y < height; y++)
      {
        for (int x = 0; x < width; x++)
        {
          int deltaY = src0[x] - src1[x];
          int deltaU = src0U[x/2] - src1U[x/2];
          int deltaV = src0V[x/2] - src1V[x/2];

          mseAdd[0] += deltaY * deltaY;
          mseAdd[1] += deltaU * deltaU;
          mseAdd[2] += deltaV * deltaV;

          // select RGB color
          unsigned char R = 0,G = 0,B = 0;
          if (deltaY == 0)
          {
            G = (deltaU == 0) ? 0 : 70;
            B = (deltaV == 0) ? 0 : 70;
          }
          else
          {
            if (deltaU == 0 && deltaV == 0)
            {
              R = 70;
              G = 70;
              B = 70;
            }
            else
            {
              G = (deltaU == 0) ? 0 : 255;
              B = (deltaV == 0) ? 0 : 255;
            }
          }

          dst[x*3    ] = R;
          dst[x*3 + 1] = G;
          dst[x*3 + 2] = G;
        }
        // Goto next line
        src0 += stride0;
        src1 += stride1;
        if (y % 2 == 1)
        {
          src0U += stride0UV;
          src0V += stride0UV;
          src1U += stride1UV;
          src1V += stride1UV;
        }
        dst += dstStride;
      }

    }
    else
    {
      // 4:2:0 but do not mark differences

      unsigned int dstStride = width;
      unsigned char* dst  = (unsigned char*)diffYUV.data();

      // Luma
      for (int y = 0; y < height; y++)
      {
        for (int x = 0; x < width; x++)
        {
          int delta = src0[x] - src1[x];
          dst[x] = clip( diffZero + delta * amplificationFactor, 0, maxVal);

          mseAdd[0] += delta * delta;
        }
        src0 += stride0;
        src1 += stride1;
        dst += dstStride;
      }

      // Chroma
      stride0   = stride0   / 2;
      stride1   = stride1   / 2;
      dstStride = dstStride / 2;
      
      // Chroma U
      src0 = (unsigned char*)currentFrameRawYUVData.data() + (frameSize.width() * frameSize.height());
      src1 = (unsigned char*)yuvItem2->currentFrameRawYUVData.data() + (yuvItem2->frameSize.width() * yuvItem2->frameSize.height());
      
      for (int y = 0; y < height / 2; y++)
      {
        for (int x = 0; x < width / 2; x++)
        {
          int delta = src0[x] - src1[x];
          dst[x] = clip( diffZero + delta * amplificationFactor, 0, maxVal);

          mseAdd[1] += delta * delta;
        }
        src0 += stride0;
        src1 += stride1;
        dst += dstStride;
      }

      // Chroma V
      src0 = (unsigned char*)currentFrameRawYUVData.data() + (frameSize.width() * frameSize.height() + (frameSize.width() / 2 * frameSize.height() / 2) );
      src1 = (unsigned char*)yuvItem2->currentFrameRawYUVData.data() + (yuvItem2->frameSize.width() * yuvItem2->frameSize.height()) + (yuvItem2->frameSize.width() / 2 * yuvItem2->frameSize.height() / 2);
      
      for (int y = 0; y < height / 2; y++)
      {
        for (int x = 0; x < width / 2; x++)
        {
          int delta = src0[x] - src1[x];
          dst[x] = clip( diffZero + delta * amplificationFactor, 0, maxVal);

          mseAdd[2] += delta * delta;
        }
        src0 += stride0;
        src1 += stride1;
        dst += dstStride;
      }
            
      // Convert to RGB
      convertYUV420ToRGB(diffYUV, tmpDiffBufferRGB, QSize(width,height));
    }
  }
  else
  {
    // One (or both) input item(s) is/are not 4:2:0
    diffType = "YUV 4:4:4";

    // How many values to go to the next line per input
    const unsigned int stride0 = frameSize.width();
    const unsigned int stride1 = yuvItem2->frameSize.width();
    const unsigned int dstStride = width;

    // How many values to go to the next component? (Luma,Cb,Cr)
    const unsigned int componentLength0 = frameSize.width() * frameSize.height();
    const unsigned int componentLength1 = yuvItem2->frameSize.width() * yuvItem2->frameSize.height();
    const unsigned int componentLengthDst = width * height;

    if (bps > 8)
    {
      // Resize the difference buffer
      diffYUV.resize( width * height * 3 * 2 );

      // For each component (Luma,Cb,Cr)...
      for (int c = 0; c < 3; c++)
      {
        // Two bytes per value. Get a pointer to the source data.
        unsigned short* src0 = (unsigned short*)tmpBufferYUV444.data() + c * componentLength0;
        unsigned short* src1 = (unsigned short*)yuvItem2->tmpBufferYUV444.data() + c * componentLength1;
        unsigned short* dst  = (unsigned short*)diffYUV.data() + c * componentLengthDst;

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
      diffYUV.resize( width * height * 3 );

      // For each component (Luma,Cb,Cr)...
      for (int c = 0; c < 3; c++)
      {
        // Get a pointer to the source data
        unsigned char* src0 = (unsigned char*)tmpBufferYUV444.data() + c * componentLength0;
        unsigned char* src1 = (unsigned char*)yuvItem2->tmpBufferYUV444.data() + c * componentLength1;
        unsigned char* dst  = (unsigned char*)diffYUV.data() + c * componentLengthDst;

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
    convertYUV4442RGB(diffYUV, tmpDiffBufferRGB);
  }

  // Append the conversion information that will be returned
  conversionInfoList.append( infoItem("Difference Type",diffType) );
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

ValuePairList videoHandlerYUV::getPixelValuesDifference(QPoint pixelPos, videoHandler *item2)
{
  videoHandlerYUV *yuvItem2 = dynamic_cast<videoHandlerYUV*>(item2);
  if (yuvItem2 == NULL)
    // The given item is not a yuv source. We cannot compare YUV values to non YUV values.
    // Call the base class comparison function to compare the items using the RGB values.
    return videoHandler::getPixelValuesDifference(pixelPos, item2);

  if (srcPixelFormat.bitsPerSample != yuvItem2->srcPixelFormat.bitsPerSample)
    // The two items have different bit depths. Compare RGB values instead.
    // TODO: Or should we do this in the YUV domain somehow?
    return videoHandler::getPixelValuesDifference(pixelPos, item2);

  int width  = qMin(frameSize.width(), yuvItem2->frameSize.width());
  int height = qMin(frameSize.height(), yuvItem2->frameSize.height());

  if (pixelPos.x() < 0 || pixelPos.x() >= width || pixelPos.y() < 0 || pixelPos.y() >= height)
    return ValuePairList();

  unsigned int Y0, U0, V0, Y1, U1, V1;
  getPixelValue(pixelPos, Y0, U0, V0);
  yuvItem2->getPixelValue(pixelPos, Y1, U1, V1);

  ValuePairList diffValues;
  diffValues.append( ValuePair("Y", QString::number((int)Y0-(int)Y1)) );
  diffValues.append( ValuePair("U", QString::number((int)U0-(int)U1)) );
  diffValues.append( ValuePair("V", QString::number((int)V0-(int)V1)) );
  
  return diffValues;
}

void videoHandlerYUV::drawPixelValues(QPainter *painter, unsigned int xMin, unsigned int xMax, unsigned int yMin, unsigned int yMax, double zoomFactor, videoHandler *item2)
{
  // Get the other YUV item (if any)
  videoHandlerYUV *yuvItem2 = NULL;
  if (item2 != NULL)
    yuvItem2 = dynamic_cast<videoHandlerYUV*>(item2);

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
        QString valText;
        if (yuvItem2 != NULL)
        {
          unsigned int Y0, U0, V0, Y1, U1, V1;
          getPixelValue(QPoint(x,y), Y0, U0, V0);
          yuvItem2->getPixelValue(QPoint(x,y), Y1, U1, V1);

          valText = QString("Y%1\nU%2\nV%3").arg(Y0-Y1).arg(U0-U1).arg(V0-V1);
          painter->setPen( (((int)Y0-(int)Y1) < whiteLimit) ? Qt::white : Qt::black );
        }
        else
        {
          unsigned int Y, U, V;
          getPixelValue(QPoint(x,y), Y, U, V);
          valText = QString("Y%1\nU%2\nV%3").arg(Y).arg(U).arg(V);
          painter->setPen( ((int)Y < whiteLimit) ? Qt::white : Qt::black );
        }

        painter->drawText(pixelRect, Qt::AlignCenter, valText);
      }
    }
  }
  else if (srcPixelFormat.subsamplingHorizontal <= 2 && srcPixelFormat.subsamplingVertical <= 2)
  {
    // Non YUV 444 format. The Y values go into the center of each pixel, but the U and V values go somewhere else,
    // depending on the srcPixelFormat.
    for (unsigned int x = (xMin == 0) ? 0 : xMin - 1; x <= xMax; x++)
    {
      for (unsigned int y = (yMin == 0) ? 0 : yMin - 1; y <= yMax; y++)
      {
        // Calculate the center point of the pixel. (Each pixel is of size (zoomFactor,zoomFactor)) and move the pixelRect to that point.
        QPoint pixCenter = centerPointZero + QPoint(x * zoomFactor, y * zoomFactor);
        pixelRect.moveCenter(pixCenter);

        // Get the text to show

        int Y,U,V;
        if (yuvItem2 != NULL)
        {
          unsigned int Y0, U0, V0, Y1, U1, V1;
          getPixelValue(QPoint(x,y), Y0, U0, V0);
          yuvItem2->getPixelValue(QPoint(x,y), Y1, U1, V1);

          Y = Y0-Y1; U = U0-U1; V = V0-V1;
        }
        else
        {
          unsigned int Yu,Uu,Vu;
          getPixelValue(QPoint(x,y), Yu, Uu, Vu);
          Y = Yu; U = Uu; V = Vu;
        }

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

bool videoHandlerYUV::isPixelDark(QPoint pixelPos)
{
  unsigned int Y0, U0, V0;
  getPixelValue(pixelPos, Y0, U0, V0);
  int whiteLimit = 1 << (srcPixelFormat.bitsPerSample - 1);
  return Y0 < whiteLimit;
}

void videoHandlerYUV::setFormatFromSize(QSize size, int bitDepth, qint64 fileSize, QString subFormat)
{
  // If the bit depth could not be determined, check 8 and 10 bit
  int testBitDepths = (bitDepth > 0) ? 1 : 2;

  for (int i = 0; i < testBitDepths; i++)
  {
    if (testBitDepths == 2)
      bitDepth = (i == 0) ? 8 : 10;

    if (bitDepth==8)
    {
      // assume 4:2:0 if subFormat does not indicate anything else
      yuvPixelFormat cFormat = yuvFormatList.getFromName( "4:2:0 Y'CbCr 8-bit planar" );
      if (subFormat == "444")
        cFormat = yuvFormatList.getFromName( "4:4:4 Y'CbCr 8-bit planar" );
      if (subFormat == "422")
        cFormat = yuvFormatList.getFromName( "4:2:2 Y'CbCr 8-bit planar" );

      // Check if the file size and the assumed format match
      int bpf = cFormat.bytesPerFrame( size );
      if (bpf != 0 && (fileSize % bpf) == 0)
      {
        // Bits per frame and file size match
        frameSize = size;
        setSrcPixelFormat( cFormat );
        return;
      }
    }
    else if (bitDepth==10)
    {
      // Assume 444 format if subFormat is set. Otherwise assume 420
      yuvPixelFormat cFormat = yuvFormatList.getFromName( "4:2:0 Y'CbCr 10-bit LE planar" );
      if (subFormat == "444")
        cFormat = yuvFormatList.getFromName( "4:4:4 Y'CbCr 10-bit LE planar" );
      
      // Check if the file size and the assumed format match
      int bpf = cFormat.bytesPerFrame( size );
      if (bpf != 0 && (fileSize % bpf) == 0)
      {
        // Bits per frame and file size match
        frameSize = size;
        setSrcPixelFormat( cFormat );
        return;
      }
    }
    else
    {
        // do other stuff
    }
  }
}

/** Try to guess the format of the raw YUV data. A list of candidates is tried (candidateModes) and it is checked if
  * the file size matches and if the correlation of the first two frames is below a threshold.
  * radData must contain at least two frames of the video sequence. Only formats that two frames of could fit into rawData
  * are tested. E.g. the biggest format that is tested here is 1080p YUV 4:2:2 8 bit which is 4147200 bytes per frame. So
  * make shure rawData contains 8294400 bytes so that all formats are tested.
  * If a file size is given, we test if the candidates frame size is a multiple of the fileSize. If fileSize is -1, this test
  * is skipped.
  */
void videoHandlerYUV::setFormatFromCorrelation(QByteArray rawYUVData, qint64 fileSize)
{
  unsigned char *ptr;
  float leastMSE, mse;
  int   bestMode;

  // step1: file size must be a multiple of w*h*(color format)
  qint64 picSize;

  if(rawYUVData.size() < 1)
    return;

  // Define the structure for each candidate mode
  // The definition is here to not pollute any other namespace unnecessarily
  typedef struct {
    QSize   frameSize;
    QString pixelFormatName;

    // flags set while checking
    bool  interesting;
    float mseY;
  } candMode_t;

  // Fill the list of possible candidate modes
  candMode_t candidateModes[] = {
    {QSize(176,144),"4:2:0 Y'CbCr 8-bit planar",false, 0.0 },
    {QSize(352,240),"4:2:0 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(352,288),"4:2:0 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(480,480),"4:2:0 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(480,576),"4:2:0 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(704,480),"4:2:0 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(720,480),"4:2:0 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(704,576),"4:2:0 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(720,576),"4:2:0 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(1024,768),"4:2:0 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(1280,720),"4:2:0 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(1280,960),"4:2:0 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(1920,1072),"4:2:0 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(1920,1080),"4:2:0 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(176,144),"4:2:2 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(352,240),"4:2:2 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(352,288),"4:2:2 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(480,480),"4:2:2 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(480,576),"4:2:2 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(704,480),"4:2:2 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(720,480),"4:2:2 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(720,486),"4:2:2 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(704,576),"4:2:2 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(720,576),"4:2:2 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(1024,768),"4:2:2 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(1280,720),"4:2:2 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(1280,960),"4:2:2 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(1920,1072),"4:2:2 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(1920,1080),"4:2:2 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(), "Unknown Pixel Format", false, 0.0 }
  };

  if (fileSize > 0)
  {
    // if any candidate exceeds file size for two frames, discard
    // if any candidate does not represent a multiple of file size, discard
    int i = 0;
    bool found = false;
    while( candidateModes[i].pixelFormatName != "Unknown Pixel Format" )
    {
      /* one pic in bytes */
      yuvPixelFormat pixelFormat = yuvFormatList.getFromName( candidateModes[i].pixelFormatName );
      picSize = pixelFormat.bytesPerFrame( candidateModes[i].frameSize );

      if( fileSize >= (picSize*2) )    // at least 2 pics for correlation analysis
      {
        if( (fileSize % picSize) == 0 ) // important: file size must be multiple of pic size
        {
          candidateModes[i].interesting = true; // test passed
          found = true;
        }
      }

      i++;
    };

    if( !found )
      // No candidate matches the file size
      return;
  }

  // calculate max. correlation for first two frames, use max. candidate frame size
  int i=0;
  while( candidateModes[i].pixelFormatName != "Unknown Pixel Format" )
  {
    if( candidateModes[i].interesting )
    {
      yuvPixelFormat pixelFormat = yuvFormatList.getFromName( candidateModes[i].pixelFormatName );
      picSize = pixelFormat.bytesPerFrame( candidateModes[i].frameSize );

      // assumptions: YUV is planar (can be changed if necessary)
      // only check mse in luminance
      ptr  = (unsigned char*) rawYUVData.data();
      candidateModes[i].mseY = computeMSE( ptr, ptr + picSize, picSize );
    }
    i++;
  };

  // step3: select best candidate
  i=0;
  leastMSE = std::numeric_limits<float>::max(); // large error...
  bestMode = 0;
  while( candidateModes[i].pixelFormatName != "Unknown Pixel Format" )
  {
    if( candidateModes[i].interesting )
    {
      mse = (candidateModes[i].mseY);
      if( mse < leastMSE )
      {
        bestMode = i;
        leastMSE = mse;
      }
    }
    i++;
  };

  if( leastMSE < 400 )
  {
    // MSE is below threshold. Choose the candidate.
    srcPixelFormat = yuvFormatList.getFromName( candidateModes[bestMode].pixelFormatName );
    frameSize = candidateModes[bestMode].frameSize;
  }
}

void videoHandlerYUV::loadFrame(int frameIndex)
{
  DEBUG_YUV( "videoHandlerYUV::loadFrame %d\n", frameIndex );

  if (!isFormatValid())
    // We cannot load a frame if the format is not known
    return;

  // Does the data in currentFrameRawYUVData need to be updated?
  if (!loadRawYUVData(frameIndex))
    // Loading failed 
    return;

  // The data in currentFrameRawYUVData is now up to date. If necessary
  // convert the data to RGB.
  if (currentFrameIdx != frameIndex)
  {
    convertYUVToPixmap(currentFrameRawYUVData, currentFrame, tmpBufferRGB, tmpBufferYUV444);
    currentFrameIdx = frameIndex;
  }
}

void videoHandlerYUV::loadFrameForCaching(int frameIndex, QPixmap &frameToCache)
{
  DEBUG_YUV( "videoHandlerYUV::loadFrameForCaching %d", frameIndex );

  // Lock the mutex for the yuvFormat. The main thread has to wait until caching is done
  // before the yuv format can change.
  cachingMutex.lock();

  rawDataMutex.lock();
  emit signalRequesRawData(frameIndex);
  tmpBufferRawYUVDataCaching = rawData;
  rawDataMutex.unlock();

  if (frameIndex != rawData_frameIdx)
  {
    // Loading failed
    currentFrameIdx = -1;
    cachingMutex.unlock();
    return;
  }

  // Convert YUV to pixmap. This can then be cached.
  convertYUVToPixmap(tmpBufferRawYUVDataCaching, frameToCache, tmpBufferRGBCaching, tmpBufferYUV444Caching);

  cachingMutex.unlock();
}

// Load the raw YUV data for the given frame index into currentFrameRawYUVData.
bool videoHandlerYUV::loadRawYUVData(int frameIndex)
{
  if (currentFrameRawYUVData_frameIdx == frameIndex)
    // Buffer already up to date
    return true;

  DEBUG_YUV( "videoHandlerYUV::loadRawYUVData %d", frameIndex );

  // The function loadFrameForCaching also uses the signalRequesRawYUVData to request raw data.
  // However, only one thread can use this at a time.
  rawDataMutex.lock();
  emit signalRequesRawData(frameIndex);

  if (frameIndex != rawData_frameIdx)
  {
    // Loading failed
    currentFrameRawYUVData_frameIdx = -1;
  }
  else
  {
    currentFrameRawYUVData = rawData;
    currentFrameRawYUVData_frameIdx = frameIndex;
  }

  rawDataMutex.unlock();
  return (currentFrameRawYUVData_frameIdx == frameIndex);
}

// Convert the given raw YUV data in sourceBuffer (using srcPixelFormat) to pixmap (RGB-888), using the
// buffer tmpRGBBuffer for intermediate RGB values.
void videoHandlerYUV::convertYUVToPixmap(QByteArray sourceBuffer, QPixmap &outputPixmap, QByteArray &tmpRGBBuffer, QByteArray &tmpYUV444Buffer)
{
  DEBUG_YUV( "videoHandlerYUV::convertYUVToPixmap" );

  if (srcPixelFormat == "4:2:0 Y'CbCr 8-bit planar" && !yuvMathRequired() && interpolationMode == NearestNeighborInterpolation &&
      frameSize.width() % 2 == 0 && frameSize.height() % 2 == 0)
      // TODO: For YUV4:2:0, an un-even width or height does not make a lot of sense. Or what should be done in this case?
      // Is there even a definition for this? The correct is probably to not allow this and enforce divisibility by 2.
  {
    // directly convert from 420 to RGB
    convertYUV420ToRGB(sourceBuffer, tmpRGBBuffer);
  }
  else
  {
    // First, convert the buffer to YUV 444
    convert2YUV444(sourceBuffer, tmpYUV444Buffer);

    // Apply transformations to the YUV components (if any are set)
    // TODO: Shouldn't this be done before the conversion to 444?
    applyYUVTransformation( tmpYUV444Buffer );

    // Convert to RGB888
    convertYUV4442RGB(tmpYUV444Buffer, tmpRGBBuffer);
  }

  // Convert the image in tmpRGBBuffer to a QPixmap using a QImage intermediate.
  // TODO: Isn't there a faster way to do this? Maybe load a pixmap from "BMP"-like data?
#if SSE_CONVERSION_420_ALT
  // RGB32 => 0xffRRGGBB
  QImage tmpImage((unsigned char*)tmpRGBBuffer.data(), frameSize.width(), frameSize.height(), QImage::Format_RGB32);
#else
  QImage tmpImage((unsigned char*)tmpRGBBuffer.data(), frameSize.width(), frameSize.height(), QImage::Format_RGB888);
#endif

  // Set the videoHanlder pixmap and image so the videoHandler can draw the item
  outputPixmap.convertFromImage(tmpImage);
}

void videoHandlerYUV::getPixelValue(QPoint pixelPos, unsigned int &Y, unsigned int &U, unsigned int &V)
{
  // Update the raw YUV data if necessary
  loadRawYUVData(currentFrameIdx);

  // Get the YUV data from the currentFrameRawYUVData
  const unsigned int offsetCoordinateY  = frameSize.width() * pixelPos.y() + pixelPos.x();
  const unsigned int offsetCoordinateUV = (frameSize.width() / srcPixelFormat.subsamplingHorizontal * (pixelPos.y() / srcPixelFormat.subsamplingVertical)) + pixelPos.x() / srcPixelFormat.subsamplingHorizontal;
  const unsigned int planeLengthY  = frameSize.width() * frameSize.height();
  const unsigned int planeLengthUV = frameSize.width() / srcPixelFormat.subsamplingHorizontal * frameSize.height() / srcPixelFormat.subsamplingVertical;
  if (srcPixelFormat.bitsPerSample > 8)
  {
    // TODO: Test for 10-bit. This is probably wrong.

    unsigned short* srcY = (unsigned short*)currentFrameRawYUVData.data();
    unsigned short* srcU = (unsigned short*)currentFrameRawYUVData.data() + planeLengthY;
    unsigned short* srcV = (unsigned short*)currentFrameRawYUVData.data() + planeLengthY + planeLengthUV;

    Y = (unsigned int)(*(srcY + offsetCoordinateY));
    U = (unsigned int)(*(srcU + offsetCoordinateUV));
    V = (unsigned int)(*(srcV + offsetCoordinateUV));
  }
  else
  {
    unsigned char* srcY = (unsigned char*)currentFrameRawYUVData.data();
    unsigned char* srcU = (unsigned char*)currentFrameRawYUVData.data() + planeLengthY;
    unsigned char* srcV = (unsigned char*)currentFrameRawYUVData.data() + planeLengthY + planeLengthUV;

    Y = (unsigned int)(*(srcY + offsetCoordinateY));
    U = (unsigned int)(*(srcU + offsetCoordinateUV));
    V = (unsigned int)(*(srcV + offsetCoordinateUV));
  }
}

// Convert 8-bit YUV 4:2:0 to RGB888 using NearestNeighborInterpolation
#if SSE_CONVERSION
void videoHandlerYUV::convertYUV420ToRGB(byteArrayAligned &sourceBuffer, byteArrayAligned &targetBuffer)
#else
void videoHandlerYUV::convertYUV420ToRGB(QByteArray &sourceBuffer, QByteArray &targetBuffer, QSize size)
#endif
{
  int frameWidth, frameHeight;
  if (size.isValid())
  {
    frameWidth = size.width();
    frameHeight = size.height();
  }
  else
  {
    frameWidth = frameSize.width();
    frameHeight = frameSize.height();
  }

  // Round down the width and height to even values. Uneven values are not
  // possible for a 4:2:0 format.
  if (frameWidth % 2 != 0)
    frameWidth -= 1;
  if (frameHeight % 2 != 0)
    frameHeight -= 1;

  int componentLenghtY  = frameWidth * frameHeight;
  int componentLengthUV = componentLenghtY >> 2;
  Q_ASSERT( sourceBuffer.size() >= componentLenghtY + componentLengthUV+ componentLengthUV ); // YUV 420 must be (at least) 1.5*Y-area

  // Resize target buffer if necessary
#if SSE_CONVERSION_420_ALT
  int targetBufferSize = frameWidth * frameHeight * 4;
#else
  int targetBufferSize = frameWidth * frameHeight * 3;
#endif
  if( targetBuffer.size() != targetBufferSize)
    targetBuffer.resize(targetBufferSize);

#if SSE_CONVERSION_420_ALT
  quint8 *srcYRaw = (quint8*) sourceBuffer.data();
  quint8 *srcURaw = srcYRaw + componentLenghtY;
  quint8 *srcVRaw = srcURaw + componentLengthUV;

  quint8 *dstBuffer = (quint8*)targetBuffer.data();
  quint32 dstBufferStride = frameWidth*4;

  yuv420_to_argb8888(srcYRaw,srcURaw,srcVRaw,frameWidth,frameWidth>>1,frameWidth,frameHeight,dstBuffer,dstBufferStride);
  return;
#endif

#if SSE_CONVERSION
  // Try to use SSE. If this fails use conventional algorithm

  if (frameWidth % 32 == 0 && frameHeight % 2 == 0)
  {
    // We can use 16byte aligned read/write operations

    quint8 *srcY = (quint8*) sourceBuffer.data();
    quint8 *srcU = srcY + componentLenghtY;
    quint8 *srcV = srcU + componentLengthUV;

    __m128i yMult  = _mm_set_epi16(75, 75, 75, 75, 75, 75, 75, 75);
    __m128i ySub   = _mm_set_epi16(16, 16, 16, 16, 16, 16, 16, 16);
    __m128i ugMult = _mm_set_epi16(25, 25, 25, 25, 25, 25, 25, 25);
    //__m128i sub16  = _mm_set_epi8(16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16);
    __m128i sub128 = _mm_set_epi8(128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128);

    //__m128i test = _mm_set_epi8(128, 0, 1, 2, 3, 245, 254, 255, 128, 128, 128, 128, 128, 128, 128, 128);

    __m128i y, u, v, uMult, vMult;
    __m128i RGBOut0, RGBOut1, RGBOut2;
    __m128i tmp;

    for (int yh=0; yh < frameHeight / 2; yh++)
    {
      for (int x=0; x < frameWidth / 32; x+=32)
      {
        // Load 16 bytes U/V
        u  = _mm_load_si128((__m128i *) &srcU[x / 2]);
        v  = _mm_load_si128((__m128i *) &srcV[x / 2]);
        // Subtract 128 from each U/V value (16 values)
        u = _mm_sub_epi8(u, sub128);
        v = _mm_sub_epi8(v, sub128);

        // Load 16 bytes Y from this line and the next one
        y = _mm_load_si128((__m128i *) &srcY[x]);

        // Get the lower 8 (8bit signed) Y values and put them into a 16bit register
        tmp = _mm_srai_epi16(_mm_unpacklo_epi8(y, y), 8);
        // Subtract 16 and multiply by 75
        tmp = _mm_sub_epi16(tmp, ySub);
        tmp = _mm_mullo_epi16(tmp, yMult);

        // Now to add them to the 16 bit RGB output values
        RGBOut0 = _mm_shuffle_epi32(tmp, _MM_SHUFFLE(1, 0, 1, 0));
        RGBOut0 = _mm_shufflelo_epi16(RGBOut0, _MM_SHUFFLE(1, 0, 0, 0));
        RGBOut0 = _mm_shufflehi_epi16(RGBOut0, _MM_SHUFFLE(2, 2, 1, 1));

        RGBOut1 = _mm_shuffle_epi32(tmp, _MM_SHUFFLE(2, 1, 2, 1));
        RGBOut1 = _mm_shufflelo_epi16(RGBOut1, _MM_SHUFFLE(1, 1, 1, 0));
        RGBOut1 = _mm_shufflehi_epi16(RGBOut1, _MM_SHUFFLE(3, 2, 2, 2));

        RGBOut2 = _mm_shuffle_epi32(tmp, _MM_SHUFFLE(3, 2, 3, 2));
        RGBOut2 = _mm_shufflelo_epi16(RGBOut2, _MM_SHUFFLE(2, 2, 1, 1));
        RGBOut2 = _mm_shufflehi_epi16(RGBOut2, _MM_SHUFFLE(3, 3, 3, 2));

        //y2 = _mm_load_si128((__m128i *) &srcY[x + 16]);

        // --- Start with the left 8 values from U/V

        // Get the lower 8 (8bit signed) U/V values and put them into a 16bit register
        uMult = _mm_srai_epi16(_mm_unpacklo_epi8(u, u), 8);
        vMult = _mm_srai_epi16(_mm_unpacklo_epi8(v, v), 8);

        // Multiply


        /*y3 = _mm_load_si128((__m128i *) &srcY[x + frameWidth]);
        y4 = _mm_load_si128((__m128i *) &srcY[x + frameWidth + 16]);*/
      }
    }



    return;
  }
#endif

  // Perform software based 420 to RGB conversion
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

  const int yOffset = 16;
  const int cZero = 128;
  //const int rgbMax = (1<<8)-1;
  int yMult, rvMult, guMult, gvMult, buMult;

  unsigned char *dst = (unsigned char*)targetBuffer.data();
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
  const unsigned char * restrict srcU = srcY + componentLenghtY;
  const unsigned char * restrict srcV = srcU + componentLengthUV;
  unsigned char * restrict dstMem = dst;

  int yh;
#if __MINGW32__ || __GNUC__
#pragma omp parallel for default(none) private(yh) shared(srcY,srcU,srcV,dstMem,yMult,rvMult,guMult,gvMult,buMult,clip_buf,frameWidth,frameHeight)// num_threads(2)
#else
#pragma omp parallel for default(none) private(yh) shared(srcY,srcU,srcV,dstMem,yMult,rvMult,guMult,gvMult,buMult,clip_buf,frameWidth,frameHeight)// num_threads(2)
#endif
  for (yh=0; yh < frameHeight / 2; yh++)
  {
    // Process two lines at once, always 4 RGB values at a time (they have the same U/V components)

    int dstAddr1 = yh * 2 * frameWidth * 3;         // The RGB output adress of line yh*2
    int dstAddr2 = (yh * 2 + 1) * frameWidth * 3;   // The RGB output adress of line yh*2+1
    int srcAddrY1 = yh * 2 * frameWidth;            // The Y source address of line yh*2
    int srcAddrY2 = (yh * 2 + 1) * frameWidth;      // The Y source address of line yh*2+1
    int srcAddrUV = yh * frameWidth / 2;            // The UV source address of both lines (UV are identical)

    for (int xh=0, x=0; xh < frameWidth / 2; xh++, x+=2)
    {
      // Process four pixels (the ones for which U/V are valid

      // Load UV and premultiply
      const int U_tmp_G = ((int)srcU[srcAddrUV + xh] - cZero) * guMult;
      const int U_tmp_B = ((int)srcU[srcAddrUV + xh] - cZero) * buMult;
      const int V_tmp_R = ((int)srcV[srcAddrUV + xh] - cZero) * rvMult;
      const int V_tmp_G = ((int)srcV[srcAddrUV + xh] - cZero) * gvMult;

      // Pixel top left
      {
        const int Y_tmp = ((int)srcY[srcAddrY1 + x] - yOffset) * yMult;

        const int R_tmp = (Y_tmp           + V_tmp_R ) >> 16;
        const int G_tmp = (Y_tmp + U_tmp_G + V_tmp_G ) >> 16;
        const int B_tmp = (Y_tmp + U_tmp_B           ) >> 16;

        dstMem[dstAddr1]   = clip_buf[R_tmp];
        dstMem[dstAddr1+1] = clip_buf[G_tmp];
        dstMem[dstAddr1+2] = clip_buf[B_tmp];
        dstAddr1 += 3;
      }
      // Pixel top right
      {
        const int Y_tmp = ((int)srcY[srcAddrY1 + x + 1] - yOffset) * yMult;

        const int R_tmp = (Y_tmp           + V_tmp_R ) >> 16;
        const int G_tmp = (Y_tmp + U_tmp_G + V_tmp_G ) >> 16;
        const int B_tmp = (Y_tmp + U_tmp_B           ) >> 16;

        dstMem[dstAddr1]   = clip_buf[R_tmp];
        dstMem[dstAddr1+1] = clip_buf[G_tmp];
        dstMem[dstAddr1+2] = clip_buf[B_tmp];
        dstAddr1 += 3;
      }
      // Pixel bottom left
      {
        const int Y_tmp = ((int)srcY[srcAddrY2 + x] - yOffset) * yMult;

        const int R_tmp = (Y_tmp           + V_tmp_R ) >> 16;
        const int G_tmp = (Y_tmp + U_tmp_G + V_tmp_G ) >> 16;
        const int B_tmp = (Y_tmp + U_tmp_B           ) >> 16;

        dstMem[dstAddr2]   = clip_buf[R_tmp];
        dstMem[dstAddr2+1] = clip_buf[G_tmp];
        dstMem[dstAddr2+2] = clip_buf[B_tmp];
        dstAddr2 += 3;
      }
      // Pixel bottom right
      {
        const int Y_tmp = ((int)srcY[srcAddrY2 + x + 1] - yOffset) * yMult;

        const int R_tmp = (Y_tmp           + V_tmp_R ) >> 16;
        const int G_tmp = (Y_tmp + U_tmp_G + V_tmp_G ) >> 16;
        const int B_tmp = (Y_tmp + U_tmp_B           ) >> 16;

        dstMem[dstAddr2]   = clip_buf[R_tmp];
        dstMem[dstAddr2+1] = clip_buf[G_tmp];
        dstMem[dstAddr2+2] = clip_buf[B_tmp];
        dstAddr2 += 3;
      }
    }
  }
}

void videoHandlerYUV::setSrcPixelFormatByName(QString name, bool emitSignal)
{
  yuvPixelFormat newSrcPixelFormat = yuvFormatList.getFromName(name);
  if (newSrcPixelFormat != srcPixelFormat)
  {
    if (controlsCreated)
      disconnect(ui->yuvFileFormatComboBox, SIGNAL(currentIndexChanged(int)), NULL, NULL);

    setSrcPixelFormat( yuvFormatList.getFromName(name) );
    int idx = yuvFormatList.indexOf( srcPixelFormat );
    
    if (controlsCreated)
    {
      ui->yuvFileFormatComboBox->setCurrentIndex( idx );
      connect(ui->yuvFileFormatComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotYUVControlChanged()));
    }

    // Clear the cache
    if (pixmapCache.count() > 0)
      pixmapCache.clear();

    if (emitSignal)
      emit signalHandlerChanged(true, true);
  }
}

void videoHandlerYUV::setFrameSize(QSize size, bool emitSignal)
{
  if (size != frameSize)
  {
    currentFrameRawYUVData_frameIdx = -1;
    currentFrameIdx = -1;
  }

  videoHandler::setFrameSize(size, emitSignal);
}
