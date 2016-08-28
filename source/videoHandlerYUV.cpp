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
using namespace YUV_Internals;

#include "stdio.h"
#include <xmmintrin.h>

#include <QtEndian>
#include <QTime>
#include <QLabel>
#include <QGroupBox>
#include <QPainter>
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

// Restrict is basically a promise to the compiler that for the scope of the pointer, the target of the pointer will only be accessed through that pointer (and pointers copied from it).
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

// Compute the MSE between the given char sources for numPixels bytes
template<typename T>
double computeMSE( T ptr, T ptr2, int numPixels )
{
  if (numPixels <= 0)
    return 0.0;

  quint64 sad = 0;
  for(int i=0; i<numPixels; i++)
  {
    int diff = (int)ptr[i] - (int)ptr2[i];
    sad += diff*diff;
  }

  return (double)sad / numPixels;
}

namespace YUV_Internals
{
  yuvPixelFormat::yuvPixelFormat(QString name)
  {
    QRegExp rxYUVFormat("([YUVA]{3,6}) (4:[420]{1}:[420]{1}) ([0-9]{1,2})-bit[ ]?([BL]{1}E)?[ ]?(packed|packed-B)?");

    if (rxYUVFormat.exactMatch(name))
    {
      yuvPixelFormat newFormat;

      // Is this a packed format or not?
      QString packed = rxYUVFormat.cap(5);
      newFormat.planar = (packed == "");
      if (!newFormat.planar)
        newFormat.bytePacking = (packed == "packed-B");
      
      // Parse the YUV order (planar or packed)
      if (newFormat.planar)
      {
        static const QStringList orderNames = QStringList() << "YUV" << "YVU" << "YUVA" << "YVUA";
        int idx = orderNames.indexOf(rxYUVFormat.cap(1));
        if (idx == -1)
          return;
        newFormat.planeOrder = static_cast<YUVPlaneOrder>(idx);
      }
      else
      {
        static const QStringList orderNames = QStringList() << "YUV" << "YVU" << "AYUV" << "YUVA" << "UYVU" << "VYUY" << "YUYV" << "YVYU" << "YYYYUV" << "YYUYYV" << "UYYVYY" << "VYYUYY";
        int idx = orderNames.indexOf(rxYUVFormat.cap(1));
        if (idx == -1)
          return;
        newFormat.packingOrder = static_cast<YUVPackingOrder>(idx);
      }

      // Parse the subsampling
      QStringList yuvSubsamplings = QStringList() << "4:4:4" << "4:2:2" << "4:2:0" << "4:4:0" << "4:1:0" << "4:1:1" << "4:0:0";
      int idx = yuvSubsamplings.indexOf(rxYUVFormat.cap(2));
      if (idx == -1)
        return;
      newFormat.subsampling = static_cast<YUVSubsamplingType>(idx);

      // Get the bit depth
      bool ok;
      int bitDepth = rxYUVFormat.cap(3).toInt(&ok);
      if (!ok || bitDepth <= 0)
        return;
      newFormat.bitsPerSample = bitDepth;

      // Get the endianess. If not in the name, assume LE
      newFormat.bigEndian = (rxYUVFormat.cap(4) == "BE");

      // Check if the format is valid.
      if (newFormat.isValid())
      {
        // Set all the values from the new format
        subsampling = newFormat.subsampling;
        bitsPerSample = newFormat.bitsPerSample;
        bigEndian = newFormat.bigEndian;
        planar = newFormat.planar;
        planeOrder = newFormat.planeOrder;
        packingOrder = newFormat.packingOrder;
        bytePacking = newFormat.bytePacking;
      }
    }
  }

  bool yuvPixelFormat::isValid() const
  {
    if (!planar)
    {
      // Check the packing mode
      if ((packingOrder == Packing_YUV || packingOrder == Packing_YVU) && subsampling != YUV_444)
        return false;
      if ((packingOrder == Packing_UYVY || packingOrder == Packing_VYUY || packingOrder == Packing_YUYV || packingOrder == Packing_YVYU) && subsampling == YUV_422)
        return false;
      if ((packingOrder == Packing_YYYYUV || packingOrder == Packing_YYUYYV || packingOrder == Packing_UYYVYY || packingOrder == Packing_VYYUYY) && subsampling == YUV_420)
        return false;
    }
    if (bitsPerSample <= 0)
      return false;
    return true;
  }

  // Generate a unique name for the YUV format
  QString yuvPixelFormat::getName() const
  {
    if (!isValid())
      return "Invalid";

    QString name;

    // Start with the YUV order
    if (planar)
    {
      int idx = int(planeOrder);
      static const QString orderNames[] = {"YUV", "YVU", "YUVA", "YVUA"};
      Q_ASSERT(idx >= 0 && idx < 4);
      name += orderNames[idx];
    }
    else
    {
      int idx = int(packingOrder);
      static const QString orderNames[] = {"YUV", "YVU", "AYUV", "YUVA", "UYVU", "VYUY", "YUYV", "YVYU", "YYYYUV", "YYUYYV", "UYYVYY", "VYYUYY"};
      Q_ASSERT(idx >= 0 && idx < 12);
      name += orderNames[idx];
    }

    // Next add the subsampling
    if (subsampling == YUV_444)
      name += " 4:4:4";
    else if (subsampling == YUV_422)
      name += " 4:2:2";
    else if (subsampling == YUV_420)
      name += " 4:2:0";
    else if (subsampling == YUV_440)
      name += " 4:4:0";
    else if (subsampling == YUV_410)
      name += " 4:1:0";
    else if (subsampling == YUV_411)
      name += " 4:1:1";
    else if (subsampling == YUV_400)
      name += " 4:0:0";
    else
      assert(false);  // Somebody forgot to add a known format here

    // Add the bits
    name += QString(" %1-bit").arg(bitsPerSample);

    // Add the endianess (if the bit depth is greater 8)
    if (bitsPerSample > 8)
      name += (bigEndian) ? " BE" : " LE";

    if (!planar)
      name += bytePacking ? " packed-B" : " packed";
     
    return name;
  }

  qint64 yuvPixelFormat::bytesPerFrame(QSize frameSize) const
  {
    qint64 bytes = 0;
    if (planar)
    {
      // Add the bytes of the 3 (or 4) planes
      int bytesPerSample = (bitsPerSample + 7) / 8; // Round to bytes
      bytes += frameSize.width() * frameSize.height() * bytesPerSample; // Luma plane
      if (subsampling == YUV_444)
        bytes += frameSize.width() * frameSize.height() * bytesPerSample * 2; // U/V planes
      else if (subsampling == YUV_422 || subsampling == YUV_440)
        bytes += (frameSize.width() / 2) * frameSize.height() * bytesPerSample * 2; // U/V planes, half the width
      else if (subsampling == YUV_420)
        bytes += (frameSize.width() / 2) * (frameSize.height() / 2) * bytesPerSample * 2; // U/V planes, half the width and height
      else if (subsampling == YUV_410)
        bytes += (frameSize.width() / 4) * (frameSize.height() / 4) * bytesPerSample * 2; // U/V planes, half the width and height
      else if (subsampling == YUV_411)
        bytes += (frameSize.width() / 4) * frameSize.height() * bytesPerSample * 2; // U/V planes, quarter the width
      else if (subsampling == YUV_400)
        bytes += 0; // No chroma components
      else
        return -1;  // Unknown subsampling

      if (planeOrder == Order_YUVA || planeOrder == Order_YVUA)
        // There is an additional alpha plane. The alpha plane is not subsampled
        bytes += frameSize.width() * frameSize.height() * bytesPerSample; // Alpha plane
    }
    else
    {
      // This is a packed format. The added number of bytes might be lower because of the packing.
      if (subsampling == YUV_444)
      {
        int bitsPerPixel = bitsPerSample * 3;
        if (packingOrder == Packing_AYUV || packingOrder == Packing_YUVA)
          bitsPerPixel += bitsPerSample;
        return ((bitsPerPixel + 7) / 8) * frameSize.width() * frameSize.height();
      }
      else if (subsampling == YUV_422 || subsampling == YUV_440)
      {
        // All packing orders have 4 values per packed value (which has 2 Y samples)
        int bitsPerPixel = bitsPerSample * 4;
        return ((bitsPerPixel + 7) / 8) * (frameSize.width() / 2) * frameSize.height();
      }
      else if (subsampling == YUV_420)
      {
        // All packing orders have 6 values per packed sample (which has 4 Y samples)
        int bitsPerPixel = bitsPerSample * 6;
        return ((bitsPerPixel + 7) / 8) * (frameSize.width() / 2) * (frameSize.height() / 2);
      }
      else
        return -1;  // Unknown subsampling
    }
    return bytes;
  }

  int yuvPixelFormat::getSubsamplingHor() const
  { 
    if (subsampling == YUV_410 || subsampling == YUV_411)
      return 4;
    if (subsampling == YUV_422 || subsampling == YUV_420)
      return 2;
    return 1;
  }
  int yuvPixelFormat::getSubsamplingVer() const
  {
    if (subsampling == YUV_410)
      return 4;
    if (subsampling == YUV_420 || subsampling == YUV_440) 
      return 2;
    return 1;
  }

  videoHandlerYUV_CustomFormatDialog::videoHandlerYUV_CustomFormatDialog(yuvPixelFormat yuvFormat)
  {
    setupUi(this);

    // Set all values correctly from the given yuvFormat
    // Set the chroma subsampling
    int idx = yuvFormat.subsampling;
    if (idx >= 0 && idx < 3)
      comboBoxChromaSubsampling->setCurrentIndex(idx);

    // Set the bit depth
    idx = yuvFormat.bitsPerSample;
    if (idx >= 0 && idx < 6)
      comboBoxBitDepth->setCurrentIndex(idx);

    // Set the endianess
    comboBoxEndianess->setCurrentIndex(yuvFormat.bigEndian ? 0 : 1);

    if (yuvFormat.planar)
    {
      groupBoxPlanar->setChecked(true);
      idx = yuvFormat.planeOrder;
      if (idx >= 0 && idx < 4)
        comboBoxPlaneOrder->setCurrentIndex(idx);
    }
    else
    {
      groupBoxPacked->setChecked(true);
      idx = yuvFormat.packingOrder;
      // The index in the combo box depends on the subsampling
      if (yuvFormat.subsampling == YUV_422 || yuvFormat.subsampling == YUV_440)
        idx -= int(Packing_UYVY);       // The first packing format for 422
      else if (yuvFormat.packingOrder == YUV_420)
        idx -= int(Packing_YYYYUV);     // The first packing format for 420

      if (idx >= 0)
        comboBoxPackingOrder->setCurrentIndex(idx);
    }
  }

  // The default constructor of the YUVFormatList will fill the list with some of the supported YUV file formats.
  YUVFormatList::YUVFormatList()
  {
    append( yuvPixelFormat(YUV_420, 8, Order_YUV) ); // YUV 4:2:0
    append( yuvPixelFormat(YUV_422, 8, Order_YUV) ); // YUV 4:2:2
    append( yuvPixelFormat(YUV_444, 8, Order_YUV) ); // YUV 4:4:4
  }

  // Put all the names of the YUVFormatList into a list and return it
  QStringList YUVFormatList::getFormatedNames()
  {
    QStringList l;
    for (int i = 0; i < count(); i++)
    {
      l.append( at(i).getName() );
    }
    return l;
  }

  void videoHandlerYUV_CustomFormatDialog::on_comboBoxChromaSubsampling_currentIndexChanged(int idx)
  {
    comboBoxPackingOrder->clear();
    if (idx == YUV_444)
    {
      comboBoxPackingOrder->addItem("YUV");
      comboBoxPackingOrder->addItem("YVU");
      comboBoxPackingOrder->addItem("AYUV");
      comboBoxPackingOrder->addItem("YUVA");
    }
    else if (idx == YUV_422 || idx == YUV_440)
    {
      comboBoxPackingOrder->addItem("UYVY");
      comboBoxPackingOrder->addItem("VYUY");
      comboBoxPackingOrder->addItem("YUYV");
      comboBoxPackingOrder->addItem("YVYU");
    }
    else if (idx == YUV_420)
    {
      comboBoxPackingOrder->addItem("YYYYUV");
      comboBoxPackingOrder->addItem("YYUYYV");
      comboBoxPackingOrder->addItem("UYYVYY");
      comboBoxPackingOrder->addItem("VYYUYY");
    }
    else
      return;

    if (idx != 3)
      comboBoxPackingOrder->setCurrentIndex(0);
    
    // Disable the combo boxes if there are no chroma components
    bool chromaPresent = (idx != 3);
    groupBoxPacked->setEnabled(chromaPresent);
    groupBoxPlanar->setEnabled(chromaPresent);
  }

  yuvPixelFormat videoHandlerYUV_CustomFormatDialog::getYUVFormat() const
  {
    // Get all the values from the controls
    yuvPixelFormat format;
    
    int idx = comboBoxChromaSubsampling->currentIndex();
    Q_ASSERT(idx >= 0 && idx < YUV_NUM_SUBSAMPLINGS);
    format.subsampling = static_cast<YUVSubsamplingType>(idx);

    idx = comboBoxBitDepth->currentIndex();
    static const int bitDepths[] = {8, 9, 10, 12, 14, 16};
    Q_ASSERT(idx >= 0 && idx < 6);
    format.bitsPerSample = bitDepths[idx];

    format.bigEndian = (comboBoxEndianess->currentIndex() == 0);
    format.planar = (groupBoxPlanar->isChecked());

    if (format.planar)
    {
      idx = comboBoxPlaneOrder->currentIndex();
      Q_ASSERT(idx >= 0 && idx < Order_NUM);
      format.planeOrder = static_cast<YUVPlaneOrder>(idx);
    }
    else
    {
      idx = comboBoxPackingOrder->currentIndex();
      int offset = 0;
      if (format.subsampling == YUV_422 || format.subsampling == YUV_440)
        offset = Packing_UYVY;
      else if (format.subsampling == YUV_420)
        offset = Packing_YYYYUV;
      Q_ASSERT(idx+offset > 0 && idx+offset < Packing_NUM);
      format.packingOrder = static_cast<YUVPackingOrder>(idx + offset);
      format.bytePacking = (checkBoxBytePacking->isChecked());
    }

    return format;
  }

  void videoHandlerYUV_CustomFormatDialog::on_comboBoxBitDepth_currentIndexChanged(int idx)
  {
    // Endianess only makes sense when the bit depth is > 8bit.
    bool bitDepth8 = (idx == 0);
    comboBoxEndianess->setEnabled(!bitDepth8);
  }
}

// ---------------------- videoHandlerYUV -----------------------------------

// Initialize the static yuvFormatList
YUV_Internals::YUVFormatList videoHandlerYUV::yuvPresetsList;

videoHandlerYUV::videoHandlerYUV() : videoHandler(),
  ui(new Ui::videoHandlerYUV)
{
  // preset internal values
  interpolationMode = NearestNeighborInterpolation;
  componentDisplayMode = DisplayAll;
  yuvColorConversionType = YUVC709ColorConversionType;

  // Set the default YUV transformation parameters.
  // TODO: Why is the offset 125 for Luma??
  mathParameters[Luma] = yuvMathParameters(1, 125, false);
  mathParameters[Chroma] = yuvMathParameters(1, 128, false);

  // If we know nothing about the YUV format, assume YUV 4:2:0 8 bit planar by default.
  srcPixelFormat = yuvPixelFormat(YUV_420, 8, Order_YUV);

  controlsCreated = false;
  currentFrameRawYUVData_frameIdx = -1;
  rawYUVData_frameIdx = -1;
}

void videoHandlerYUV::loadValues(QSize newFramesize, QString sourcePixelFormat)
{
  setFrameSize(newFramesize);
}

videoHandlerYUV::~videoHandlerYUV()
{
  delete ui;
}

/// --- Convert from the current YUV input format to YUV 444

//inline quint32 SwapInt32(quint32 arg) {
//  quint32 result;
//  result = ((arg & 0xFF) << 24) | ((arg & 0xFF00) << 8) | ((arg >> 8) & 0xFF00) | ((arg >> 24) & 0xFF);
//  return result;
//}
//
//inline quint16 SwapInt16(quint16 arg) {
//  quint16 result;
//  result = (quint16)(((arg << 8) & 0xFF00) | ((arg >> 8) & 0xFF));
//  return result;
//}
//
//inline quint32 SwapInt32BigToHost(quint32 arg) {
//#if __BIG_ENDIAN__ || IS_BIG_ENDIAN
//  return arg;
//#else
//  return SwapInt32(arg);
//#endif
//}
//
//inline quint32 SwapInt32LittleToHost(quint32 arg) {
//#if __LITTLE_ENDIAN__ || IS_LITTLE_ENDIAN
//  return arg;
//#else
//  return SwapInt32(arg);
//#endif
//}
//
//inline quint16 SwapInt16LittleToHost(quint16 arg) {
//#if __LITTLE_ENDIAN__
//  return arg;
//#else
//  return SwapInt16(arg);
//#endif
//}

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

/* The supported formats are:
 * 4:2:2 8-bit packed --- UYVY|UYVY|...
 * 4:2:2 10-bit packed (UYVY) --- 
*/
//#if SSE_CONVERSION
//void videoHandlerYUV::convert2YUV444(byteArrayAligned &sourceBuffer, byteArrayAligned &targetBuffer)
//#else
//void videoHandlerYUV::convert2YUV444(QByteArray &sourceBuffer, QByteArray &targetBuffer)
//#endif
//{
//  if (srcPixelFormat == "Unknown Pixel Format") {
//    // Unknown format. We cannot convert this.
//    return;
//  }
//
//  const int componentWidth = frameSize.width();
//  const int componentHeight = frameSize.height();
//  // TODO: make this compatible with 10bit sequences
//  const int componentLength = componentWidth * componentHeight; // number of bytes per luma frames
//  const int horiSubsampling = srcPixelFormat.subsamplingHorizontal;
//  const int vertSubsampling = srcPixelFormat.subsamplingVertical;
//  const int chromaWidth = horiSubsampling == 0 ? 0 : componentWidth / horiSubsampling;
//  const int chromaHeight = vertSubsampling == 0 ? 0 : componentHeight / vertSubsampling;
//  const int chromaLength = chromaWidth * chromaHeight; // number of bytes per chroma frame
//  // make sure target buffer is big enough (YUV444 means 3 byte per sample)
//  int targetBufferLength = 3 * componentWidth * componentHeight * srcPixelFormat.bytePerComponentSample;
//  if (targetBuffer.size() != targetBufferLength)
//    targetBuffer.resize(targetBufferLength);
//
//  // TODO: keep unsigned char for 10bit? use short?
//  if (chromaLength == 0) {
//    const unsigned char *srcY = (unsigned char*)sourceBuffer.data();
//    unsigned char *dstY = (unsigned char*)targetBuffer.data();
//    unsigned char *dstU = dstY + componentLength;
//    memcpy(dstY, srcY, componentLength);
//    memset(dstU, 128, 2 * componentLength);
//  }
//  else if (srcPixelFormat == "4:2:2 8-bit packed") {
//    const unsigned char *srcY = (unsigned char*)sourceBuffer.data();
//    unsigned char *dstY = (unsigned char*)targetBuffer.data();
//    unsigned char *dstU = dstY + componentLength;
//    unsigned char *dstV = dstU + componentLength;
//
//    int y;
//#pragma omp parallel for default(none) shared(dstY,dstU,dstV,srcY)
//    for (y = 0; y < componentHeight; y++) {
//      for (int x = 0; x < componentWidth; x++) {
//        dstY[x + y*componentWidth] = srcY[((x + y*componentWidth) << 1) + 1];
//        dstU[x + y*componentWidth] = srcY[((((x >> 1) << 1) + y*componentWidth) << 1)];
//        dstV[x + y*componentWidth] = srcY[((((x >> 1) << 1) + y*componentWidth) << 1) + 2];
//      }
//    }
//  }
//  else if (srcPixelFormat == "4:2:2 10-bit packed (UYVY)") 
//  {
//    const quint32 *srcY = (quint32*)sourceBuffer.data();
//    quint16 *dstY = (quint16*)targetBuffer.data();
//    quint16 *dstU = dstY + componentLength;
//    quint16 *dstV = dstU + componentLength;
//
//    int i;
//#define BIT_INCREASE 6
//#pragma omp parallel for default(none) shared(dstY,dstU,dstV,srcY)
//    for (i = 0; i < ((componentLength + 5) / 6); i++) {
//      const int srcPos = i * 4;
//      const int dstPos = i * 6;
//      quint32 srcVal;
//      srcVal = SwapInt32BigToHost(srcY[srcPos]);
//      dstV[dstPos] = dstV[dstPos + 1] = (srcVal & 0xffc00000) >> (22 - BIT_INCREASE);
//      dstY[dstPos] = (srcVal & 0x003ff000) >> (12 - BIT_INCREASE);
//      dstU[dstPos] = dstU[dstPos + 1] = (srcVal & 0x00000ffc) << (BIT_INCREASE - 2);
//      srcVal = SwapInt32BigToHost(srcY[srcPos + 1]);
//      dstY[dstPos + 1] = (srcVal & 0xffc00000) >> (22 - BIT_INCREASE);
//      dstV[dstPos + 2] = dstV[dstPos + 3] = (srcVal & 0x003ff000) >> (12 - BIT_INCREASE);
//      dstY[dstPos + 2] = (srcVal & 0x00000ffc) << (BIT_INCREASE - 2);
//      srcVal = SwapInt32BigToHost(srcY[srcPos + 2]);
//      dstU[dstPos + 2] = dstU[dstPos + 3] = (srcVal & 0xffc00000) >> (22 - BIT_INCREASE);
//      dstY[dstPos + 3] = (srcVal & 0x003ff000) >> (12 - BIT_INCREASE);
//      dstV[dstPos + 4] = dstV[dstPos + 5] = (srcVal & 0x00000ffc) << (BIT_INCREASE - 2);
//      srcVal = SwapInt32BigToHost(srcY[srcPos + 3]);
//      dstY[dstPos + 4] = (srcVal & 0xffc00000) >> (22 - BIT_INCREASE);
//      dstU[dstPos + 4] = dstU[dstPos + 5] = (srcVal & 0x003ff000) >> (12 - BIT_INCREASE);
//      dstY[dstPos + 5] = (srcVal & 0x00000ffc) << (BIT_INCREASE - 2);
//    }
//  }
//  else if (srcPixelFormat == "4:2:2 10-bit packed 'v210'") {
//    const quint32 *srcY = (quint32*)sourceBuffer.data();
//    quint16 *dstY = (quint16*)targetBuffer.data();
//    quint16 *dstU = dstY + componentLength;
//    quint16 *dstV = dstU + componentLength;
//
//    int i;
//#define BIT_INCREASE 6
//#pragma omp parallel for default(none) shared(dstY,dstU,dstV,srcY)
//    for (i = 0; i < ((componentLength + 5) / 6); i++) {
//      const int srcPos = i * 4;
//      const int dstPos = i * 6;
//      quint32 srcVal;
//      srcVal = SwapInt32LittleToHost(srcY[srcPos]);
//      dstV[dstPos] = dstV[dstPos + 1] = (srcVal & 0x3ff00000) >> (20 - BIT_INCREASE);
//      dstY[dstPos] = (srcVal & 0x000ffc00) >> (10 - BIT_INCREASE);
//      dstU[dstPos] = dstU[dstPos + 1] = (srcVal & 0x000003ff) << BIT_INCREASE;
//      srcVal = SwapInt32LittleToHost(srcY[srcPos + 1]);
//      dstY[dstPos + 1] = (srcVal & 0x000003ff) << BIT_INCREASE;
//      dstU[dstPos + 2] = dstU[dstPos + 3] = (srcVal & 0x000ffc00) >> (10 - BIT_INCREASE);
//      dstY[dstPos + 2] = (srcVal & 0x3ff00000) >> (20 - BIT_INCREASE);
//      srcVal = SwapInt32LittleToHost(srcY[srcPos + 2]);
//      dstU[dstPos + 4] = dstU[dstPos + 5] = (srcVal & 0x3ff00000) >> (20 - BIT_INCREASE);
//      dstY[dstPos + 3] = (srcVal & 0x000ffc00) >> (10 - BIT_INCREASE);
//      dstV[dstPos + 2] = dstV[dstPos + 3] = (srcVal & 0x000003ff) << BIT_INCREASE;
//      srcVal = SwapInt32LittleToHost(srcY[srcPos + 3]);
//      dstY[dstPos + 4] = (srcVal & 0x000003ff) << BIT_INCREASE;
//      dstV[dstPos + 4] = dstV[dstPos + 5] = (srcVal & 0x000ffc00) >> (10 - BIT_INCREASE);
//      dstY[dstPos + 5] = (srcVal & 0x3ff00000) >> (20 - BIT_INCREASE);
//    }
//  }
//  else if (srcPixelFormat == "4:2:0 Y'CbCr 8-bit planar" && interpolationMode == BiLinearInterpolation) {
//    // vertically midway positioning - unsigned rounding
//    const unsigned char *srcY = (unsigned char*)sourceBuffer.data();
//    const unsigned char *srcU = srcY + componentLength;
//    const unsigned char *srcV = srcU + chromaLength;
//    const unsigned char *srcUV[2] = { srcU, srcV };
//    unsigned char *dstY = (unsigned char*)targetBuffer.data();
//    unsigned char *dstU = dstY + componentLength;
//    unsigned char *dstV = dstU + componentLength;
//    unsigned char *dstUV[2] = { dstU, dstV };
//
//    const int dstLastLine = (componentHeight - 1)*componentWidth;
//    const int srcLastLine = (chromaHeight - 1)*chromaWidth;
//
//    memcpy(dstY, srcY, componentLength);
//
//    int c;
//    for (c = 0; c < 2; c++) {
//      //NSLog(@"%i", omp_get_num_threads());
//      // first line
//      dstUV[c][0] = srcUV[c][0];
//      int i;
//#pragma omp parallel for default(none) shared(dstUV,srcUV) firstprivate(c)
//      for (i = 0; i < chromaWidth - 1; i++) {
//        dstUV[c][i * 2 + 1] = (((int)(srcUV[c][i]) + (int)(srcUV[c][i + 1]) + 1) >> 1);
//        dstUV[c][i * 2 + 2] = srcUV[c][i + 1];
//      }
//      dstUV[c][componentWidth - 1] = dstUV[c][componentWidth - 2];
//
//      int j;
//#pragma omp parallel for default(none) shared(dstUV,srcUV) firstprivate(c)
//      for (j = 0; j < chromaHeight - 1; j++) {
//        const int dstTop = (j * 2 + 1)*componentWidth;
//        const int dstBot = (j * 2 + 2)*componentWidth;
//        const int srcTop = j*chromaWidth;
//        const int srcBot = (j + 1)*chromaWidth;
//        dstUV[c][dstTop] = ((3 * (int)(srcUV[c][srcTop]) + (int)(srcUV[c][srcBot]) + 2) >> 2);
//        dstUV[c][dstBot] = (((int)(srcUV[c][srcTop]) + 3 * (int)(srcUV[c][srcBot]) + 2) >> 2);
//        for (int i = 0; i < chromaWidth - 1; i++) {
//          const int tl = srcUV[c][srcTop + i];
//          const int tr = srcUV[c][srcTop + i + 1];
//          const int bl = srcUV[c][srcBot + i];
//          const int br = srcUV[c][srcBot + i + 1];
//          dstUV[c][dstTop + i * 2 + 1] = ((6 * tl + 6 * tr + 2 * bl + 2 * br + 8) >> 4);
//          dstUV[c][dstBot + i * 2 + 1] = ((2 * tl + 2 * tr + 6 * bl + 6 * br + 8) >> 4);
//          dstUV[c][dstTop + i * 2 + 2] = ((3 * tr + br + 2) >> 2);
//          dstUV[c][dstBot + i * 2 + 2] = ((tr + 3 * br + 2) >> 2);
//        }
//        dstUV[c][dstTop + componentWidth - 1] = dstUV[c][dstTop + componentWidth - 2];
//        dstUV[c][dstBot + componentWidth - 1] = dstUV[c][dstBot + componentWidth - 2];
//      }
//
//      dstUV[c][dstLastLine] = srcUV[c][srcLastLine];
//#pragma omp parallel for default(none) shared(dstUV,srcUV) firstprivate(c)
//      for (i = 0; i < chromaWidth - 1; i++) {
//        dstUV[c][dstLastLine + i * 2 + 1] = (((int)(srcUV[c][srcLastLine + i]) + (int)(srcUV[c][srcLastLine + i + 1]) + 1) >> 1);
//        dstUV[c][dstLastLine + i * 2 + 2] = srcUV[c][srcLastLine + i + 1];
//      }
//      dstUV[c][dstLastLine + componentWidth - 1] = dstUV[c][dstLastLine + componentWidth - 2];
//    }
//  }
//  else if (srcPixelFormat == "4:2:0 Y'CbCr 8-bit planar" && interpolationMode == InterstitialInterpolation) {
//    // interstitial positioning - unsigned rounding, takes 2 times as long as nearest neighbour
//    const unsigned char *srcY = (unsigned char*)sourceBuffer.data();
//    const unsigned char *srcU = srcY + componentLength;
//    const unsigned char *srcV = srcU + chromaLength;
//    const unsigned char *srcUV[2] = { srcU, srcV };
//    unsigned char *dstY = (unsigned char*)targetBuffer.data();
//    unsigned char *dstU = dstY + componentLength;
//    unsigned char *dstV = dstU + componentLength;
//    unsigned char *dstUV[2] = { dstU, dstV };
//
//    const int dstLastLine = (componentHeight - 1)*componentWidth;
//    const int srcLastLine = (chromaHeight - 1)*chromaWidth;
//
//    memcpy(dstY, srcY, componentLength);
//
//    int c;
//    for (c = 0; c < 2; c++) {
//      // first line
//      dstUV[c][0] = srcUV[c][0];
//
//      int i;
//#pragma omp parallel for default(none) shared(dstUV,srcUV) firstprivate(c)
//      for (i = 0; i < chromaWidth - 1; i++) {
//        dstUV[c][2 * i + 1] = ((3 * (int)(srcUV[c][i]) + (int)(srcUV[c][i + 1]) + 2) >> 2);
//        dstUV[c][2 * i + 2] = (((int)(srcUV[c][i]) + 3 * (int)(srcUV[c][i + 1]) + 2) >> 2);
//      }
//      dstUV[c][componentWidth - 1] = srcUV[c][chromaWidth - 1];
//
//      int j;
//#pragma omp parallel for default(none) shared(dstUV,srcUV) firstprivate(c)
//      for (j = 0; j < chromaHeight - 1; j++) {
//        const int dstTop = (j * 2 + 1)*componentWidth;
//        const int dstBot = (j * 2 + 2)*componentWidth;
//        const int srcTop = j*chromaWidth;
//        const int srcBot = (j + 1)*chromaWidth;
//        dstUV[c][dstTop] = ((3 * (int)(srcUV[c][srcTop]) + (int)(srcUV[c][srcBot]) + 2) >> 2);
//        dstUV[c][dstBot] = (((int)(srcUV[c][srcTop]) + 3 * (int)(srcUV[c][srcBot]) + 2) >> 2);
//        for (int i = 0; i < chromaWidth - 1; i++) {
//          const int tl = srcUV[c][srcTop + i];
//          const int tr = srcUV[c][srcTop + i + 1];
//          const int bl = srcUV[c][srcBot + i];
//          const int br = srcUV[c][srcBot + i + 1];
//          dstUV[c][dstTop + i * 2 + 1] = (9 * tl + 3 * tr + 3 * bl + br + 8) >> 4;
//          dstUV[c][dstBot + i * 2 + 1] = (3 * tl + tr + 9 * bl + 3 * br + 8) >> 4;
//          dstUV[c][dstTop + i * 2 + 2] = (3 * tl + 9 * tr + bl + 3 * br + 8) >> 4;
//          dstUV[c][dstBot + i * 2 + 2] = (tl + 3 * tr + 3 * bl + 9 * br + 8) >> 4;
//        }
//        dstUV[c][dstTop + componentWidth - 1] = ((3 * (int)(srcUV[c][srcTop + chromaWidth - 1]) + (int)(srcUV[c][srcBot + chromaWidth - 1]) + 2) >> 2);
//        dstUV[c][dstBot + componentWidth - 1] = (((int)(srcUV[c][srcTop + chromaWidth - 1]) + 3 * (int)(srcUV[c][srcBot + chromaWidth - 1]) + 2) >> 2);
//      }
//
//      dstUV[c][dstLastLine] = srcUV[c][srcLastLine];
//#pragma omp parallel for default(none) shared(dstUV,srcUV) firstprivate(c)
//      for (i = 0; i < chromaWidth - 1; i++) {
//        dstUV[c][dstLastLine + i * 2 + 1] = ((3 * (int)(srcUV[c][srcLastLine + i]) + (int)(srcUV[c][srcLastLine + i + 1]) + 2) >> 2);
//        dstUV[c][dstLastLine + i * 2 + 2] = (((int)(srcUV[c][srcLastLine + i]) + 3 * (int)(srcUV[c][srcLastLine + i + 1]) + 2) >> 2);
//      }
//      dstUV[c][dstLastLine + componentWidth - 1] = srcUV[c][srcLastLine + chromaWidth - 1];
//    }
//  } /*else if (pixelFormatType == YUVC_420YpCbCr8PlanarPixelFormat && self.chromaInterpolation == 3) {
//    // interstitial positioning - correct signed rounding - takes 6/5 times as long as unsigned rounding
//    const unsigned char *srcY = (unsigned char*)sourceBuffer->data();
//    const unsigned char *srcU = srcY + componentLength;
//    const unsigned char *srcV = srcU + chromaLength;
//    unsigned char *dstY = (unsigned char*)targetBuffer->data();
//    unsigned char *dstU = dstY + componentLength;
//    unsigned char *dstV = dstU + componentLength;
//
//    memcpy(dstY, srcY, componentLength);
//
//    unsigned char *dstC = dstU;
//    const unsigned char *srcC = srcU;
//    int c;
//    for (c = 0; c < 2; c++) {
//    // first line
//    unsigned char *endLine = dstC + componentWidth;
//    unsigned char *endComp = dstC + componentLength;
//    *dstC++ = *srcC;
//    while (dstC < (endLine-1)) {
//    *dstC++ = thresAddAndShift( 3*(int)(*srcC) +   (int)(*(srcC+1)), 512, 1, 2, 2);
//    *dstC++ = thresAddAndShift(   (int)(*srcC) + 3*(int)(*(srcC+1)), 512, 1, 2, 2);
//    srcC++;
//    }
//    *dstC++ = *srcC++;
//    srcC -= chromaWidth;
//
//    while (dstC < endComp - 2*componentWidth) {
//    endLine = dstC + componentWidth;
//    *(dstC)                = thresAddAndShift( 3*(int)(*srcC) +   (int)(*(srcC+chromaWidth)), 512, 1, 2, 2);
//    *(dstC+componentWidth) = thresAddAndShift(   (int)(*srcC) + 3*(int)(*(srcC+chromaWidth)), 512, 1, 2, 2);
//    dstC++;
//    while (dstC < endLine-1) {
//    int tl = (int)*srcC;
//    int tr = (int)*(srcC+1);
//    int bl = (int)*(srcC+chromaWidth);
//    int br = (int)*(srcC+chromaWidth+1);
//    *(dstC)                  = thresAddAndShift(9*tl + 3*tr + 3*bl +   br, 2048, 7, 8, 4);
//    *(dstC+1)                = thresAddAndShift(3*tl + 9*tr +   bl + 3*br, 2048, 7, 8, 4);
//    *(dstC+componentWidth)   = thresAddAndShift(3*tl +   tr + 9*bl + 3*br, 2048, 7, 8, 4);
//    *(dstC+componentWidth+1) = thresAddAndShift(  tl + 3*tr + 3*bl + 9*br, 2048, 7, 8, 4);
//    srcC++;
//    dstC+=2;
//    }
//    *(dstC)                = thresAddAndShift( 3*(int)(*srcC) +   (int)(*(srcC+chromaWidth)), 512, 1, 2, 2);
//    *(dstC+componentWidth) = thresAddAndShift(   (int)(*srcC) + 3*(int)(*(srcC+chromaWidth)), 512, 1, 2, 2);
//    dstC++;
//    srcC++;
//    dstC += componentWidth;
//    }
//
//    endLine = dstC + componentWidth;
//    *dstC++ = *srcC;
//    while (dstC < (endLine-1)) {
//    *dstC++ = thresAddAndShift( 3*(int)(*srcC) +   (int)(*(srcC+1)), 512, 1, 2, 2);
//    *dstC++ = thresAddAndShift(   (int)(*srcC) + 3*(int)(*(srcC+1)), 512, 1, 2, 2);
//    srcC++;
//    }
//    *dstC++ = *srcC++;
//
//    dstC = dstV;
//    srcC = srcV;
//    }
//    }*/ else if (srcPixelFormat.planar && srcPixelFormat.bitsPerSample == 8) {
//    // sample and hold interpolation
//    const bool reverseUV = (srcPixelFormat == "4:4:4 Y'CrCb 8-bit planar") || (srcPixelFormat == "4:2:2 Y'CrCb 8-bit planar");
//    const unsigned char *srcY = (unsigned char*)sourceBuffer.data();
//    const unsigned char *srcU = srcY + componentLength + (reverseUV ? chromaLength : 0);
//    const unsigned char *srcV = srcY + componentLength + (reverseUV ? 0 : chromaLength);
//    unsigned char *dstY = (unsigned char*)targetBuffer.data();
//    unsigned char *dstU = dstY + componentLength;
//    unsigned char *dstV = dstU + componentLength;
//    int horiShiftTmp = 0;
//    int vertShiftTmp = 0;
//    while (((1 << horiShiftTmp) & horiSubsampling) != 0) horiShiftTmp++;
//    while (((1 << vertShiftTmp) & vertSubsampling) != 0) vertShiftTmp++;
//    const int horiShift = horiShiftTmp;
//    const int vertShift = vertShiftTmp;
//
//    memcpy(dstY, srcY, componentLength);
//
//    if (2 == horiSubsampling && 2 == vertSubsampling) {
//      int y;
//#pragma omp parallel for default(none) shared(dstV,dstU,srcV,srcU)
//      for (y = 0; y < chromaHeight; y++) {
//        for (int x = 0; x < chromaWidth; x++) {
//          dstU[2 * x + 2 * y*componentWidth] = dstU[2 * x + 1 + 2 * y*componentWidth] = srcU[x + y*chromaWidth];
//          dstV[2 * x + 2 * y*componentWidth] = dstV[2 * x + 1 + 2 * y*componentWidth] = srcV[x + y*chromaWidth];
//        }
//        memcpy(&dstU[(2 * y + 1)*componentWidth], &dstU[(2 * y)*componentWidth], componentWidth);
//        memcpy(&dstV[(2 * y + 1)*componentWidth], &dstV[(2 * y)*componentWidth], componentWidth);
//      }
//    }
//    else if ((1 << horiShift) == horiSubsampling && (1 << vertShift) == vertSubsampling) {
//      int y;
//#pragma omp parallel for default(none) shared(dstV,dstU,srcV,srcU)
//      for (y = 0; y < componentHeight; y++) {
//        for (int x = 0; x < componentWidth; x++) {
//          //dstY[x + y*componentWidth] = srcY[x + y*componentWidth];
//          dstU[x + y*componentWidth] = srcU[(x >> horiShift) + (y >> vertShift)*chromaWidth];
//          dstV[x + y*componentWidth] = srcV[(x >> horiShift) + (y >> vertShift)*chromaWidth];
//        }
//      }
//    }
//    else {
//      int y;
//#pragma omp parallel for default(none) shared(dstV,dstU,srcV,srcU)
//      for (y = 0; y < componentHeight; y++) {
//        for (int x = 0; x < componentWidth; x++) {
//          //dstY[x + y*componentWidth] = srcY[x + y*componentWidth];
//          dstU[x + y*componentWidth] = srcU[x / horiSubsampling + y / vertSubsampling*chromaWidth];
//          dstV[x + y*componentWidth] = srcV[x / horiSubsampling + y / vertSubsampling*chromaWidth];
//        }
//      }
//    }
//  }
//    else if (srcPixelFormat == "4:2:0 Y'CbCr 10-bit LE planar") {
//      // TODO: chroma interpolation for 4:2:0 10bit planar
//      const unsigned short *srcY = (unsigned short*)sourceBuffer.data();
//      const unsigned short *srcU = srcY + componentLength;
//      const unsigned short *srcV = srcU + chromaLength;
//      unsigned short *dstY = (unsigned short*)targetBuffer.data();
//      unsigned short *dstU = dstY + componentLength;
//      unsigned short *dstV = dstU + componentLength;
//
//      int y;
//#pragma omp parallel for default(none) shared(dstY,dstV,dstU,srcY,srcV,srcU)
//      for (y = 0; y < componentHeight; y++) {
//        for (int x = 0; x < componentWidth; x++) {
//          //dstY[x + y*componentWidth] = MIN(1023, CFSwapInt16LittleToHost(srcY[x + y*componentWidth])) << 6; // clip value for data which exceeds the 2^10-1 range
//          //     dstY[x + y*componentWidth] = SwapInt16LittleToHost(srcY[x + y*componentWidth])<<6;
//          //    dstU[x + y*componentWidth] = SwapInt16LittleToHost(srcU[x/2 + (y/2)*chromaWidth])<<6;
//          //    dstV[x + y*componentWidth] = SwapInt16LittleToHost(srcV[x/2 + (y/2)*chromaWidth])<<6;
//
//          dstY[x + y*componentWidth] = qFromLittleEndian(srcY[x + y*componentWidth]);
//          dstU[x + y*componentWidth] = qFromLittleEndian(srcU[x / 2 + (y / 2)*chromaWidth]);
//          dstV[x + y*componentWidth] = qFromLittleEndian(srcV[x / 2 + (y / 2)*chromaWidth]);
//        }
//      }
//    }
//    else if (srcPixelFormat == "4:4:4 Y'CbCr 12-bit BE planar"
//      || srcPixelFormat == "4:4:4 Y'CbCr 16-bit BE planar")
//    {
//      // Swap the input data in 2 byte pairs.
//      // BADC -> ABCD
//      const char *src = (char*)sourceBuffer.data();
//      char *dst = (char*)targetBuffer.data();
//      int i;
//#pragma omp parallel for default(none) shared(src,dst)
//      for (i = 0; i < srcPixelFormat.bytesPerFrame( QSize(componentWidth, componentHeight) ); i+=2)
//      {
//        dst[i] = src[i + 1];
//        dst[i + 1] = src[i];
//      }
//    }
//    else if (srcPixelFormat == "4:4:4 Y'CbCr 10-bit LE planar")
//    {
//      const unsigned short *srcY = (unsigned short*)sourceBuffer.data();
//      const unsigned short *srcU = srcY + componentLength;
//      const unsigned short *srcV = srcU + componentLength;
//      unsigned short *dstY = (unsigned short*)targetBuffer.data();
//      unsigned short *dstU = dstY + componentLength;
//      unsigned short *dstV = dstU + componentLength;
//      int y;
//#pragma omp parallel for default(none) shared(dstY,dstV,dstU,srcY,srcV,srcU)
//      for (y = 0; y < componentHeight; y++)
//      {
//        for (int x = 0; x < componentWidth; x++)
//        {
//          dstY[x + y*componentWidth] = qFromLittleEndian(srcY[x + y*componentWidth]);
//          dstU[x + y*componentWidth] = qFromLittleEndian(srcU[x + y*chromaWidth]);
//          dstV[x + y*componentWidth] = qFromLittleEndian(srcV[x + y*chromaWidth]);
//        }
//      }
//
//    }
//    else if (srcPixelFormat == "4:4:4 Y'CbCr 10-bit BE planar")
//    {
//      const unsigned short *srcY = (unsigned short*)sourceBuffer.data();
//      const unsigned short *srcU = srcY + componentLength;
//      const unsigned short *srcV = srcU + chromaLength;
//      unsigned short *dstY = (unsigned short*)targetBuffer.data();
//      unsigned short *dstU = dstY + componentLength;
//      unsigned short *dstV = dstU + componentLength;
//      int y;
//#pragma omp parallel for default(none) shared(dstY,dstV,dstU,srcY,srcV,srcU)
//      for (y = 0; y < componentHeight; y++)
//      {
//        for (int x = 0; x < componentWidth; x++)
//        {
//          dstY[x + y*componentWidth] = qFromBigEndian(srcY[x + y*componentWidth]);
//          dstU[x + y*componentWidth] = qFromBigEndian(srcU[x + y*chromaWidth]);
//          dstV[x + y*componentWidth] = qFromBigEndian(srcV[x + y*chromaWidth]);
//        }
//      }
//
//    }
//
//    else {
//      qDebug() << "Unhandled pixel format: " << srcPixelFormat.name << endl;
//    }
//
//    return;
//}

//#if SSE_CONVERSION
//void videoHandlerYUV::applyYUVTransformation(byteArrayAligned &sourceBuffer)
//#else
//void videoHandlerYUV::applyYUVTransformation(QByteArray &sourceBuffer)
//#endif
//{
//  // TODO: Check this for 10-bit Unput
//  // TODO: If luma only is selected, the U and V components are set to chromaZero, but a little color
//  //       can still be seen. Should we do this in the conversion functions? They have to be
//  //       resorted anyways.
//
//  if (!yuvMathRequired())
//    return;
//
//  const int lumaLength = frameSize.width() * frameSize.height();
//  const int singleChromaLength = lumaLength;
//  //const int chromaLength = 2*singleChromaLength;
//  const int sourceBPS = srcPixelFormat.bitsPerSample;
//  const int maxVal = (1<<sourceBPS)-1;
//  const int chromaZero = (1<<(sourceBPS-1));
//
//  // For now there is only one scale parameter for U and V. However, the transformation
//  // function allows them to be treated seperately.
//  int chromaUScale = chromaScale;
//  int chromaVScale = chromaScale;
//
//  typedef enum {
//    YUVMathDefaultColors,
//    YUVMathLumaOnly,
//    YUVMathCbOnly,
//    YUVMathCrOnly
//  } YUVTransformationMode;
//
//  YUVTransformationMode colorMode = YUVMathDefaultColors;
//
//  if( lumaScale != 1 && chromaUScale == 1 && chromaUScale == 1 )
//    colorMode = YUVMathLumaOnly;
//  else if( lumaScale == 1 && chromaUScale != 1 && chromaVScale == 1 )
//    colorMode = YUVMathCbOnly;
//  else if( lumaScale == 1 && chromaUScale == 1 && chromaVScale != 1 )
//    colorMode = YUVMathCrOnly;
//
//  if (sourceBPS == 8)
//  {
//    // Process 8 bit input
//    const unsigned char *src = (const unsigned char*)sourceBuffer.data();
//    unsigned char *dst = (unsigned char*)sourceBuffer.data();
//
//    // Process Luma
//    if (componentDisplayMode == DisplayCb || componentDisplayMode == DisplayCr)
//    {
//      // Set the Y component to 0 since we are not displayling it
//      memset(dst, 0, lumaLength * sizeof(unsigned char));
//    }
//    else if (colorMode == YUVMathDefaultColors || colorMode == YUVMathLumaOnly)
//    {
//      // The Y component is displayed and a Y transformation has to be applied
//      int i;
//#pragma omp parallel for default(none) shared(src,dst)
//      for (i = 0; i < lumaLength; i++)
//      {
//        int newVal = lumaInvert ? (maxVal-(int)(src[i])):((int)(src[i]));
//        newVal = (newVal - lumaOffset) * lumaScale + lumaOffset;
//        newVal = MAX( 0, MIN( maxVal, newVal ) );
//        dst[i] = (unsigned char)newVal;
//      }
//      dst += lumaLength;
//    }
//    src += lumaLength;
//
//    // Process chroma components
//    for (int c = 0; c < 2; c++)
//    {
//      if (   componentDisplayMode == DisplayY ||
//           ( componentDisplayMode == DisplayCb && c == 1 ) ||
//           ( componentDisplayMode == DisplayCr && c == 0 ) )
//      {
//        // This chroma component is not displayed. Set it to zero.
//        memset(dst, chromaZero, singleChromaLength * sizeof(unsigned char) );
//      }
//      else if (    colorMode == YUVMathDefaultColors
//               || (colorMode == YUVMathCbOnly && c == 0)
//               || (colorMode == YUVMathCrOnly && c == 1)
//               )
//      {
//        // This chroma component needs to be transformed
//        int i;
//        int cMultiplier = (c==0) ? chromaUScale : chromaVScale;
//#pragma omp parallel for default(none) shared(src,dst,cMultiplier)
//        for (i = 0; i < singleChromaLength; i++)
//        {
//          int newVal = chromaInvert ? (maxVal-(int)(src[i])):((int)(src[i]));
//          newVal = (newVal - chromaOffset) * cMultiplier + chromaOffset;
//          newVal = MAX( 0, MIN( maxVal, newVal ) );
//          dst[i] = (unsigned char)newVal;
//        }
//      }
//      src += singleChromaLength;
//      dst += singleChromaLength;
//    }
//
//  }
//  else if (sourceBPS>8 && sourceBPS<=16)
//  {
//    // Process 9 to 16 bit input
//    const unsigned short *src = (const unsigned short*)sourceBuffer.data();
//    unsigned short *dst = (unsigned short*)sourceBuffer.data();
//
//    // Process Luma
//    if (componentDisplayMode == DisplayCb || componentDisplayMode == DisplayCr)
//    {
//      // Set the Y component to 0 since we are not displayling it
//      memset(dst, 0, lumaLength * sizeof(unsigned short));
//    }
//    else if (colorMode == YUVMathDefaultColors || colorMode == YUVMathLumaOnly)
//    {
//      // The Y component is displayed and a Y transformation has to be applied
//      int i;
//#pragma omp parallel for default(none) shared(src,dst)
//      for (i = 0; i < lumaLength; i++) {
//        int newVal = lumaInvert ? (maxVal-(int)(src[i])):((int)(src[i]));
//        newVal = (newVal - lumaOffset) * lumaScale + lumaOffset;
//        newVal = MAX( 0, MIN( maxVal, newVal ) );
//        dst[i] = (unsigned short)newVal;
//      }
//      dst += lumaLength;
//    }
//    src += lumaLength;
//
//    // Process chroma components
//    for (int c = 0; c < 2; c++) {
//      if (   componentDisplayMode != DisplayY ||
//           ( componentDisplayMode == DisplayCb && c == 1 ) ||
//           ( componentDisplayMode == DisplayCr && c == 0 ) )
//      {
//        // This chroma component is not displayed. Set it to zero.
//        memset(dst, chromaZero, singleChromaLength * sizeof(unsigned char) );
//      }
//      else if (   colorMode == YUVMathDefaultColors
//         || (colorMode == YUVMathCbOnly && c == 0)
//         || (colorMode == YUVMathCrOnly && c == 1)
//         )
//      {
//        // This chroma component needs to be transformed
//        int i;
//        int cMultiplier = (c==0) ? chromaUScale : chromaVScale;
//#pragma omp parallel for default(none) shared(src,dst,cMultiplier)
//        for (i = 0; i < singleChromaLength; i++)
//        {
//          int newVal = chromaInvert ? (maxVal-(int)(src[i])):((int)(src[i]));
//          newVal = (newVal - chromaOffset) * cMultiplier + chromaOffset;
//          newVal = MAX( 0, MIN( maxVal, newVal ) );
//          dst[i] = (unsigned short)newVal;
//        }
//        dst += singleChromaLength;
//      }
//      src += singleChromaLength;
//    }
//  }
//  else
//    qDebug() << "unsupported bitdepth, returning original data: " << sourceBPS << endl;
//}
//

//
//#if SSE_CONVERSION
//void videoHandlerYUV::convertYUV4442RGB(byteArrayAligned &sourceBuffer, byteArrayAligned &targetBuffer)
//#else
//void videoHandlerYUV::convertYUV4442RGB(QByteArray &sourceBuffer, QByteArray &targetBuffer)
//#endif
//{
//  static unsigned char clp_buf[384+256+384];
//  static unsigned char *clip_buf = clp_buf+384;
//  static bool clp_buf_initialized = false;
//
//  if (!clp_buf_initialized)
//  {
//    // Initialize clipping table. Because of the static bool, this will only be called once.
//    memset(clp_buf, 0, 384);
//    int i;
//    for (i = 0; i < 256; i++)
//      clp_buf[384+i] = i;
//    memset(clp_buf+384+256, 255, 384);
//    clp_buf_initialized = true;
//  }
//
//  const int bps = srcPixelFormat.bitsPerSample;
//
//  // make sure target buffer is big enough
//  int srcBufferLength = sourceBuffer.size();
//  Q_ASSERT( srcBufferLength%3 == 0 ); // YUV444 has 3 bytes per pixel
//  int componentLength = 0;
//  //buffer size changes depending on the bit depth
//  if(bps == 8)
//  {
//    componentLength = srcBufferLength/3;
//    if( targetBuffer.size() != srcBufferLength)
//      targetBuffer.resize(srcBufferLength);
//  }
//  else if(bps==10)
//  {
//    componentLength = srcBufferLength/6;
//    if( targetBuffer.size() != srcBufferLength/2)
//      targetBuffer.resize(srcBufferLength/2);
//  }
//  else
//  {
//    if( targetBuffer.size() != srcBufferLength)
//      targetBuffer.resize(srcBufferLength);
//  }
//
//  const int yOffset = 16<<(bps-8);
//  const int cZero = 128<<(bps-8);
//  const int rgbMax = (1<<bps)-1;
//  int yMult, rvMult, guMult, gvMult, buMult;
//
//  unsigned char *dst = (unsigned char*)targetBuffer.data();
//
//  if (bps == 8)
//  {
//    switch (yuvColorConversionType)
//    {
//      case YUVC601ColorConversionType:
//        yMult =   76309;
//        rvMult = 104597;
//        guMult = -25675;
//        gvMult = -53279;
//        buMult = 132201;
//        break;
//      case YUVC2020ColorConversionType:
//        yMult =   76309;
//        rvMult = 110013;
//        guMult = -12276;
//        gvMult = -42626;
//        buMult = 140363;
//        break;
//      case YUVC709ColorConversionType:
//      default:
//        yMult =   76309;
//        rvMult = 117489;
//        guMult = -13975;
//        gvMult = -34925;
//        buMult = 138438;
//        break;
//    }
//    const unsigned char * restrict srcY = (unsigned char*)sourceBuffer.data();
//    const unsigned char * restrict srcU = srcY + componentLength;
//    const unsigned char * restrict srcV = srcU + componentLength;
//    unsigned char * restrict dstMem = dst;
//
//    int i;
//#pragma omp parallel for default(none) private(i) shared(srcY,srcU,srcV,dstMem,yMult,rvMult,guMult,gvMult,buMult,clip_buf,componentLength)// num_threads(2)
//    for (i = 0; i < componentLength; ++i)
//    {
//      const int Y_tmp = ((int)srcY[i] - yOffset) * yMult;
//      const int U_tmp = (int)srcU[i] - cZero;
//      const int V_tmp = (int)srcV[i] - cZero;
//
//      const int R_tmp = (Y_tmp                  + V_tmp * rvMult ) >> 16;//32 to 16 bit conversion by left shifting
//      const int G_tmp = (Y_tmp + U_tmp * guMult + V_tmp * gvMult ) >> 16;
//      const int B_tmp = (Y_tmp + U_tmp * buMult                  ) >> 16;
//
//      dstMem[3*i]   = clip_buf[R_tmp];
//      dstMem[3*i+1] = clip_buf[G_tmp];
//      dstMem[3*i+2] = clip_buf[B_tmp];
//    }
//  }
//  else if (bps > 8 && bps <= 16)
//  {
//    switch (yuvColorConversionType)
//    {
//      case YUVC601ColorConversionType:
//        yMult =   19535114;
//        rvMult =  26776886;
//        guMult =  -6572681;
//        gvMult = -13639334;
//        buMult =  33843539;
//        break;
//      case YUVC709ColorConversionType:
//      default:
//        yMult =   19535114;
//        rvMult =  30077204;
//        guMult =  -3577718;
//        gvMult =  -8940735;
//        buMult =  35440221;
//    }
//
//    if (bps < 16)
//    {
//      yMult  = (yMult  + (1<<(15-bps))) >> (16-bps);//32 bit values
//      rvMult = (rvMult + (1<<(15-bps))) >> (16-bps);
//      guMult = (guMult + (1<<(15-bps))) >> (16-bps);
//      gvMult = (gvMult + (1<<(15-bps))) >> (16-bps);
//      buMult = (buMult + (1<<(15-bps))) >> (16-bps);
//    }
//    const unsigned short *srcY = (unsigned short*)sourceBuffer.data();
//    const unsigned short *srcU = srcY + componentLength;
//    const unsigned short *srcV = srcU + componentLength;
//    unsigned char *dstMem = dst;
//
//    int i;
//#pragma omp parallel for default(none) private(i) shared(srcY,srcU,srcV,dstMem,yMult,rvMult,guMult,gvMult,buMult,componentLength) // num_threads(2)
//    for (i = 0; i < componentLength; ++i)
//    {
//      qint64 Y_tmp = ((qint64)srcY[i] - yOffset)*yMult;
//      qint64 U_tmp = (qint64)srcU[i]- cZero ;
//      qint64 V_tmp = (qint64)srcV[i]- cZero ;
//      // unsigned int temp = 0, temp1=0;
//
//      qint64 R_tmp  = (Y_tmp                  + V_tmp * rvMult) >> (8+bps);
//      dstMem[i*3]   = (R_tmp<0 ? 0 : (R_tmp>rgbMax ? rgbMax : R_tmp))>>(bps-8);
//      qint64 G_tmp  = (Y_tmp + U_tmp * guMult + V_tmp * gvMult) >> (8+bps);
//      dstMem[i*3+1] = (G_tmp<0 ? 0 : (G_tmp>rgbMax ? rgbMax : G_tmp))>>(bps-8);
//      qint64 B_tmp  = (Y_tmp + U_tmp * buMult                 ) >> (8+bps);
//      dstMem[i*3+2] = (B_tmp<0 ? 0 : (B_tmp>rgbMax ? rgbMax : B_tmp))>>(bps-8);
////the commented section uses RGB 30 format (10 bits per channel)
//
///*
//      qint64 R_tmp  = ((Y_tmp                  + V_tmp * rvMult))>>(8+bps)   ;
//      temp = (R_tmp<0 ? 0 : (R_tmp>rgbMax ? rgbMax : R_tmp));
//      dstMem[i*4+3]   = ((temp>>4) & 0x3F);
//
//      qint64 G_tmp  = ((Y_tmp + U_tmp * guMult + V_tmp * gvMult))>>(8+bps);
//      temp1 = (G_tmp<0 ? 0 : (G_tmp>rgbMax ? rgbMax : G_tmp));
//      dstMem[i*4+2] = ((temp<<4) & 0xF0 ) | ((temp1>>6) & 0x0F);
//
//      qint64 B_tmp  = ((Y_tmp + U_tmp * buMult                 ))>>(8+bps) ;
//      temp=0;
//      temp = (B_tmp<0 ? 0 : (B_tmp>rgbMax ? rgbMax : B_tmp));
//      dstMem[i*4+1] = ((temp1<<2)&0xFC) | ((temp>>8) & 0x03);
//      dstMem[i*4] = temp & 0xFF;
//*/
//    }
//  }
//  else
//    printf("bitdepth %i not supported\n", bps);
//}

QLayout *videoHandlerYUV::createYUVVideoHandlerControls(QWidget *parentWidget, bool isSizeFixed)
{

  // Absolutely always only call this function once!
  assert(!controlsCreated);
  controlsCreated = true;

  QVBoxLayout *newVBoxLayout = NULL;
  if (!isSizeFixed)
  {
    // Our parent (videoHandler) also has controls to add. Create a new vBoxLayout and append the parent controls
    // and our controls into that layout, seperated by a line. Return that layout
    newVBoxLayout = new QVBoxLayout;
    newVBoxLayout->addLayout( frameHandler::createFrameHandlerControls(parentWidget, isSizeFixed) );
  
    QFrame *line = new QFrame(parentWidget);
    line->setObjectName(QStringLiteral("line"));
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    newVBoxLayout->addWidget(line);
  }
  
  // Create the UI and setupt all the controls
  ui->setupUi(parentWidget);

  // Add the preset YUV formats. If the current format is in the list, add it and select it.
  ui->yuvFormatComboBox->addItems(yuvPresetsList.getFormatedNames());
  int idx = yuvPresetsList.indexOf(srcPixelFormat);
  if (idx == -1)
  {
    // The currently set pixel format is not in the presets list. Add and select it.
    ui->yuvFormatComboBox->addItem(srcPixelFormat.getName());
    yuvPresetsList.append(srcPixelFormat);
    idx = yuvPresetsList.indexOf(srcPixelFormat);
  }
  ui->yuvFormatComboBox->setCurrentIndex(idx);
  // Add the custom... entry that allows the user to add custom formats
  ui->yuvFormatComboBox->addItem( "Custom..." );
  ui->yuvFormatComboBox->setEnabled(!isSizeFixed);

  // Set all the values of the properties widget to the values of this class
  ui->colorComponentsComboBox->addItems( QStringList() << "Y'CbCr" << "Luma Only" << "Cb only" << "Cr only" );
  ui->colorComponentsComboBox->setCurrentIndex( (int)componentDisplayMode );
  ui->chromaInterpolationComboBox->addItems( QStringList() << "Nearest neighbour" << "Bilinear" );
  ui->chromaInterpolationComboBox->setCurrentIndex( (int)interpolationMode );
  ui->chromaInterpolationComboBox->setEnabled(srcPixelFormat.subsampled());
  ui->colorConversionComboBox->addItems( QStringList() << "ITU-R.BT709" << "ITU-R.BT601" << "ITU-R.BT202" );
  ui->colorConversionComboBox->setCurrentIndex( (int)yuvColorConversionType );
  ui->lumaScaleSpinBox->setValue( mathParameters[Luma].scale );
  ui->lumaOffsetSpinBox->setMaximum(1000);
  ui->lumaOffsetSpinBox->setValue( mathParameters[Luma].offset );
  ui->lumaInvertCheckBox->setChecked( mathParameters[Luma].invert );
  ui->chromaScaleSpinBox->setValue( mathParameters[Chroma].scale );
  ui->chromaOffsetSpinBox->setMaximum(1000);
  ui->chromaOffsetSpinBox->setValue( mathParameters[Chroma].offset );
  ui->chromaInvertCheckBox->setChecked( mathParameters[Chroma].invert );

  // Connect all the change signals from the controls to "connectWidgetSignals()"
  connect(ui->yuvFormatComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotYUVFormatControlChanged(int)));
  connect(ui->colorComponentsComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotYUVControlChanged()));
  connect(ui->chromaInterpolationComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotYUVControlChanged()));
  connect(ui->colorConversionComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotYUVControlChanged()));
  connect(ui->lumaScaleSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotYUVControlChanged()));
  connect(ui->lumaOffsetSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotYUVControlChanged()));
  connect(ui->lumaInvertCheckBox, SIGNAL(stateChanged(int)), this, SLOT(slotYUVControlChanged()));
  connect(ui->chromaScaleSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotYUVControlChanged()));
  connect(ui->chromaOffsetSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotYUVControlChanged()));
  connect(ui->chromaInvertCheckBox, SIGNAL(stateChanged(int)), this, SLOT(slotYUVControlChanged()));

  if (!isSizeFixed && newVBoxLayout)
    newVBoxLayout->addLayout(ui->topVBoxLayout);

  return (isSizeFixed) ? ui->topVBoxLayout : newVBoxLayout;
}

void videoHandlerYUV::slotYUVFormatControlChanged(int idx)
{
  yuvPixelFormat newFormat;

  if (idx == yuvPresetsList.count())
  {
    // The user selected the "cutom format..." option
    videoHandlerYUV_CustomFormatDialog dialog( srcPixelFormat );
    if (dialog.exec() == QDialog::Accepted && dialog.getYUVFormat().isValid())
    {
      // Set the custom format
      // Check if the user specified a new format
      newFormat = dialog.getYUVFormat();
   
      // Check if the custom format it in the presets list. If not, add it
      int idx = yuvPresetsList.indexOf(newFormat);
      if (idx == -1 && newFormat.isValid())
      {
        // Valid pixel format with is not in the list. Add it...
        yuvPresetsList.append(newFormat);
        int nrItems = ui->yuvFormatComboBox->count();
        disconnect(ui->yuvFormatComboBox, SIGNAL(currentIndexChanged(int)), NULL, NULL);
        ui->yuvFormatComboBox->insertItem(nrItems-1, newFormat.getName() );
        // Setlect the added format
        idx = yuvPresetsList.indexOf(newFormat);
        ui->yuvFormatComboBox->setCurrentIndex(idx);
        connect(ui->yuvFormatComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotYUVFormatControlChanged(int)));
      }
    }
    else
    {
      // The user pressed cancel. Go back to the old format
      int idx = yuvPresetsList.indexOf(srcPixelFormat);
      Q_ASSERT(idx != -1);  // The previously selected format should always be in the list
      disconnect(ui->yuvFormatComboBox, SIGNAL(currentIndexChanged(int)));
      ui->yuvFormatComboBox->setCurrentIndex(idx);
      connect(ui->yuvFormatComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotYUVFormatControlChanged(int)));
    }
  }
  else
    // One of the preset formats was selected
    newFormat = yuvPresetsList.at(idx);

  // Set the new format (if new) and emit a signal that a new format was selected.
  if (srcPixelFormat != newFormat && newFormat.isValid())
    setSrcPixelFormat(newFormat);    
}

void videoHandlerYUV::setSrcPixelFormat(yuvPixelFormat format, bool emitSignal)
{ 
  // Store the nr bytes per frame of the old pixel format
  qint64 oldFormatBytesPerFrame = srcPixelFormat.bytesPerFrame(frameSize);

  // Set the new pixel format. Lock the mutex, so that no background process is running wile the format changes.
  yuvFormatMutex.lock(); 
  srcPixelFormat = format;
  yuvFormatMutex.unlock();

  if (controlsCreated)
    // Every time the pixel format changed, see if the interpolation combo box is enabled/disabled
    ui->chromaInterpolationComboBox->setEnabled(format.subsampled());

  if (emitSignal)
  {
    // Set the current buffers to be invalid and emit the signal that this item needs to be redrawn.
    currentFrameIdx = -1;
    currentImage_frameIndex = -1;
    
    // Clear the cache
    pixmapCache.clear();
    
    if (srcPixelFormat.bytesPerFrame(frameSize) != oldFormatBytesPerFrame)
      // The number of bytes per frame changed. The raw YUV data buffer is also out of date
      currentFrameRawYUVData_frameIdx = -1;
    
    // The number of frames in the sequence might have changed as well
    emit signalUpdateFrameLimits();

    emit signalHandlerChanged(true, true);
  }
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
    mathParameters[Luma].scale = ui->lumaScaleSpinBox->value();
    mathParameters[Luma].offset = ui->lumaOffsetSpinBox->value();
    mathParameters[Luma].invert = ui->lumaInvertCheckBox->isChecked();
    mathParameters[Chroma].scale = ui->chromaScaleSpinBox->value();
    mathParameters[Chroma].offset = ui->chromaOffsetSpinBox->value();
    mathParameters[Chroma].invert = ui->chromaInvertCheckBox->isChecked();

    // Set the current frame in the buffer to be invalid and clear the cache.
    // Emit that this item needs redraw and the cache needs updating.
    currentFrameIdx = -1;
    currentImage_frameIndex = -1;
    pixmapCache.clear();
    emit signalHandlerChanged(true, true);
  }
  else if (sender == ui->yuvFormatComboBox)
  {
    qint64 oldFormatBytesPerFrame = srcPixelFormat.bytesPerFrame(frameSize);

    // Set the new YUV format
    //setSrcPixelFormat( yuvFormatList.getFromName( ui->yuvFormatComboBox->currentText() ) );

    // Check if the new format changed the number of frames in the sequence
    emit signalUpdateFrameLimits();

    // Set the current frame in the buffer to be invalid and clear the cache.
    // Emit that this item needs redraw and the cache needs updating.
    currentFrameIdx = -1;
    currentImage_frameIndex = -1;
    if (srcPixelFormat.bytesPerFrame(frameSize) != oldFormatBytesPerFrame)
      // The number of bytes per frame changed. The raw YUV data buffer also has to be updated.
      currentFrameRawYUVData_frameIdx = -1;
    if (pixmapCache.count() > 0)
      pixmapCache.clear();
    emit signalHandlerChanged(true, true);
  }
}

QPixmap videoHandlerYUV::calculateDifference(frameHandler *item2, int frame, QList<infoItem> &conversionInfoList, int amplificationFactor, bool markDifference)
{
  return QPixmap();

//  videoHandlerYUV *yuvItem2 = dynamic_cast<videoHandlerYUV*>(item2);
//  if (yuvItem2 == NULL)
//    // The given item is not a yuv source. We cannot compare YUV values to non YUV values.
//    // Call the base class comparison function to compare the items using the RGB values.
//    return videoHandler::calculateDifference(item2, frame, conversionInfoList, amplificationFactor, markDifference);
//
//  if (srcPixelFormat.bitsPerSample != yuvItem2->srcPixelFormat.bitsPerSample)
//    // The two items have different bit depths. Compare RGB values instead.
//    // TODO: Or should we do this in the YUV domain somehow?
//    return videoHandler::calculateDifference(item2, frame, conversionInfoList, amplificationFactor, markDifference);
//
//  // Load the right raw YUV data (if not already loaded).
//  // This will just update the raw YUV data. No conversion to pixmap (RGB) is performed. This is either
//  // done on request if the frame is actually shown or has already been done by the caching process.
//  if (!loadRawYUVData(frame))
//    return QPixmap();  // Loading failed
//  if (!yuvItem2->loadRawYUVData(frame))
//    return QPixmap();  // Loading failed
//
//  int width  = qMin(frameSize.width(), yuvItem2->frameSize.width());
//  int height = qMin(frameSize.height(), yuvItem2->frameSize.height());
//
//  const int bps = srcPixelFormat.bitsPerSample;
//  const int diffZero = 128 << (bps - 8);
//  const int maxVal = (1 << bps) - 1;
//
//  QString diffType;
//
//  // Create a YUV444 buffer for the difference
//#if SSE_CONVERSION
//  byteArrayAligned diffYUV;
//  byteArrayAligned tmpDiffBufferRGB;
//#else
//  QByteArray diffYUV;
//  QByteArray tmpDiffBufferRGB;
//#endif
//
//  // Also calculate the MSE while we're at it (Y,U,V)
//  // TODO: Bug: MSE is not scaled correctly in all YUV format cases
//  qint64 mseAdd[3] = {0, 0, 0};

  //if (srcPixelFormat.name == "4:2:0 Y'CbCr 8-bit planar" && yuvItem2->srcPixelFormat.name == "4:2:0 Y'CbCr 8-bit planar")
  //{
  //  // Both items are YUV 4:2:0 8-bit
  //  // We can directly subtract the YUV 4:2:0 values
  //  diffType = "YUV 4:2:0";

  //  DEBUG_YUV( "videoHandlerYUV::calculateDifference YUV420 frame %d\n", frame );

  //  // How many values to go to the next line per input
  //  unsigned int stride0 = frameSize.width();
  //  unsigned int stride1 = yuvItem2->frameSize.width();

  //  // Resize the difference buffer
  //  diffYUV.resize( width * height + (width / 2) * (height / 2) * 2 );  // YUV 4:2:0

  //  unsigned char* src0 = (unsigned char*)currentFrameRawYUVData.data();
  //  unsigned char* src1 = (unsigned char*)yuvItem2->currentFrameRawYUVData.data();

  //  if (markDifference)
  //  {
  //    // We don't want to see the actual difference but just where differences are.
  //    // To be even faster, we will directly draw into the RGB buffer

  //    // Resize the RGB buffer
  //    tmpDiffBufferRGB.resize( width * height * 3 );
  //    unsigned char* dst = (unsigned char*)tmpDiffBufferRGB.data();

  //    // Get pointers to U/V ...
  //    unsigned char* src0U = src0 + (width * height);
  //    unsigned char* src1U = src1 + (width * height);
  //    unsigned char* src0V = src0U + (width / 2 * height / 2);
  //    unsigned char* src1V = src1U + (width / 2 * height / 2);

  //    unsigned int stride0UV = stride0 / 2;
  //    unsigned int stride1UV = stride1 / 2;
  //    unsigned int dstStride = width * 3;

  //    for (int y = 0; y < height; y++)
  //    {
  //      for (int x = 0; x < width; x++)
  //      {
  //        int deltaY = src0[x] - src1[x];
  //        int deltaU = src0U[x/2] - src1U[x/2];
  //        int deltaV = src0V[x/2] - src1V[x/2];

  //        mseAdd[0] += deltaY * deltaY;
  //        mseAdd[1] += deltaU * deltaU;
  //        mseAdd[2] += deltaV * deltaV;

  //        // select RGB color
  //        unsigned char R = 0,G = 0,B = 0;
  //        if (deltaY == 0)
  //        {
  //          G = (deltaU == 0) ? 0 : 70;
  //          B = (deltaV == 0) ? 0 : 70;
  //        }
  //        else
  //        {
  //          if (deltaU == 0 && deltaV == 0)
  //          {
  //            R = 70;
  //            G = 70;
  //            B = 70;
  //          }
  //          else
  //          {
  //            G = (deltaU == 0) ? 0 : 255;
  //            B = (deltaV == 0) ? 0 : 255;
  //          }
  //        }

  //        dst[x*3    ] = R;
  //        dst[x*3 + 1] = G;
  //        dst[x*3 + 2] = B;
  //      }
  //      // Goto next line
  //      src0 += stride0;
  //      src1 += stride1;
  //      if (y % 2 == 1)
  //      {
  //        src0U += stride0UV;
  //        src0V += stride0UV;
  //        src1U += stride1UV;
  //        src1V += stride1UV;
  //      }
  //      dst += dstStride;
  //    }

  //  }
  //  else
  //  {
  //    // 4:2:0 8 bit. Actually calculate the difference between the inputs

  //    unsigned int dstStride = width;
  //    unsigned char* dst  = (unsigned char*)diffYUV.data();

  //    // Luma
  //    for (int y = 0; y < height; y++)
  //    {
  //      for (int x = 0; x < width; x++)
  //      {
  //        int delta = src0[x] - src1[x];
  //        dst[x] = clip( diffZero + delta * amplificationFactor, 0, maxVal);

  //        mseAdd[0] += delta * delta;
  //      }
  //      src0 += stride0;
  //      src1 += stride1;
  //      dst += dstStride;
  //    }

  //    // Chroma
  //    stride0   = stride0   / 2;
  //    stride1   = stride1   / 2;
  //    dstStride = dstStride / 2;
  //    
  //    // Chroma U
  //    src0 = (unsigned char*)currentFrameRawYUVData.data() + (frameSize.width() * frameSize.height());
  //    src1 = (unsigned char*)yuvItem2->currentFrameRawYUVData.data() + (yuvItem2->frameSize.width() * yuvItem2->frameSize.height());
  //    
  //    for (int y = 0; y < height / 2; y++)
  //    {
  //      for (int x = 0; x < width / 2; x++)
  //      {
  //        int delta = src0[x] - src1[x];
  //        dst[x] = clip( diffZero + delta * amplificationFactor, 0, maxVal);

  //        mseAdd[1] += delta * delta;
  //      }
  //      src0 += stride0;
  //      src1 += stride1;
  //      dst += dstStride;
  //    }

  //    // Chroma V
  //    src0 = (unsigned char*)currentFrameRawYUVData.data() + (frameSize.width() * frameSize.height() + (frameSize.width() / 2 * frameSize.height() / 2) );
  //    src1 = (unsigned char*)yuvItem2->currentFrameRawYUVData.data() + (yuvItem2->frameSize.width() * yuvItem2->frameSize.height()) + (yuvItem2->frameSize.width() / 2 * yuvItem2->frameSize.height() / 2);
  //    
  //    for (int y = 0; y < height / 2; y++)
  //    {
  //      for (int x = 0; x < width / 2; x++)
  //      {
  //        int delta = src0[x] - src1[x];
  //        dst[x] = clip( diffZero + delta * amplificationFactor, 0, maxVal);

  //        mseAdd[2] += delta * delta;
  //      }
  //      src0 += stride0;
  //      src1 += stride1;
  //      dst += dstStride;
  //    }
  //          
  //    // Convert to RGB
  //    convertYUV420ToRGB(diffYUV, tmpDiffBufferRGB, QSize(width,height));
  //  }
  //}
  //else if (srcPixelFormat.name == "4:2:2 Y'CbCr 8-bit planar" || yuvItem2->srcPixelFormat.name == "4:2:2 Y'CrCb 8-bit planar")
  //{
  //  // YUV 4:2:2 8 bit
  //  diffType = "YUV 4:2:2";

  //  DEBUG_YUV( "videoHandlerYUV::calculateDifference YUV422 frame %d bps %d\n", frame, bps );

  //  // How many values to go to the next line per input
  //  unsigned int stride0 = frameSize.width();
  //  unsigned int stride1 = yuvItem2->frameSize.width();

  //  // Resize the difference buffer
  //  diffYUV.resize( width * height + (width / 2) * height * 2 );  // YUV 4:2:2

  //  unsigned char* src0 = (unsigned char*)currentFrameRawYUVData.data();
  //  unsigned char* src1 = (unsigned char*)yuvItem2->currentFrameRawYUVData.data();

  //  if (markDifference)
  //  {
  //    // We don't want to see the actual difference but just where differences are.
  //    // To be even faster, we will directly draw into the RGB buffer
  //  }
  //  else
  //  {
  //    // 4:2:2 8 bit. Actually calculate the difference between the inputs
  //    unsigned int dstStride = width;
  //    unsigned char* dst  = (unsigned char*)diffYUV.data();

  //    // Luma
  //    for (int y = 0; y < height; y++)
  //    {
  //      for (int x = 0; x < width; x++)
  //      {
  //        int delta = src0[x] - src1[x];
  //        dst[x] = clip( diffZero + delta * amplificationFactor, 0, maxVal);

  //        mseAdd[0] += delta * delta;
  //      }
  //      src0 += stride0;
  //      src1 += stride1;
  //      dst += dstStride;
  //    }

  //    // Chroma
  //    stride0   = stride0   / 2;
  //    stride1   = stride1   / 2;
  //    dstStride = dstStride / 2;

  //    // Chroma U
  //    src0 = (unsigned char*)currentFrameRawYUVData.data() + (frameSize.width() * frameSize.height());
  //    src1 = (unsigned char*)yuvItem2->currentFrameRawYUVData.data() + (yuvItem2->frameSize.width() * yuvItem2->frameSize.height());

  //    for (int y = 0; y < height; y++)
  //    {
  //      for (int x = 0; x < width / 2; x++)
  //      {
  //        int delta = src0[x] - src1[x];
  //        dst[x] = clip( diffZero + delta * amplificationFactor, 0, maxVal);

  //        mseAdd[1] += delta * delta;
  //      }
  //      src0 += stride0;
  //      src1 += stride1;
  //      dst += dstStride;
  //    }

  //    // Chroma V
  //    src0 = (unsigned char*)currentFrameRawYUVData.data() + (frameSize.width() * frameSize.height() + (frameSize.width() / 2 * frameSize.height()) );
  //    src1 = (unsigned char*)yuvItem2->currentFrameRawYUVData.data() + (yuvItem2->frameSize.width() * yuvItem2->frameSize.height()) + (yuvItem2->frameSize.width() / 2 * yuvItem2->frameSize.height());

  //    for (int y = 0; y < height; y++)
  //    {
  //      for (int x = 0; x < width / 2; x++)
  //      {
  //        int delta = src0[x] - src1[x];
  //        dst[x] = clip( diffZero + delta * amplificationFactor, 0, maxVal);

  //        mseAdd[2] += delta * delta;
  //      }
  //      src0 += stride0;
  //      src1 += stride1;
  //      dst += dstStride;
  //    }

  //    QByteArray tmpDiffBuffer444;
  //    convert2YUV444(diffYUV, tmpDiffBuffer444);
  //    convertYUV4442RGB(tmpDiffBuffer444, tmpDiffBufferRGB);
  //  }
  //}
  //else
  //{
  //  // One (or both) input item(s) is/are not 4:2:0
  //  diffType = "YUV 4:4:4";

  //  DEBUG_YUV( "videoHandlerYUV::calculateDifference YUV444 frame %d bps %d\n", frame, bps );

  //  // How many values to go to the next line per input
  //  const unsigned int stride0 = frameSize.width();
  //  const unsigned int stride1 = yuvItem2->frameSize.width();
  //  const unsigned int dstStride = width;

  //  // How many values to go to the next component? (Luma,Cb,Cr)
  //  const unsigned int componentLength0 = frameSize.width() * frameSize.height();
  //  const unsigned int componentLength1 = yuvItem2->frameSize.width() * yuvItem2->frameSize.height();
  //  const unsigned int componentLengthDst = width * height;

  //  if (bps > 8)
  //  {
  //    // Resize the difference buffer
  //    diffYUV.resize( width * height * 3 * 2 );

  //    // For each component (Luma,Cb,Cr)...
  //    for (int c = 0; c < 3; c++)
  //    {
  //      // Two bytes per value. Get a pointer to the source data.
  //      unsigned short* src0 = (unsigned short*)currentFrameRawYUVData.data() + c * componentLength0;
  //      unsigned short* src1 = (unsigned short*)yuvItem2->currentFrameRawYUVData.data() + c * componentLength1;
  //      unsigned short* dst  = (unsigned short*)diffYUV.data() + c * componentLengthDst;

  //      for (int y = 0; y < height; y++)
  //      {
  //        for (int x = 0; x < width; x++)
  //        {
  //          int delta = src0[x] - src1[x];
  //          dst[x] = clip( diffZero + delta * amplificationFactor, 0, maxVal);

  //          mseAdd[c] += delta * delta;
  //        }
  //        src0 += stride0;
  //        src1 += stride1;
  //        dst += dstStride;
  //      }
  //    }
  //  }
  //  else
  //  {
  //    // Resize the difference buffer
  //    diffYUV.resize( width * height * 3 );

  //    // For each component (Luma,Cb,Cr)...
  //    for (int c = 0; c < 3; c++)
  //    {
  //      // Get a pointer to the source data
  //      unsigned char* src0 = (unsigned char*)currentFrameRawYUVData.data() + c * componentLength0;
  //      unsigned char* src1 = (unsigned char*)yuvItem2->currentFrameRawYUVData.data() + c * componentLength1;
  //      unsigned char* dst  = (unsigned char*)diffYUV.data() + c * componentLengthDst;

  //      for (int y = 0; y < height; y++)
  //      {
  //        for (int x = 0; x < width; x++)
  //        {
  //          int delta = src0[x] - src1[x];
  //          dst[x] = clip( diffZero + delta * amplificationFactor, 0, maxVal);

  //          mseAdd[c] += delta * delta;
  //        }
  //        src0 += stride0;
  //        src1 += stride1;
  //        dst += dstStride;
  //      }
  //    }
  //  }

  //  // Convert to RGB888
  //  convertYUV4442RGB(diffYUV, tmpDiffBufferRGB);
  //}

  //// Append the conversion information that will be returned
  //conversionInfoList.append( infoItem("Difference Type",diffType) );
  //double mse[4];
  //mse[0] = double(mseAdd[0]) / (width * height);
  //mse[1] = double(mseAdd[1]) / (width * height);
  //mse[2] = double(mseAdd[2]) / (width * height);
  //mse[3] = mse[0] + mse[1] + mse[2];
  //conversionInfoList.append( infoItem("MSE Y",QString("%1").arg(mse[0])) );
  //conversionInfoList.append( infoItem("MSE U",QString("%1").arg(mse[1])) );
  //conversionInfoList.append( infoItem("MSE V",QString("%1").arg(mse[2])) );
  //conversionInfoList.append( infoItem("MSE All",QString("%1").arg(mse[3])) );

  //// Convert the image in tmpDiffBufferRGB to a QPixmap using a QImage intermediate.
  //// TODO: Isn't there a faster way to do this? Maybe load a pixmap from "BMP"-like data?
  //QImage tmpImage((unsigned char*)tmpDiffBufferRGB.data(), width, height, QImage::Format_RGB888);
  //QPixmap retPixmap;
  //retPixmap.convertFromImage(tmpImage);
  //return retPixmap;
}

ValuePairList videoHandlerYUV::getPixelValues(QPoint pixelPos, int frameIdx, frameHandler *item2)
{
  ValuePairList values;

  if (item2 != NULL)
  {
    videoHandlerYUV *yuvItem2 = dynamic_cast<videoHandlerYUV*>(item2);
    if (yuvItem2 == NULL)
      // The given item is not a yuv source. We cannot compare YUV values to non YUV values.
      // Call the base class comparison function to compare the items using the RGB values.
      return frameHandler::getPixelValues(pixelPos, frameIdx, item2);

    if (srcPixelFormat.bitsPerSample != yuvItem2->srcPixelFormat.bitsPerSample)
      // The two items have different bit depths. Compare RGB values instead.
      // TODO: Or should we do this in the YUV domain somehow?
      return frameHandler::getPixelValues(pixelPos, frameIdx, item2);

    int width  = qMin(frameSize.width(), yuvItem2->frameSize.width());
    int height = qMin(frameSize.height(), yuvItem2->frameSize.height());

    if (pixelPos.x() < 0 || pixelPos.x() >= width || pixelPos.y() < 0 || pixelPos.y() >= height)
      return ValuePairList();

    unsigned int Y0, U0, V0, Y1, U1, V1;
    getPixelValue(pixelPos, frameIdx, Y0, U0, V0);
    yuvItem2->getPixelValue(pixelPos, frameIdx, Y1, U1, V1);

    values.append( ValuePair("Y", QString::number((int)Y0-(int)Y1)) );
    values.append( ValuePair("U", QString::number((int)U0-(int)U1)) );
    values.append( ValuePair("V", QString::number((int)V0-(int)V1)) );
  }
  else
  {
    int width = frameSize.width();
    int height = frameSize.height();

    if (pixelPos.x() < 0 || pixelPos.x() >= width || pixelPos.y() < 0 || pixelPos.y() >= height)
      return ValuePairList();

    unsigned int Y,U,V;
    getPixelValue(pixelPos, frameIdx, Y, U, V);

    values.append( ValuePair("Y", QString::number(Y)) );
    values.append( ValuePair("U", QString::number(U)) );
    values.append( ValuePair("V", QString::number(V)) );  
  }
  
  return values;
}

void videoHandlerYUV::drawPixelValues(QPainter *painter, int frameIdx,  QRect videoRect, double zoomFactor, frameHandler *item2)
{
  // Get the other YUV item (if any)
  videoHandlerYUV *yuvItem2 = NULL;
  if (item2 != NULL)
    yuvItem2 = dynamic_cast<videoHandlerYUV*>(item2);

  // First determine which pixels from this item are actually visible, because we only have to draw the pixel values
  // of the pixels that are actually visible
  QRect viewport = painter->viewport();
  QTransform worldTransform = painter->worldTransform();
    
  int xMin = (videoRect.width() / 2 - worldTransform.dx()) / zoomFactor;
  int yMin = (videoRect.height() / 2 - worldTransform.dy()) / zoomFactor;
  int xMax = (videoRect.width() / 2 - (worldTransform.dx() - viewport.width() )) / zoomFactor;
  int yMax = (videoRect.height() / 2 - (worldTransform.dy() - viewport.height() )) / zoomFactor;

  // Clip the min/max visible pixel values to the size of the item (no pixels outside of the
  // item have to be labeled)
  xMin = clip(xMin, 0, frameSize.width()-1);
  yMin = clip(yMin, 0, frameSize.height()-1);
  xMax = clip(xMax, 0, frameSize.width()-1);
  yMax = clip(yMax, 0, frameSize.height()-1);

  // The center point of the pixel (0,0).
  QPoint centerPointZero = ( QPoint(-frameSize.width(), -frameSize.height()) * zoomFactor + QPoint(zoomFactor,zoomFactor) ) / 2;
  // This rect has the size of one pixel and is moved on top of each pixel to draw the text
  QRect pixelRect;
  pixelRect.setSize( QSize(zoomFactor, zoomFactor) );

  // We might change the pen doing this so backup the current pen and reset it later
  QPen backupPen = painter->pen();

  // If the Y is below this value, use white text, otherwise black text
  // If there is a second item, a difference will be drawn. A difference of 0 is displayed as gray.
  int whiteLimit = (yuvItem2) ? 0 : 1 << (srcPixelFormat.bitsPerSample - 1);

  if (srcPixelFormat.getSubsamplingHor() == 1 && srcPixelFormat.getSubsamplingVer() == 1)
  {
    // YUV 444 format. We draw all values in the center of each pixel
    for (int x = xMin; x <= xMax; x++)
    {
      for (int y = yMin; y <= yMax; y++)
      {
        // Calculate the center point of the pixel. (Each pixel is of size (zoomFactor,zoomFactor)) and move the pixelRect to that point.
        QPoint pixCenter = centerPointZero + QPoint(x * zoomFactor, y * zoomFactor);
        pixelRect.moveCenter(pixCenter);

        // Get the text to show
        QString valText;
        if (yuvItem2 != NULL)
        {
          unsigned int Y0, U0, V0, Y1, U1, V1;
          getPixelValue(QPoint(x,y), frameIdx, Y0, U0, V0);
          yuvItem2->getPixelValue(QPoint(x,y), frameIdx, Y1, U1, V1);

          int dY = Y0 - Y1;
          int dU = U0 - U1;
          int dV = V0 - V1;

          valText = QString("Y%1\nU%2\nV%3").arg(dY).arg(dU).arg(dV);
          bool drawWhite = (mathParameters[Luma].invert) ? ((dY) > whiteLimit) : ((dY) < whiteLimit);
          painter->setPen( drawWhite ? Qt::white : Qt::black );
        }
        else
        {
          unsigned int Y, U, V;
          getPixelValue(QPoint(x,y), frameIdx, Y, U, V);
          valText = QString("Y%1\nU%2\nV%3").arg(Y).arg(U).arg(V);
          bool drawWhite = (mathParameters[Luma].invert) ? ((int)Y > whiteLimit) : ((int)Y < whiteLimit);
          painter->setPen( drawWhite ? Qt::white : Qt::black );
        }

        painter->drawText(pixelRect, Qt::AlignCenter, valText);
      }
    }
  }
  else if (srcPixelFormat.getSubsamplingHor() <= 4 && srcPixelFormat.getSubsamplingVer() <= 4)
  {
    // Non YUV 444 format. The Y values go into the center of each pixel, but the U and V values go somewhere else,
    // depending on the srcPixelFormat.
    for (int x = (xMin == 0) ? 0 : xMin - 1; x <= xMax; x++)
    {
      for (int y = (yMin == 0) ? 0 : yMin - 1; y <= yMax; y++)
      {
        // Calculate the center point of the pixel. (Each pixel is of size (zoomFactor,zoomFactor)) and move the pixelRect to that point.
        QPoint pixCenter = centerPointZero + QPoint(x * zoomFactor, y * zoomFactor);
        pixelRect.moveCenter(pixCenter);

        // Get the YUV values to show
        int Y,U,V;
        if (yuvItem2 != NULL)
        {
          unsigned int Y0, U0, V0, Y1, U1, V1;
          getPixelValue(QPoint(x,y), frameIdx, Y0, U0, V0);
          yuvItem2->getPixelValue(QPoint(x,y), frameIdx, Y1, U1, V1);

          Y = Y0-Y1; U = U0-U1; V = V0-V1;
        }
        else
        {
          unsigned int Yu,Uu,Vu;
          getPixelValue(QPoint(x,y), frameIdx, Yu, Uu, Vu);
          Y = Yu; U = Uu; V = Vu;
        }

        QString valText = QString("Y%1").arg(Y);

        bool drawWhite = (mathParameters[Luma].invert) ? ((int)Y > whiteLimit) : ((int)Y < whiteLimit);
        painter->setPen( drawWhite ? Qt::white : Qt::black );
        painter->drawText(pixelRect, Qt::AlignCenter, valText);
        
        // Now draw the UV values
        valText = QString("U%1\nV%2").arg(U).arg(V);
        if (srcPixelFormat.getSubsamplingHor() == 2 && srcPixelFormat.getSubsamplingVer() == 1 && (x % 2) == 0)
        {
          // Horizontal sub sampling by 2 and x is even. Draw the U and V values in the center of the two horizontal pixels (x and x+1)
          pixelRect.translate(zoomFactor/2, 0);
          painter->drawText(pixelRect, Qt::AlignCenter, valText);
        }
        if (srcPixelFormat.getSubsamplingHor() == 1 && srcPixelFormat.getSubsamplingVer() == 2 && (y % 2) == 0)
        {
          // Vertical sub sampling by 2 and y is even. Draw the U and V values in the center of the two vertical pixels (y and y+1)
          pixelRect.translate(0, zoomFactor/2);
          painter->drawText(pixelRect, Qt::AlignCenter, valText);
        }
        if (srcPixelFormat.getSubsamplingHor() == 2 && srcPixelFormat.getSubsamplingVer() == 2 && (x % 2) == 0 && (y % 2) == 0)
        {
          // Horizontal and vertical sub sampling by 2 and x and y are even. Draw the U and V values in the center of the four pixels
          pixelRect.translate(zoomFactor/2, zoomFactor/2);
          painter->drawText(pixelRect, Qt::AlignCenter, valText);
        }
        if (srcPixelFormat.getSubsamplingHor() == 4 && srcPixelFormat.getSubsamplingVer() == 1 && (x % 4) == 0)
        {
          // Horizontal subsampling by 4 and x is divisible by 4 so draw the UV values in the center of the four pixels
          pixelRect.translate(zoomFactor+zoomFactor/2, 0);
          painter->drawText(pixelRect, Qt::AlignCenter, valText);
        }
        if (srcPixelFormat.getSubsamplingHor() == 4 && srcPixelFormat.getSubsamplingVer() == 4 && (x % 4) == 0 && (y % 4) == 0)
        {
          // Horizontal subsampling by 4 and x is divisible by 4. Same for vertical. So draw the UV values in the center of the 4x4 pixels
          pixelRect.translate(zoomFactor+zoomFactor/2, zoomFactor+zoomFactor/2);
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

void videoHandlerYUV::setFormatFromSizeAndName(QSize size, int &rate, int &bitDepth, qint64 fileSize, QFileInfo fileInfo)
{
  // If the bit depth could not be determined, check 8 and 10 bit
  int testBitDepths = (bitDepth > 0) ? 1 : 2;

  // Lets try to get a yuv format from the file name.
  QString fileName = fileInfo.fileName();
  
  // First, lets see if there is a yuv format defined as ffmpeg names them:
  // First the YUV order, then the subsampling, then a 'p' if the format is planar, then the number of bits (if > 8), finally 'le' or 'be' if bits is > 8.
  // E.g: yuv420p, yuv420p10le, yuv444p16be
  QStringList subsamplingNameList = QStringList() << "444" << "422" << "420" << "440" << "410" << "411" << "400";
  QList<int> bitDepthList = QList<int>() << 9 << 10 << 12 << 14 << 16 << 8;

  // First all planar formats
  QStringList planarYUVOrderList = QStringList() << "yuv" << "yuvj" << "yvu" << "yuva" << "yvua";
  for (int o = 0; o < planarYUVOrderList.count(); o++)
  {
    YUVPlaneOrder planeOrder = (o > 0) ? static_cast<YUVPlaneOrder>(o-1) : static_cast<YUVPlaneOrder>(o);

    for (int s = 0; s < YUV_NUM_SUBSAMPLINGS; s++)
    {
      YUVSubsamplingType subsampling = static_cast<YUVSubsamplingType>(s);
      Q_FOREACH(int bitDepth, bitDepthList)
      {
        QStringList endianessList = QStringList() << "le";
        if (bitDepth > 8)
          endianessList << "be";

        Q_FOREACH(QString endianess, endianessList)
        {
          QString formatName = planarYUVOrderList[o] + subsamplingNameList[s] + "p";
          if (bitDepth > 8)
            formatName += QString::number(bitDepth) + endianess;

          // Check if this format is in the file name
          if (fileName.contains(formatName))
          {
            // Check if the format and the file size match
            yuvPixelFormat fmt = yuvPixelFormat(subsampling, bitDepth, planeOrder, endianess=="be");
            int bpf = fmt.bytesPerFrame(size);
            if (bpf != 0 && (fileSize % bpf) == 0)
            {
              // Bits per frame and file size match
              setSrcPixelFormat(fmt);
              return;
            }
          }
        }
      }
    }
  }

  for (int i = 0; i < testBitDepths; i++)
  {
    if (testBitDepths == 2)
      bitDepth = (i == 0) ? 8 : 10;

    //if (bitDepth==8)
    //{
    //  // assume 4:2:0 if subFormat does not indicate anything else
    //  yuvPixelFormat cFormat = yuvFormatList.getFromName( "4:2:0 Y'CbCr 8-bit planar" );
    //  if (subFormat == "444")
    //    cFormat = yuvFormatList.getFromName( "4:4:4 Y'CbCr 8-bit planar" );
    //  if (subFormat == "422")
    //    cFormat = yuvFormatList.getFromName( "4:2:2 Y'CbCr 8-bit planar" );

    //  // Check if the file size and the assumed format match
    //  int bpf = cFormat.bytesPerFrame( size );
    //  if (bpf != 0 && (fileSize % bpf) == 0)
    //  {
    //    // Bits per frame and file size match
    //    setFrameSize(size);
    //    setSrcPixelFormat( cFormat );
    //    return;
    //  }
    //}
    //else if (bitDepth==10)
    //{
    //  // Assume 444 format if subFormat is set. Otherwise assume 420
    //  yuvPixelFormat cFormat = yuvFormatList.getFromName( "4:2:0 Y'CbCr 10-bit LE planar" );
    //  if (subFormat == "444")
    //    cFormat = yuvFormatList.getFromName( "4:4:4 Y'CbCr 10-bit LE planar" );
    //  
    //  // Check if the file size and the assumed format match
    //  int bpf = cFormat.bytesPerFrame( size );
    //  if (bpf != 0 && (fileSize % bpf) == 0)
    //  {
    //    // Bits per frame and file size match
    //    setFrameSize(size);
    //    setSrcPixelFormat( cFormat );
    //    return;
    //  }
    //}
    //else
    //{
    //    // do other stuff
    //}
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
  if(rawYUVData.size() < 1)
    return;

  class testFormatAndSize
  {
  public:
    testFormatAndSize(QSize size, yuvPixelFormat format) : size(size), format(format) { interesting = false; mse = 0; };
    QSize size;
    yuvPixelFormat format;
    bool interesting;
    double mse;
  };

  // The candidates for the size
  QList<QSize> testSizes = {
    QSize(176,144),
    QSize(352,240),
    QSize(352,288),
    QSize(480,480),
    QSize(480,576),
    QSize(704,480),
    QSize(720,480),
    QSize(704,576),
    QSize(720,576),
    QSize(1024,768),
    QSize(1280,720),
    QSize(1280,960),
    QSize(1920,1072),
    QSize(1920,1080)
  };

  // Test bit depths 8, 10 and 16
  QList<testFormatAndSize> formatList;
  for (int b = 0; b < 3; b++)
  {
    int bits = (b==0) ? 8 : (b==1) ? 10 : 16;
    // Test all subsamplings
    for (int i = 0; i < YUV_NUM_SUBSAMPLINGS; i++)
    {
      YUVSubsamplingType subsampling = static_cast<YUVSubsamplingType>(i);
      Q_FOREACH(QSize size, testSizes)
      {
        formatList.append(testFormatAndSize(size, yuvPixelFormat(subsampling, bits, Order_YUV)));
      }
    }
  }

  if (fileSize > 0)
  {
    // if any candidate exceeds file size for two frames, discard
    // if any candidate does not represent a multiple of file size, discard
    bool found = false;

    QMutableListIterator<testFormatAndSize> i(formatList);
    while(i.hasNext())
    {
      testFormatAndSize testFormat = i.next();
      qint64 picSize = testFormat.format.bytesPerFrame(testFormat.size);

      if( fileSize >= (picSize*2) )       // at least 2 pics for correlation analysis
      {
        if( (fileSize % picSize) == 0 )   // important: file size must be multiple of pic size
        {
          testFormat.interesting = true;  // test passed
          i.setValue(testFormat);         // Modify the list item
          found = true;
        }
      }
    }

    if( !found )
      // No candidate matches the file size
      return;
  }

  // calculate max. correlation for first two frames, use max. candidate frame size
  QMutableListIterator<testFormatAndSize> i(formatList);
  while(i.hasNext())
  {
    testFormatAndSize testFormat = i.next();
    if (testFormat.interesting)
    {
      qint64 picSize = testFormat.format.bytesPerFrame(testFormat.size);

      // Calculate the MSE for 2 frames
      if (testFormat.format.bitsPerSample == 8)
      {
        unsigned char *ptr = (unsigned char*) rawYUVData.data();
        testFormat.mse = computeMSE(ptr, ptr + picSize, picSize);
      }
      else if (testFormat.format.bitsPerSample > 8 && testFormat.format.bitsPerSample <= 16)
      {
        unsigned short *ptr = (unsigned short*) rawYUVData.data();
        testFormat.mse = computeMSE(ptr, ptr + picSize, picSize);
      }
      else
        // Not handled here
        continue;

      i.setValue(testFormat); // Set the iterator value
    }
  }

  // step3: select best candidate
  double leastMSE = std::numeric_limits<double>::max(); // large error...
  yuvPixelFormat bestFormat;
  QSize bestSize;
  Q_FOREACH(testFormatAndSize testFormat, formatList)
  {
    if (testFormat.interesting && testFormat.mse < leastMSE)
    {
      bestFormat = testFormat.format;
      bestSize = testFormat.size;
      leastMSE = testFormat.mse;
    }
  }

  if( leastMSE < 400 )
  {
    // MSE is below threshold. Choose the candidate.
    srcPixelFormat = bestFormat;
    setFrameSize(bestSize);
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
    convertYUVToPixmap(currentFrameRawYUVData, currentFrame, tmpBufferRGB);
    currentFrameIdx = frameIndex;
  }
}

void videoHandlerYUV::loadFrameForCaching(int frameIndex, QPixmap &frameToCache)
{
  DEBUG_YUV( "videoHandlerYUV::loadFrameForCaching %d", frameIndex );

  // Lock the mutex for the yuvFormat. The main thread has to wait until caching is done
  // before the yuv format can change.
  yuvFormatMutex.lock();

  requestDataMutex.lock();
  emit signalRequesRawData(frameIndex);
  tmpBufferRawYUVDataCaching = rawYUVData;
  requestDataMutex.unlock();

  if (frameIndex != rawYUVData_frameIdx)
  {
    // Loading failed
    currentFrameIdx = -1;
    yuvFormatMutex.unlock();
    return;
  }

  // Convert YUV to pixmap. This can then be cached.
  convertYUVToPixmap(tmpBufferRawYUVDataCaching, frameToCache, tmpBufferRGBCaching);

  yuvFormatMutex.unlock();
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
  requestDataMutex.lock();
  emit signalRequesRawData(frameIndex);

  if (frameIndex != rawYUVData_frameIdx)
  {
    // Loading failed
    currentFrameRawYUVData_frameIdx = -1;
  }
  else
  {
    currentFrameRawYUVData = rawYUVData;
    currentFrameRawYUVData_frameIdx = frameIndex;
  }

  requestDataMutex.unlock();
  return (currentFrameRawYUVData_frameIdx == frameIndex);
}

bool videoHandlerYUV::convertYUVPackedToPlanar(QByteArray &sourceBuffer, QByteArray &targetBuffer, const QSize curFrameSize)
{
  return false;
}

inline int clip8Bit(int val)
{
  if (val < 0)
    return 0;
  if (val > 255)
    return 255;
  return val;
}

/* Apply the given transformation to the YUV sample. If invert is true, the sample is inverted at the value defined by offset. 
 * If the scale is greater one, the values will be amplified relative to the offset value.
 * The input can be 8 to 16 bit. The output will be of the same bit depth. The output is clamped to (0...clipMax).
 */
inline int transformYUV(const bool invert, const int scale, const int offset, const unsigned int value, const int clipMax)
{
  int newValue = value;
  if (invert)
    newValue = -(newValue - offset) * scale + offset;  // Scale + Offset + Invert
  else
    newValue = (newValue - offset) * scale + offset;  // Scale + Offset

  // Clip to 8 bit
  if (newValue < 0)
    newValue = 0;
  if (newValue > clipMax)
    newValue = clipMax;

  return newValue;
}

inline void convertYUVToRGB8Bit(const unsigned int valY, const unsigned int valU, const unsigned int valV, int &valR, int &valG, int &valB, const int RGBConv[5], const int bps)
{
  if (bps > 14)
  {
    // The bit depth of an int (32) is not enough to perform a YUV -> RGB conversion for a bit depth > 14 bits.
    // We could use 64 bit values but for what? We are clipping the result to 8 bit anyways so let's just
    // get rid of 2 of the bits for the YUV values.
    const int yOffset = 16<<(bps-10);
    const int cZero = 128<<(bps-10);

    const int Y_tmp = ((valY >> 2) - yOffset) * RGBConv[0];
    const int U_tmp = (valU >> 2) - cZero;
    const int V_tmp = (valV >> 2) - cZero;

    const int R_tmp = (Y_tmp                      + V_tmp * RGBConv[1] ) >> (16 + bps - 10); //32 to 16 bit conversion by right shifting
    const int G_tmp = (Y_tmp + U_tmp * RGBConv[2] + V_tmp * RGBConv[3] ) >> (16 + bps - 10);
    const int B_tmp = (Y_tmp + U_tmp * RGBConv[4]                      ) >> (16 + bps - 10);

    valR = (R_tmp < 0) ? 0 : (R_tmp > 255) ? 255 : R_tmp;
    valG = (G_tmp < 0) ? 0 : (G_tmp > 255) ? 255 : G_tmp;
    valB = (B_tmp < 0) ? 0 : (B_tmp > 255) ? 255 : B_tmp;
  }
  else
  {
    const int yOffset = 16<<(bps-8);
    const int cZero = 128<<(bps-8);

    const int Y_tmp = (valY - yOffset) * RGBConv[0];
    const int U_tmp = valU - cZero;
    const int V_tmp = valV - cZero;
    
    const int R_tmp = (Y_tmp                      + V_tmp * RGBConv[1] ) >> (16 + bps - 8); //32 to 16 bit conversion by right shifting
    const int G_tmp = (Y_tmp + U_tmp * RGBConv[2] + V_tmp * RGBConv[3] ) >> (16 + bps - 8);
    const int B_tmp = (Y_tmp + U_tmp * RGBConv[4]                      ) >> (16 + bps - 8);
    
    valR = (R_tmp < 0) ? 0 : (R_tmp > 255) ? 255 : R_tmp;
    valG = (G_tmp < 0) ? 0 : (G_tmp > 255) ? 255 : G_tmp;
    valB = (B_tmp < 0) ? 0 : (B_tmp > 255) ? 255 : B_tmp;
  }
}

inline unsigned int getValueFromSource(const unsigned char * restrict src, const int idx, const int bps, const bool bigEndian)
{
  if (bps > 8)
    // Read two bytes in the right order
    return (bigEndian) ? src[idx*2] << 8 | src[idx*2+1] : src[idx*2] | src[idx*2+1] << 8;
  else
    // Just read one byte
    return src[idx];
}

// For every input sample in src, apply YUV transformation, (scale to 8 bit if required) and set the value as RGB (monochrome).
inline void YUVPlaneToRGBMonochrome_444(const int componentSize, const yuvMathParameters math, const unsigned char * restrict src, unsigned char * restrict dst, 
                                        const int inMax, const int bps, const bool bigEndian)
{
  const bool applyMath = math.yuvMathRequired();
  const int shiftTo8Bit = bps - 8;
  for (int i = 0; i < componentSize; ++i)
  {
    int newVal = getValueFromSource(src, i, bps, bigEndian);
    if (applyMath)
      newVal = transformYUV(math.invert, math.scale, math.offset, newVal, inMax);

    // Scale to 8 bit (if required)
    if (shiftTo8Bit > 0)
      newVal = clip8Bit(newVal >> shiftTo8Bit);
    // Set the value for R, G and B
    dst[i*3  ] = (unsigned char)newVal;
    dst[i*3+1] = (unsigned char)newVal;
    dst[i*3+2] = (unsigned char)newVal;
  }
}

// For every input sample in the YZV 422 src, apply interpolation (sample and hold), apply YUV transformation, (scale to 8 bit if required) 
// and set the value as RGB (monochrome).
inline void YUVPlaneToRGBMonochrome_422(const int componentSize, const yuvMathParameters math, const unsigned char * restrict src, unsigned char * restrict dst, 
                                        const int inMax, const int bps, const bool bigEndian)
{
  const bool applyMath = math.yuvMathRequired();
  const int shiftTo8Bit = bps - 8;
  for (int i = 0; i < componentSize; ++i)
  {
    int newVal = getValueFromSource(src, i, bps, bigEndian);
    if (applyMath)
      newVal = transformYUV(math.invert, math.scale, math.offset, newVal, inMax);

    // Scale and clip to 8 bit
    if (shiftTo8Bit > 0)
      newVal = clip8Bit(newVal >> shiftTo8Bit);
    // Set the value for R, G and B of 2 pixels
    dst[i*6  ] = (unsigned char)newVal;
    dst[i*6+1] = (unsigned char)newVal;
    dst[i*6+2] = (unsigned char)newVal;
    dst[i*6+3] = (unsigned char)newVal;
    dst[i*6+4] = (unsigned char)newVal;
    dst[i*6+5] = (unsigned char)newVal;
  }
}

inline void YUVPlaneToRGBMonochrome_420(const int w, const int h, const yuvMathParameters math, const unsigned char * restrict src, unsigned char * restrict dst, 
                                        const int inMax, const int bps, const bool bigEndian)
{
  const bool applyMath = math.yuvMathRequired();
  const int shiftTo8Bit = bps - 8;
  for (int y = 0; y < h/2; y++)
    for (int x = 0; x < w/2; x++)
    {
      int newVal = getValueFromSource(src, y*(w/2)+x, bps, bigEndian);
      if (applyMath)
        newVal = transformYUV(math.invert, math.scale, math.offset, newVal, inMax);

      // Scale and clip to 8 bit
      if (shiftTo8Bit > 0)
        newVal = clip8Bit(newVal << shiftTo8Bit);
      // Set the value for R, G and B of 4 pixels
      int o = (y*2*w + x*2)*3;
      dst[o  ] = (unsigned char)newVal;
      dst[o+1] = (unsigned char)newVal;
      dst[o+2] = (unsigned char)newVal;
      dst[o+3] = (unsigned char)newVal;
      dst[o+4] = (unsigned char)newVal;
      dst[o+5] = (unsigned char)newVal;
      o += w*3;
      dst[o  ] = (unsigned char)newVal;
      dst[o+1] = (unsigned char)newVal;
      dst[o+2] = (unsigned char)newVal;
      dst[o+3] = (unsigned char)newVal;
      dst[o+4] = (unsigned char)newVal;
      dst[o+5] = (unsigned char)newVal;
    }
}

inline void YUVPlaneToRGBMonochrome_440(const int w, const int h, const yuvMathParameters math, const unsigned char * restrict src, unsigned char * restrict dst, 
                                        const int inMax, const int bps, const bool bigEndian)
{
  const bool applyMath = math.yuvMathRequired();
  const int shiftTo8Bit = bps - 8;
  for (int y = 0; y < h/2; y++)
    for (int x = 0; x < w; x++)
    {
      int newVal = getValueFromSource(src, y*w+x, bps, bigEndian);
      if (applyMath)
        newVal = transformYUV(math.invert, math.scale, math.offset, newVal, inMax);

      // Scale and clip to 8 bit
      if (shiftTo8Bit > 0)
        newVal = clip8Bit(newVal >> shiftTo8Bit);
      // Set the value for R, G and B of 2 pixels
      dst[(y*2*w+x)*3  ] = (unsigned char)newVal;
      dst[(y*2*w+x)*3+1] = (unsigned char)newVal;
      dst[(y*2*w+x)*3+2] = (unsigned char)newVal;
      dst[((y*2+1)*w+x)*3  ] = (unsigned char)newVal;
      dst[((y*2+1)*w+x)*3+1] = (unsigned char)newVal;
      dst[((y*2+1)*w+x)*3+2] = (unsigned char)newVal;
    }
}

inline void YUVPlaneToRGBMonochrome_411(const int componentSize, const yuvMathParameters math, const unsigned char * restrict src, unsigned char * restrict dst, 
                                        const int inMax, const int bps, const bool bigEndian)
{
  // Horizontally U and V are subsampled by 4
  const bool applyMath = math.yuvMathRequired();
  const int shiftTo8Bit = bps - 8;
  for (int i = 0; i < componentSize; ++i)
  {
    int newVal = getValueFromSource(src, i, bps, bigEndian);
    if (applyMath)
      newVal = transformYUV(math.invert, math.scale, math.offset, newVal, inMax);

    // Scale and clip to 8 bit
    if (shiftTo8Bit > 0)
      newVal = clip8Bit(newVal >> shiftTo8Bit);
    // Set the value for R, G and B of 2 pixels
    dst[i*12   ] = (unsigned char)newVal;
    dst[i*12+1 ] = (unsigned char)newVal;
    dst[i*12+2 ] = (unsigned char)newVal;
    dst[i*12+3 ] = (unsigned char)newVal;
    dst[i*12+4 ] = (unsigned char)newVal;
    dst[i*12+5 ] = (unsigned char)newVal;
    dst[i*12+6 ] = (unsigned char)newVal;
    dst[i*12+7 ] = (unsigned char)newVal;
    dst[i*12+8 ] = (unsigned char)newVal;
    dst[i*12+9 ] = (unsigned char)newVal;
    dst[i*12+10] = (unsigned char)newVal;
    dst[i*12+11] = (unsigned char)newVal;
  }
}

inline void YUVPlaneToRGBMonochrome_410(const int w, const int h, const yuvMathParameters math, const unsigned char * restrict src, unsigned char * restrict dst, 
                                        const int inMax, const int bps, const bool bigEndian)
{
  // Horizontal subsampling by 4, vertical subsampling by 4
  const bool applyMath = math.yuvMathRequired();
  const int shiftTo8Bit = bps - 8;
  for (int y = 0; y < h/4; y++)
    for (int x = 0; x < w/4; x++)
    {
      int newVal = getValueFromSource(src, y*(w/4)+x, bps, bigEndian);

      if (applyMath)
        newVal = transformYUV(math.invert, math.scale, math.offset, newVal, inMax);

      // Scale and clip to 8 bit
      if (shiftTo8Bit > 0)
        newVal = clip8Bit(newVal >> shiftTo8Bit);
      // Set the value as RGB for 4 pixels in this line and the next 3 lines
      for (int yo = 0; yo < 4; yo++)
        for (int xo = 0; xo < 4; xo++)
        {
          dst[((y*4+yo)*w+(x*4+xo))*3  ] = (unsigned char)newVal;
          dst[((y*4+yo)*w+(x*4+xo))*3+1] = (unsigned char)newVal;
          dst[((y*4+yo)*w+(x*4+xo))*3+2] = (unsigned char)newVal;
        }
    }
}

inline int interpolateUVSample(const InterpolationMode mode, const int sample1, const int sample2)
{
  if (mode == NearestNeighborInterpolation)
    return sample1;   // Sample and hold
  if (mode == BiLinearInterpolation)
    // Interpolate linearly between sample1 and sample2
    return ((sample1 + sample2) + 1) >> 1;
}

inline int interpolateUVSampleQ(const InterpolationMode mode, const int sample1, const int sample2, const int quarterPos)
{
  if (mode == NearestNeighborInterpolation)
    return sample1;   // Sample and hold
  if (mode == BiLinearInterpolation)
  {
    // Interpolate linearly between sample1 and sample2
    if (quarterPos == 0)
      return sample1;
    if (quarterPos == 1)
      return ((sample1*3 + sample2) + 1) >> 2;
    if (quarterPos == 2)
      return ((sample1 + sample2) + 1) >> 1;
    if (quarterPos == 3)
      return ((sample1 + sample2*3) + 1) >> 2;
  }
}

// TODO: Consider sample position
inline int interpolateUVSample2D(const InterpolationMode mode, const int sample1, const int sample2, const int sample3, const int sample4)
{
  if (mode == NearestNeighborInterpolation)
    return sample1;   // Sample and hold
  if (mode == BiLinearInterpolation)
    // Interpolate linearly between sample1 - sample 4
    return ((sample1 + sample2 + sample3 + sample4) + 2) >> 2;
}

inline void YUVPlaneToRGB_444(const int componentSize, const yuvMathParameters mathY, const yuvMathParameters mathC, 
                              const unsigned char * restrict srcY, const unsigned char * restrict srcU, const unsigned char * restrict srcV,
                              unsigned char * restrict dst, const int RGBConv[5], const int inMax, const int bps, const bool bigEndian)
{
  const bool applyMathLuma = mathY.yuvMathRequired();
  const bool applyMathChroma = mathC.yuvMathRequired();

  for (int i = 0; i < componentSize; ++i)
  {
    unsigned int valY = getValueFromSource(srcY, i, bps, bigEndian);
    unsigned int valU = getValueFromSource(srcU, i, bps, bigEndian);
    unsigned int valV = getValueFromSource(srcV, i, bps, bigEndian);

    if (applyMathLuma)
      valY = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY, inMax);
    if (applyMathChroma)
    {
      valU = transformYUV(mathC.invert, mathC.scale, mathC.offset, valU, inMax);
      valV = transformYUV(mathC.invert, mathC.scale, mathC.offset, valV, inMax);
    }
    
    // Get the RGB values for this sample
    int valR, valG, valB;
    convertYUVToRGB8Bit(valY, valU, valV, valR, valG, valB, RGBConv, bps);

    // Save the RGB values
    dst[i*3  ] = valR;
    dst[i*3+1] = valG;
    dst[i*3+2] = valB;
  }
}

inline void YUVPlaneToRGB_422(const int w, const int h, const yuvMathParameters mathY, const yuvMathParameters mathC, 
                              const unsigned char * restrict srcY, const unsigned char * restrict srcU, const unsigned char * restrict srcV,
                              unsigned char * restrict dst, const int RGBConv[5], const int inMax, const InterpolationMode interpolation, const int bps, const bool bigEndian)
{
  const bool applyMathLuma = mathY.yuvMathRequired();
  const bool applyMathChroma = mathC.yuvMathRequired();
  // Horizontal upsampling is required. Process two Y values at a time
  for (int y = 0; y < h; y++)
  {
    int curUSample = getValueFromSource(srcU, y*w/2, bps, bigEndian);
    int curVSample = getValueFromSource(srcV, y*w/2, bps, bigEndian);
    if (applyMathChroma)
    {
      curUSample = transformYUV(mathC.invert, mathC.scale, mathC.offset, curUSample, inMax);
      curVSample = transformYUV(mathC.invert, mathC.scale, mathC.offset, curVSample, inMax);
    }
    
    for (int x = 0; x < (w/2)-1; x++)
    {
      // Get the next U/V sample
      int nextUSample = getValueFromSource(srcU, y*w/2+x+1, bps, bigEndian);
      int nextVSample = getValueFromSource(srcV, y*w/2+x+1, bps, bigEndian);
      if (applyMathChroma)
      {
        nextUSample = transformYUV(mathC.invert, mathC.scale, mathC.offset, nextUSample, inMax);
        nextVSample = transformYUV(mathC.invert, mathC.scale, mathC.offset, nextVSample, inMax);
      }
      
      // From the current and the next U/V sample, interpolate the UV sample in between
      int interpolatedU = interpolateUVSample(interpolation, curUSample, nextUSample);
      int interpolatedV = interpolateUVSample(interpolation, curVSample, nextVSample);

      // Get the 2 Y samples
      int valY1 = getValueFromSource(srcY, y*w+x*2,   bps, bigEndian);
      int valY2 = getValueFromSource(srcY, y*w+x*2+1, bps, bigEndian);
      if (applyMathLuma)
      {
        valY1 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY1, inMax);
        valY2 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY2, inMax);
      }

      // Convert to 2 RGB values and save them
      int valR1, valR2, valG1, valG2, valB1, valB2;
      convertYUVToRGB8Bit(valY1, curUSample   , curVSample   , valR1, valG1, valB1, RGBConv, bps);
      convertYUVToRGB8Bit(valY2, interpolatedU, interpolatedV, valR2, valG2, valB2, RGBConv, bps);
      dst[(y*w+x*2)*3  ] = valR1;
      dst[(y*w+x*2)*3+1] = valG1;
      dst[(y*w+x*2)*3+2] = valB1;
      dst[(y*w+x*2)*3+3] = valR2;
      dst[(y*w+x*2)*3+4] = valG2;
      dst[(y*w+x*2)*3+5] = valB2;

      // The next one is now the current one
      curUSample = nextUSample;
      curVSample = nextVSample;
    }

    // For the last row, there is no next sample. Just reuse the current one again. No interpolation required either.

    // Get the 2 Y samples
    int valY1 = getValueFromSource(srcY, (y+1)*w-2, bps, bigEndian);
    int valY2 = getValueFromSource(srcY, (y+1)*w-1, bps, bigEndian);
    if (applyMathLuma)
    {
      valY1 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY1, inMax);
      valY2 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY2, inMax);
    }
    
    // Convert to 2 RGB values and save them
    int valR1, valR2, valG1, valG2, valB1, valB2;
    convertYUVToRGB8Bit(valY1, curUSample, curVSample, valR1, valG1, valB1, RGBConv, bps);
    convertYUVToRGB8Bit(valY2, curUSample, curVSample, valR2, valG2, valB2, RGBConv, bps);
    dst[((y+1)*w)*3-6] = valR1;
    dst[((y+1)*w)*3-5] = valG1;
    dst[((y+1)*w)*3-4] = valB1;
    dst[((y+1)*w)*3-3] = valR2;
    dst[((y+1)*w)*3-2] = valG2;
    dst[((y+1)*w)*3-1] = valB2;
  }
}

inline void YUVPlaneToRGB_440(const int w, const int h, const yuvMathParameters mathY, const yuvMathParameters mathC, 
                              const unsigned char * restrict srcY, const unsigned char * restrict srcU, const unsigned char * restrict srcV,
                              unsigned char * restrict dst, const int RGBConv[5], const int inMax, const InterpolationMode interpolation, const int bps, const bool bigEndian)
{
  const bool applyMathLuma = mathY.yuvMathRequired();
  const bool applyMathChroma = mathC.yuvMathRequired();
  // Vertical upsampling is required. Process two Y values at a time
  
  for (int x = 0; x < w; x++)
  {
    int curUSample = getValueFromSource(srcU, x, bps, bigEndian);
    int curVSample = getValueFromSource(srcV, x, bps, bigEndian);
    if (applyMathChroma)
    {
      curUSample = transformYUV(mathC.invert, mathC.scale, mathC.offset, curUSample, inMax);
      curVSample = transformYUV(mathC.invert, mathC.scale, mathC.offset, curVSample, inMax);
    }

    for (int y = 0; y < (h/2)-1; y++)
    {
      // Get the next U/V sample
      int nextUSample = getValueFromSource(srcU, y*w+x, bps, bigEndian);
      int nextVSample = getValueFromSource(srcV, y*w+x, bps, bigEndian);
      if (applyMathChroma)
      {
        nextUSample = transformYUV(mathC.invert, mathC.scale, mathC.offset, nextUSample, inMax);
        nextVSample = transformYUV(mathC.invert, mathC.scale, mathC.offset, nextVSample, inMax);
      }

      // From the current and the next U/V sample, interpolate the UV sample in between
      int interpolatedU = interpolateUVSample(interpolation, curUSample, nextUSample);
      int interpolatedV = interpolateUVSample(interpolation, curVSample, nextVSample);

      // Get the 2 Y samples
      int valY1 = getValueFromSource(srcY,     y*2*w+x, bps, bigEndian);
      int valY2 = getValueFromSource(srcY, (y*2+1)*w+x, bps, bigEndian);
      if (applyMathLuma)
      {
        valY1 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY1, inMax);
        valY2 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY2, inMax);
      }

      // Convert to 2 RGB values and save them
      int valR1, valR2, valG1, valG2, valB1, valB2;
      convertYUVToRGB8Bit(valY1, curUSample   , curVSample   , valR1, valG1, valB1, RGBConv, bps);
      convertYUVToRGB8Bit(valY2, interpolatedU, interpolatedV, valR2, valG2, valB2, RGBConv, bps);
      dst[(y*2*w+x)*3  ] = valR1;
      dst[(y*2*w+x)*3+1] = valG1;
      dst[(y*2*w+x)*3+2] = valB1;
      dst[((y*2+1)*w+x)*3  ] = valR2;
      dst[((y*2+1)*w+x)*3+1] = valG2;
      dst[((y*2+1)*w+x)*3+2] = valB2;

      // The next one is now the current one
      curUSample = nextUSample;
      curVSample = nextVSample;
    }

    // For the last column, there is no next sample. Just reuse the current one again. No interpolation required either.

    // Get the 2 Y samples
    int valY1 = getValueFromSource(srcY, (h-2)*w+x, bps, bigEndian);
    int valY2 = getValueFromSource(srcY, (h-1)*w+x, bps, bigEndian);
    if (applyMathLuma)
    {
      valY1 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY1, inMax);
      valY2 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY2, inMax);
    }

    // Convert to 2 RGB values and save them
    int valR1, valR2, valG1, valG2, valB1, valB2;
    convertYUVToRGB8Bit(valY1, curUSample, curVSample, valR1, valG1, valB1, RGBConv, bps);
    convertYUVToRGB8Bit(valY2, curUSample, curVSample, valR2, valG2, valB2, RGBConv, bps);
    dst[((h-2)*w+x)*3  ] = valR1;
    dst[((h-2)*w+x)*3+1] = valG1;
    dst[((h-2)*w+x)*3+2] = valB1;
    dst[((h-1)*w+x)*3  ] = valR2;
    dst[((h-1)*w+x)*3+1] = valG2;
    dst[((h-1)*w+x)*3+2] = valB2;
  }
}

inline void YUVPlaneToRGB_411(const int w, const int h, const yuvMathParameters mathY, const yuvMathParameters mathC, 
                              const unsigned char * restrict srcY, const unsigned char * restrict srcU, const unsigned char * restrict srcV,
                              unsigned char * restrict dst, const int RGBConv[5], const int inMax, const InterpolationMode interpolation, const int bps, const bool bigEndian)
{
  // Chroma: quarter horizontal resolution
  const bool applyMathLuma = mathY.yuvMathRequired();
  const bool applyMathChroma = mathC.yuvMathRequired();

  // Horizontal upsampling is required. Process four Y values at a time.
  for (int y = 0; y < h; y++)
  {
    int curUSample = getValueFromSource(srcU, y*w/4, bps, bigEndian);
    int curVSample = getValueFromSource(srcV, y*w/4, bps, bigEndian);
    if (applyMathChroma)
    {
      curUSample = transformYUV(mathC.invert, mathC.scale, mathC.offset, curUSample, inMax);
      curVSample = transformYUV(mathC.invert, mathC.scale, mathC.offset, curVSample, inMax);
    }

    for (int x = 0; x < (w/4)-1; x++)
    {
      // Get the next U/V sample
      int nextUSample = getValueFromSource(srcU, y*w/4+x+1, bps, bigEndian);
      int nextVSample = getValueFromSource(srcV, y*w/4+x+1, bps, bigEndian);
      if (applyMathChroma)
      {
        nextUSample = transformYUV(mathC.invert, mathC.scale, mathC.offset, nextUSample, inMax);
        nextVSample = transformYUV(mathC.invert, mathC.scale, mathC.offset, nextVSample, inMax);
      }

      // From the current and the next U/V sample, interpolate the UV sample in between
      int interpolatedU1 = interpolateUVSampleQ(interpolation, curUSample, nextUSample, 1);
      int interpolatedV1 = interpolateUVSampleQ(interpolation, curVSample, nextVSample, 1);
      int interpolatedU2 = interpolateUVSample(interpolation, curUSample, nextUSample);
      int interpolatedV2 = interpolateUVSample(interpolation, curVSample, nextVSample);
      int interpolatedU3 = interpolateUVSampleQ(interpolation, curUSample, nextUSample, 3);
      int interpolatedV3 = interpolateUVSampleQ(interpolation, curVSample, nextVSample, 3);

      // Get the 4 Y samples
      int valY1 = getValueFromSource(srcY, y*w+x*4,   bps, bigEndian);
      int valY2 = getValueFromSource(srcY, y*w+x*4+1, bps, bigEndian);
      int valY3 = getValueFromSource(srcY, y*w+x*4+2, bps, bigEndian);
      int valY4 = getValueFromSource(srcY, y*w+x*4+3, bps, bigEndian);
      if (applyMathLuma)
      {
        valY1 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY1, inMax);
        valY2 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY2, inMax);
        valY3 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY3, inMax);
        valY4 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY4, inMax);
      }

      // Convert to 4 RGB values and save them
      int valR, valG, valB;
      convertYUVToRGB8Bit(valY1, curUSample, curVSample, valR, valG, valB, RGBConv, bps);
      dst[(y*w+x*4)*3  ] = valR;
      dst[(y*w+x*4)*3+1] = valG;
      dst[(y*w+x*4)*3+2] = valB;
      convertYUVToRGB8Bit(valY2, interpolatedU1, interpolatedV1, valR, valG, valB, RGBConv, bps);
      dst[(y*w+x*4)*3+3] = valR;
      dst[(y*w+x*4)*3+4] = valG;
      dst[(y*w+x*4)*3+5] = valB;
      convertYUVToRGB8Bit(valY3, interpolatedU2, interpolatedV2, valR, valG, valB, RGBConv, bps);
      dst[(y*w+x*4)*3+6] = valR;
      dst[(y*w+x*4)*3+7] = valG;
      dst[(y*w+x*4)*3+8] = valB;
      convertYUVToRGB8Bit(valY4, interpolatedU3, interpolatedV3, valR, valG, valB, RGBConv, bps);
      dst[(y*w+x*4)*3+9] = valR;
      dst[(y*w+x*4)*3+10] = valG;
      dst[(y*w+x*4)*3+11] = valB;

      // The next one is now the current one
      curUSample = nextUSample;
      curVSample = nextVSample;
    }

    // For the last row, there is no next sample. Just reuse the current one again. No interpolation required either.

    // Get the 2 Y samples
    int valY1 = getValueFromSource(srcY, (y+1)*w-4, bps, bigEndian);
    int valY2 = getValueFromSource(srcY, (y+1)*w-3, bps, bigEndian);
    int valY3 = getValueFromSource(srcY, (y+1)*w-2, bps, bigEndian);
    int valY4 = getValueFromSource(srcY, (y+1)*w-1, bps, bigEndian);
    if (applyMathLuma)
    {
      valY1 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY1, inMax);
      valY2 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY2, inMax);
      valY3 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY3, inMax);
      valY4 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY4, inMax);
    }

    // Convert to 4 RGB values and save them
    int valR, valG, valB;
    convertYUVToRGB8Bit(valY1, curUSample, curVSample, valR, valG, valB, RGBConv, bps);
    dst[((y+1)*w)*3-12] = valR;
    dst[((y+1)*w)*3-11] = valG;
    dst[((y+1)*w)*3-10] = valB;
    convertYUVToRGB8Bit(valY2, curUSample, curVSample, valR, valG, valB, RGBConv, bps);
    dst[((y+1)*w)*3-9 ] = valR;
    dst[((y+1)*w)*3-8 ] = valG;
    dst[((y+1)*w)*3-7 ] = valB;
    convertYUVToRGB8Bit(valY3, curUSample, curVSample, valR, valG, valB, RGBConv, bps);
    dst[((y+1)*w)*3-6 ] = valR;
    dst[((y+1)*w)*3-5 ] = valG;
    dst[((y+1)*w)*3-4 ] = valB;
    convertYUVToRGB8Bit(valY4, curUSample, curVSample, valR, valG, valB, RGBConv, bps);
    dst[((y+1)*w)*3-3 ] = valR;
    dst[((y+1)*w)*3-2 ] = valG;
    dst[((y+1)*w)*3-1 ] = valB;
  }
}

inline void YUVPlaneToRGB_420(const int w, const int h, const yuvMathParameters mathY, const yuvMathParameters mathC, 
                              const unsigned char * restrict srcY, const unsigned char * restrict srcU, const unsigned char * restrict srcV,
                              unsigned char * restrict dst, const int RGBConv[5], const int inMax, const InterpolationMode interpolation, const int bps, const bool bigEndian)
{
  const bool applyMathLuma = mathY.yuvMathRequired();
  const bool applyMathChroma = mathC.yuvMathRequired();
  // Format is YUV 4:2:0. Horizontal and vertival upsampling is required. Process 4 Y positions at a time
  const int hh = h/2; // The half values
  const int wh = w/2;
  for (int y = 0; y < hh-1; y++)
  {
    // Get the current U/V samples for this y line and the next one (_NL)
    int curU    = getValueFromSource(srcU, y*wh, bps, bigEndian);
    int curV    = getValueFromSource(srcV, y*wh, bps, bigEndian);
    int curU_NL = getValueFromSource(srcU, (y+1)*wh, bps, bigEndian);
    int curV_NL = getValueFromSource(srcV, (y+1)*wh, bps, bigEndian);
    if (applyMathChroma)
    {
      curU    = transformYUV(mathC.invert, mathC.scale, mathC.offset, curU, inMax);
      curV    = transformYUV(mathC.invert, mathC.scale, mathC.offset, curV, inMax);
      curU_NL = transformYUV(mathC.invert, mathC.scale, mathC.offset, curU_NL, inMax);
      curV_NL = transformYUV(mathC.invert, mathC.scale, mathC.offset, curV_NL, inMax);
    }

    for (int x = 0; x < wh-1; x++)
    {
      // Get the next U/V sample for this line and the next one
      int nextU    = getValueFromSource(srcU, y*wh+x+1, bps, bigEndian);
      int nextV    = getValueFromSource(srcV, y*wh+x+1, bps, bigEndian);
      int nextU_NL = getValueFromSource(srcU, (y+1)*wh+x+1, bps, bigEndian);
      int nextV_NL = getValueFromSource(srcV, (y+1)*wh+x+1, bps, bigEndian);
      if (applyMathChroma)
      {
        nextU    = transformYUV(mathC.invert, mathC.scale, mathC.offset, nextU, inMax);
        nextV    = transformYUV(mathC.invert, mathC.scale, mathC.offset, nextV, inMax);
        nextU_NL = transformYUV(mathC.invert, mathC.scale, mathC.offset, nextU_NL, inMax);
        nextV_NL = transformYUV(mathC.invert, mathC.scale, mathC.offset, nextV_NL, inMax);
      }

      // From the current and the next U/V sample, interpolate the 3 UV samples in between
      int interpolatedU_Hor = interpolateUVSample(interpolation, curU, nextU);  // Horizontal interpolation
      int interpolatedV_Hor = interpolateUVSample(interpolation, curV, nextV);  
      int interpolatedU_Ver = interpolateUVSample(interpolation, curU, curU_NL);  // Vertical interpolation
      int interpolatedV_Ver = interpolateUVSample(interpolation, curV, curV_NL);  
      int interpolatedU_Bi  = interpolateUVSample2D(interpolation, curU, nextU, curU_NL, nextU_NL);   // 2D interpolation
      int interpolatedV_Bi  = interpolateUVSample2D(interpolation, curV, nextV, curV_NL, nextV_NL);   // 2D interpolation

      // Get the 4 Y samples
      int valY1 = getValueFromSource(srcY, (y*w+x)*2,   bps, bigEndian);
      int valY2 = getValueFromSource(srcY, (y*w+x)*2+1, bps, bigEndian);
      int valY3 = getValueFromSource(srcY, (y*2+1)*w+x*2,   bps, bigEndian);
      int valY4 = getValueFromSource(srcY, (y*2+1)*w+x*2+1, bps, bigEndian);
      if (applyMathLuma)
      {
        valY1 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY1, inMax);
        valY2 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY2, inMax);
        valY3 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY3, inMax);
        valY4 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY4, inMax);
      }

      // Convert to 4 RGB values and save them
      int valR1, valR2, valG1, valG2, valB1, valB2;
      convertYUVToRGB8Bit(valY1, curU             , curV             , valR1, valG1, valB1, RGBConv, bps);
      convertYUVToRGB8Bit(valY2, interpolatedU_Hor, interpolatedV_Hor, valR2, valG2, valB2, RGBConv, bps);
      dst[ (y*2   *w+x*2)*3  ] = valR1;
      dst[ (y*2   *w+x*2)*3+1] = valG1;
      dst[ (y*2   *w+x*2)*3+2] = valB1;
      dst[ (y*2   *w+x*2)*3+3] = valR2;
      dst[ (y*2   *w+x*2)*3+4] = valG2;
      dst[ (y*2   *w+x*2)*3+5] = valB2;
      convertYUVToRGB8Bit(valY3, interpolatedU_Ver, interpolatedV_Ver, valR1, valG1, valB1, RGBConv, bps);  // Second line
      convertYUVToRGB8Bit(valY4, interpolatedU_Bi , interpolatedV_Bi , valR2, valG2, valB2, RGBConv, bps);
      dst[((y*2+1)*w+x*2)*3  ] = valR1;
      dst[((y*2+1)*w+x*2)*3+1] = valG1;
      dst[((y*2+1)*w+x*2)*3+2] = valB1;
      dst[((y*2+1)*w+x*2)*3+3] = valR2;
      dst[((y*2+1)*w+x*2)*3+4] = valG2;
      dst[((y*2+1)*w+x*2)*3+5] = valB2;

      // The next one is now the current one
      curU = nextU;
      curV = nextV;
      curU_NL = nextU_NL;
      curV_NL = nextV_NL;
    }

    // For the last x value (the right border), there is no next value. Just sample and hold. Only vertival interpolation is required.
    int interpolatedU_Ver = interpolateUVSample(interpolation, curU, curU_NL);  // Vertical interpolation
    int interpolatedV_Ver = interpolateUVSample(interpolation, curV, curV_NL);

    // Get the 4 Y samples
    int valY1 = getValueFromSource(srcY, (y*2+1)*w-2, bps, bigEndian);
    int valY2 = getValueFromSource(srcY, (y*2+1)*w-1, bps, bigEndian);
    int valY3 = getValueFromSource(srcY, (y*2+2)*w-2, bps, bigEndian);
    int valY4 = getValueFromSource(srcY, (y*2+2)*w-1, bps, bigEndian);
    if (applyMathLuma)
    {
      valY1 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY1, inMax);
      valY2 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY2, inMax);
      valY3 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY3, inMax);
      valY4 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY4, inMax);
    }

    // Convert to 4 RGB values and save them
    int valR1, valR2, valG1, valG2, valB1, valB2;
    convertYUVToRGB8Bit(valY1, curU, curV, valR1, valG1, valB1, RGBConv, bps);
    convertYUVToRGB8Bit(valY2, curU, curV, valR2, valG2, valB2, RGBConv, bps);
    dst[((y*2+1)*w)*3-6] = valR1;
    dst[((y*2+1)*w)*3-5] = valG1;
    dst[((y*2+1)*w)*3-4] = valB1;
    dst[((y*2+1)*w)*3-3] = valR2;
    dst[((y*2+1)*w)*3-2] = valG2;
    dst[((y*2+1)*w)*3-1] = valB2;
    convertYUVToRGB8Bit(valY3, interpolatedU_Ver, interpolatedV_Ver, valR1, valG1, valB1, RGBConv, bps);  // Second line
    convertYUVToRGB8Bit(valY4, interpolatedU_Ver, interpolatedV_Ver, valR2, valG2, valB2, RGBConv, bps);
    dst[((y*2+2)*w)*3-6] = valR1;
    dst[((y*2+2)*w)*3-5] = valG1;
    dst[((y*2+2)*w)*3-4] = valB1;
    dst[((y*2+2)*w)*3-3] = valR2;
    dst[((y*2+2)*w)*3-2] = valG2;
    dst[((y*2+2)*w)*3-1] = valB2;
  }

  // At the last Y line (the bottom line) a similar scenario occurs. There is no next Y line. Just sample and hold. Only horizontal interpolation is required.

  // Get the current U/V samples for this y line
  const int y = hh-1;  // Just process the last y line
  const int y2 = (hh-1)*2;

  // Get 2 chroma samples from this line
  int curU = getValueFromSource(srcU, y*wh, bps, bigEndian);
  int curV = getValueFromSource(srcV, y*wh, bps, bigEndian);
  if (applyMathChroma)
  {
    curU = transformYUV(mathC.invert, mathC.scale, mathC.offset, curU, inMax);
    curV = transformYUV(mathC.invert, mathC.scale, mathC.offset, curV, inMax);
  }

  for (int x = 0; x < (w/2)-1; x++)
  {
    // Get the next U/V sample for this line and the next one
    int nextU = getValueFromSource(srcU, y*wh+x+1, bps, bigEndian);
    int nextV = getValueFromSource(srcV, y*wh+x+1, bps, bigEndian);
    if (applyMathChroma)
    {
      nextU = transformYUV(mathC.invert, mathC.scale, mathC.offset, nextU, inMax);
      nextV = transformYUV(mathC.invert, mathC.scale, mathC.offset, nextV, inMax);
    }

    // From the current and the next U/V sample, interpolate the 3 UV samples in between
    int interpolatedU_Hor = interpolateUVSample(interpolation, curU, nextU);  // Horizontal interpolation
    int interpolatedV_Hor = interpolateUVSample(interpolation, curV, nextV);

    // Get the 4 Y samples
    int valY1 = getValueFromSource(srcY, (y*w+x)*2,      bps, bigEndian);
    int valY2 = getValueFromSource(srcY, (y*w+x)*2+1,    bps, bigEndian);
    int valY3 = getValueFromSource(srcY, (y2+1)*w+x*2,   bps, bigEndian);
    int valY4 = getValueFromSource(srcY, (y2+1)*w+x*2+1, bps, bigEndian);
    if (applyMathLuma)
    {
      valY1 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY1, inMax);
      valY2 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY2, inMax);
      valY3 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY3, inMax);
      valY4 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY4, inMax);
    }

    // Convert to 4 RGB values and save them
    int valR1, valR2, valG1, valG2, valB1, valB2;
    convertYUVToRGB8Bit(valY1, curU             , curV             , valR1, valG1, valB1, RGBConv, bps);
    convertYUVToRGB8Bit(valY2, interpolatedU_Hor, interpolatedV_Hor, valR2, valG2, valB2, RGBConv, bps);
    dst[ (y2   *w+x*2)*3  ] = valR1;
    dst[ (y2   *w+x*2)*3+1] = valG1;
    dst[ (y2   *w+x*2)*3+2] = valB1;
    dst[ (y2   *w+x*2)*3+3] = valR2;
    dst[ (y2   *w+x*2)*3+4] = valG2;
    dst[ (y2   *w+x*2)*3+5] = valB2;
    convertYUVToRGB8Bit(valY3, curU             , curV             , valR1, valG1, valB1, RGBConv, bps);  // Second line
    convertYUVToRGB8Bit(valY4, interpolatedU_Hor, interpolatedV_Hor, valR2, valG2, valB2, RGBConv, bps);
    dst[((y2+1)*w+x*2)*3  ] = valR1;
    dst[((y2+1)*w+x*2)*3+1] = valG1;
    dst[((y2+1)*w+x*2)*3+2] = valB1;
    dst[((y2+1)*w+x*2)*3+3] = valR2;
    dst[((y2+1)*w+x*2)*3+4] = valG2;
    dst[((y2+1)*w+x*2)*3+5] = valB2;

    // The next one is now the current one
    curU = nextU;
    curV = nextV;
  }

  // For the last x value in the last y row (the right bottom), there is no next value in neither direction.
  // Just sample and hold. No interpolation is required.

  // Get the 4 Y samples
  int valY1 = getValueFromSource(srcY, (y2+1)*w-2, bps, bigEndian);
  int valY2 = getValueFromSource(srcY, (y2+1)*w-1, bps, bigEndian);
  int valY3 = getValueFromSource(srcY, (y2+2)*w-2, bps, bigEndian);
  int valY4 = getValueFromSource(srcY, (y2+2)*w-1, bps, bigEndian);
  if (applyMathLuma)
  {
    valY1 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY1, inMax);
    valY2 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY2, inMax);
    valY3 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY3, inMax);
    valY4 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY4, inMax);
  }

  // Convert to 4 RGB values and save them
  int valR1, valR2, valG1, valG2, valB1, valB2;
  convertYUVToRGB8Bit(valY1, curU, curV, valR1, valG1, valB1, RGBConv, bps);
  convertYUVToRGB8Bit(valY2, curU, curV, valR2, valG2, valB2, RGBConv, bps);
  dst[((y2+1)*w)*3-6] = valR1;
  dst[((y2+1)*w)*3-5] = valG1;
  dst[((y2+1)*w)*3-4] = valB1;
  dst[((y2+1)*w)*3-3] = valR2;
  dst[((y2+1)*w)*3-2] = valG2;
  dst[((y2+1)*w)*3-1] = valB2;
  convertYUVToRGB8Bit(valY3, curU, curV, valR1, valG1, valB1, RGBConv, bps);  // Second line
  convertYUVToRGB8Bit(valY4, curU, curV, valR2, valG2, valB2, RGBConv, bps);
  dst[((y2+2)*w)*3-6] = valR1;
  dst[((y2+2)*w)*3-5] = valG1;
  dst[((y2+2)*w)*3-4] = valB1;
  dst[((y2+2)*w)*3-3] = valR2;
  dst[((y2+2)*w)*3-2] = valG2;
  dst[((y2+2)*w)*3-1] = valB2;
}

inline void YUVPlaneToRGB_410(const int w, const int h, const yuvMathParameters mathY, const yuvMathParameters mathC, 
                              const unsigned char * restrict srcY, const unsigned char * restrict srcU, const unsigned char * restrict srcV,
                              unsigned char * restrict dst, const int RGBConv[5], const int inMax, const InterpolationMode interpolation, const int bps, const bool bigEndian)
{
  const bool applyMathLuma = mathY.yuvMathRequired();
  const bool applyMathChroma = mathC.yuvMathRequired();
  // Format is YUV 4:1:0. Horizontal and vertival upsampling is required. Process 4 Y positions of 2 lines at a time
  // Horizontal subsampling by 4, vertical subsampling by 2
  const int hq = h/4; // The quarter values
  const int wq = w/4;

  for (int y = 0; y < hq; y++)
  {
    // Get the current U/V samples for this y line and the next one (_NL)
    int curU    = getValueFromSource(srcU, y*wq, bps, bigEndian);
    int curV    = getValueFromSource(srcV, y*wq, bps, bigEndian);
    int curU_NL = (y < hq-1) ? getValueFromSource(srcU, (y+1)*wq, bps, bigEndian) : curU;
    int curV_NL = (y < hq-1) ? getValueFromSource(srcV, (y+1)*wq, bps, bigEndian) : curV;
    if (applyMathChroma)
    {
      curU    = transformYUV(mathC.invert, mathC.scale, mathC.offset, curU, inMax);
      curV    = transformYUV(mathC.invert, mathC.scale, mathC.offset, curV, inMax);
      curU_NL = transformYUV(mathC.invert, mathC.scale, mathC.offset, curU_NL, inMax);
      curV_NL = transformYUV(mathC.invert, mathC.scale, mathC.offset, curV_NL, inMax);
    }

    for (int x = 0; x < wq; x++)
    {
      // We process 4*4 values per U/V value

      // Get the next U/V sample for this line and the next one
      int nextU    = (x < wq-1) ? getValueFromSource(srcU, y*wq+x+1, bps, bigEndian) : curU;
      int nextV    = (x < wq-1) ? getValueFromSource(srcV, y*wq+x+1, bps, bigEndian) : curV;
      int nextU_NL = (x < wq-1) ? getValueFromSource(srcU, (y+1)*wq+x+1, bps, bigEndian) : curU_NL;
      int nextV_NL = (x < wq-1) ? getValueFromSource(srcV, (y+1)*wq+x+1, bps, bigEndian) : curV_NL;
      if (applyMathChroma)
      {
        nextU    = transformYUV(mathC.invert, mathC.scale, mathC.offset, nextU, inMax);
        nextV    = transformYUV(mathC.invert, mathC.scale, mathC.offset, nextV, inMax);
        nextU_NL = transformYUV(mathC.invert, mathC.scale, mathC.offset, nextU_NL, inMax);
        nextV_NL = transformYUV(mathC.invert, mathC.scale, mathC.offset, nextV_NL, inMax);
      }

      // Now we interpolate and set the RGB values for the 4x4 pixels
      for (int yo = 0; yo < 4; yo++)
      {
        // Interpolate vertically
        int curU_INT = interpolateUVSampleQ(interpolation, curU, curU_NL, yo);
        int curV_INT = interpolateUVSampleQ(interpolation, curV, curV_NL, yo);
        int nextU_INT = interpolateUVSampleQ(interpolation, nextU, nextU_NL, yo);
        int nextV_INT = interpolateUVSampleQ(interpolation, nextV, nextV_NL, yo);

        for (int xo = 0; xo < 4; xo++)
        {
          // Interpolate horizontally
          int U = interpolateUVSampleQ(interpolation, curU_INT, nextU_INT, xo);
          int V = interpolateUVSampleQ(interpolation, curV_INT, nextV_INT, xo);
          // Get the Y sample
          int Y = getValueFromSource(srcY, (y*4+yo)*w+x*4+xo, bps, bigEndian);
          if (applyMathLuma)
            Y = transformYUV(mathY.invert, mathY.scale, mathY.offset, Y, inMax);

          // Convert to RGB and save
          int R, G, B;
          convertYUVToRGB8Bit(Y, U, V, R, G, B, RGBConv, bps);
          dst[((y*4+yo)*w+x*4+xo)*3  ] = R;
          dst[((y*4+yo)*w+x*4+xo)*3+1] = G;
          dst[((y*4+yo)*w+x*4+xo)*3+2] = B;
        }
      }

      curU = nextU;
      curV = nextV;
      curU_NL = nextU_NL;
      curV_NL = nextV_NL;
    }
  }
}

bool videoHandlerYUV::convertYUVPlanarToRGB(QByteArray &sourceBuffer, QByteArray &targetBuffer, const QSize curFrameSize) const
{
  // These are constant for the runtime of this function. This way, the compiler can optimize the
  // hell out of this function.
  const yuvPixelFormat format = srcPixelFormat;
  const InterpolationMode interpolation = interpolationMode;
  const ComponentDisplayMode component = componentDisplayMode;
  const YUVCColorConversionType conversion = yuvColorConversionType;
  const int w = curFrameSize.width();
  const int h = curFrameSize.height();

  // Do we have to apply yuv math?
  const yuvMathParameters mathY = mathParameters[Luma];
  const yuvMathParameters mathC = mathParameters[Chroma];
  const bool applyMathLuma   = mathY.yuvMathRequired();
  const bool applyMathChroma = mathC.yuvMathRequired();

  const int bps = format.bitsPerSample;
  const int yOffset = 16<<(bps-8);
  const int cZero = 128<<(bps-8);
  const int inputMax = (1<<bps)-1;

  // Check some things
  if (bps < 8 || bps > 16)
    // Not yet supported ...
    return false;
  if (w % format.getSubsamplingHor() != 0)
    return false;
  if (h % format.getSubsamplingVer() != 0)
    return false;

  // Make sure that the target buffer is big enough. We always output 8 bit RGB, so the output size only depends on the frame size:
  int targetBufferSize = w * h * 3;
  if (targetBuffer.size() != targetBufferSize)
    targetBuffer.resize(targetBufferSize);

  // The luma component has full resolution. The size of each chroma components depends on the subsampling.
  const int componentSizeLuma = (w * h);
  const int componentSizeChroma = (w / format.getSubsamplingHor()) * (h / format.getSubsamplingVer());

  // How many bytes are in each component?
  const int nrBytesLumaPlane = (bps > 8) ? componentSizeLuma * 2 : componentSizeLuma;
  const int nrBytesChromaPlane = (bps > 8) ? componentSizeChroma * 2 : componentSizeChroma;

  // A pointer to the output
  unsigned char * restrict dst = (unsigned char*)targetBuffer.data();

  if (component != DisplayAll)
  {
    // We only display one of the color components (possibly with yuv math).
    if (component == DisplayY)
    {
      // Luma only. The chroma subsampling does not matter.
      const unsigned char * restrict srcY = (unsigned char*)sourceBuffer.data();
      YUVPlaneToRGBMonochrome_444(componentSizeLuma, mathY, srcY, dst, inputMax, bps, format.bigEndian);
    }
    else
    {
      // Display only the U or V component
      bool firstComponent = (((format.planeOrder == Order_YUV || format.planeOrder == Order_YUVA) && component == DisplayCb) ||
                             ((format.planeOrder == Order_YVU || format.planeOrder == Order_YVUA) && component == DisplayCr)    );

      const unsigned char * restrict srcC = (unsigned char*)sourceBuffer.data() + nrBytesLumaPlane + (nrBytesChromaPlane * (firstComponent ? 0 : 1));
      if (format.subsampling == YUV_444)
        YUVPlaneToRGBMonochrome_444(componentSizeChroma, mathC, srcC, dst, inputMax, bps, format.bigEndian);
      else if (format.subsampling == YUV_422)
        YUVPlaneToRGBMonochrome_422(componentSizeChroma, mathC, srcC, dst, inputMax, bps, format.bigEndian);
      else if (format.subsampling == YUV_420)
        YUVPlaneToRGBMonochrome_420(w, h, mathC, srcC, dst, inputMax, bps, format.bigEndian);
      else if (format.subsampling == YUV_440)
        YUVPlaneToRGBMonochrome_440(w, h, mathC, srcC, dst, inputMax, bps, format.bigEndian);
      else if (format.subsampling == YUV_410)
        YUVPlaneToRGBMonochrome_410(w, h, mathC, srcC, dst, inputMax, bps, format.bigEndian);
      else if (format.subsampling == YUV_411)
        YUVPlaneToRGBMonochrome_411(componentSizeChroma, mathC, srcC, dst, inputMax, bps, format.bigEndian);
      else
        return false;

    }
  }
  else
  {
    // We are displaying all components, so we have to perform conversion to RGB (possibly including interpolation and yuv math)

    // Get/set the parameters used for YUV -> RGB conversion
    const int RGBConv[5] = { 76309,                                                                                                                 //yMult
      (yuvColorConversionType == YUVC601ColorConversionType) ? 104597 : (yuvColorConversionType == YUVC2020ColorConversionType) ? 110013 : 117489,  //rvMult
      (yuvColorConversionType == YUVC601ColorConversionType) ? -25675 : (yuvColorConversionType == YUVC2020ColorConversionType) ? -12276 : -13975,  //guMult
      (yuvColorConversionType == YUVC601ColorConversionType) ? -53279 : (yuvColorConversionType == YUVC2020ColorConversionType) ? -42626 : -34925,  //gvMult
      (yuvColorConversionType == YUVC601ColorConversionType) ? 132201 : (yuvColorConversionType == YUVC2020ColorConversionType) ? 140363 : 138438   //buMult
    };
    
    // Get the pointers to the source planes (8 bit per sample)
    const unsigned char * restrict srcY = (unsigned char*)sourceBuffer.data();
    const unsigned char * restrict srcU = (format.planeOrder == Order_YUV || format.planeOrder == Order_YUVA) ? srcY + nrBytesLumaPlane : srcY + nrBytesLumaPlane + nrBytesChromaPlane;
    const unsigned char * restrict srcV = (format.planeOrder == Order_YUV || format.planeOrder == Order_YUVA) ? srcY + nrBytesLumaPlane + nrBytesChromaPlane: srcY + nrBytesLumaPlane;
    
    if (format.subsampling == YUV_444)
      YUVPlaneToRGB_444(componentSizeLuma, mathY, mathC, srcY, srcU, srcV, dst, RGBConv, inputMax, bps, format.bigEndian);
    else if (format.subsampling == YUV_422)
      YUVPlaneToRGB_422(w, h, mathY, mathC, srcY, srcU, srcV, dst, RGBConv, inputMax, interpolation, bps, format.bigEndian);
    else if (format.subsampling == YUV_420)
      YUVPlaneToRGB_420(w, h, mathY, mathC, srcY, srcU, srcV, dst, RGBConv, inputMax, interpolation, bps, format.bigEndian);
    else if (format.subsampling == YUV_440)
      YUVPlaneToRGB_440(w, h, mathY, mathC, srcY, srcU, srcV, dst, RGBConv, inputMax, interpolation, bps, format.bigEndian);
    else if (format.subsampling == YUV_410)
      YUVPlaneToRGB_410(w, h, mathY, mathC, srcY, srcU, srcV, dst, RGBConv, inputMax, interpolation, bps, format.bigEndian);
    else if (format.subsampling == YUV_411)
      YUVPlaneToRGB_411(w, h, mathY, mathC, srcY, srcU, srcV, dst, RGBConv, inputMax, interpolation, bps, format.bigEndian);
    else if (format.subsampling == YUV_400)
      YUVPlaneToRGBMonochrome_444(componentSizeLuma, mathY, srcY, dst, inputMax, bps, format.bigEndian);
    else
      return false;
  
  }

  return true;
}

// Convert the given raw YUV data in sourceBuffer (using srcPixelFormat) to pixmap (RGB-888), using the
// buffer tmpRGBBuffer for intermediate RGB values.
void videoHandlerYUV::convertYUVToPixmap(QByteArray sourceBuffer, QPixmap &outputPixmap, QByteArray &tmpRGBBuffer)
{
  DEBUG_YUV( "videoHandlerYUV::convertYUVToPixmap" );
  
  // This is the frame size that we will use for this conversion job. If the frame size changes
  // while this function is running, we don't care.
  const QSize curFrameSize = frameSize;

//  if (srcPixelFormat == "4:2:0 Y'CbCr 8-bit planar" && !yuvMathRequired() && interpolationMode == NearestNeighborInterpolation &&
//      frameSize.width() % 2 == 0 && frameSize.height() % 2 == 0)
//      // TODO: For YUV4:2:0, an un-even width or height does not make a lot of sense. Or what should be done in this case?
//      // Is there even a definition for this? The correct is probably to not allow this and enforce divisibility by 2.
//  {
//    // directly convert from 420 to RGB
//    convertYUV420ToRGB(sourceBuffer, tmpRGBBuffer);
//  }
//  else
//  {
//    // First, convert the buffer to YUV 444
//    convert2YUV444(sourceBuffer, tmpYUV444Buffer);
//
//    // Apply transformations to the YUV components (if any are set)
//    // TODO: Shouldn't this be done before the conversion to 444?
//    applyYUVTransformation( tmpYUV444Buffer );
//
//    // Convert to RGB888
//    convertYUV4442RGB(tmpYUV444Buffer, tmpRGBBuffer);
//  }
//

  // Convert the source to RGB
  bool convOK = true;
  if (srcPixelFormat.planar)
    convOK = convertYUVPlanarToRGB(sourceBuffer, tmpRGBBuffer, curFrameSize);
  else
  {
    // Convert to a planar format first
    QByteArray tmpPlanarYUVSource;
    convOK &= convertYUVPackedToPlanar(sourceBuffer, tmpPlanarYUVSource, curFrameSize);

    if (convOK)
      convOK &= convertYUVPlanarToRGB(tmpPlanarYUVSource, tmpRGBBuffer, curFrameSize);
  }
  
  if (convOK)
  {
    // Convert the image in tmpRGBBuffer to a QPixmap using a QImage intermediate.
    // TODO: Isn't there a faster way to do this? Maybe load a pixmap from "BMP"-like data?
#if SSE_CONVERSION_420_ALT
    // RGB32 => 0xffRRGGBB
    QImage tmpImage((unsigned char*)tmpRGBBuffer.data(), curFrameSize.width(), curFrameSize.height(), QImage::Format_RGB32);
#else
    QImage tmpImage((unsigned char*)tmpRGBBuffer.data(), curFrameSize.width(), curFrameSize.height(), QImage::Format_RGB888);
#endif
  
    // Set the videoHanlder pixmap and image so the videoHandler can draw the item
    outputPixmap.convertFromImage(tmpImage);
  }
  else
    outputPixmap = QPixmap();
}

void videoHandlerYUV::getPixelValue(QPoint pixelPos, int frameIdx, unsigned int &Y, unsigned int &U, unsigned int &V)
{
  // Update the raw YUV data if necessary
  loadRawYUVData(frameIdx);

  const yuvPixelFormat format = srcPixelFormat;

  // The luma component has full resolution. The size of each chroma components depends on the subsampling.
  const int w = frameSize.width();
  const int h = frameSize.height();
  const int componentSizeLuma = (w * h);
  const int componentSizeChroma = (w / format.getSubsamplingHor()) * (h / format.getSubsamplingVer());

  // How many bytes are in each component?
  const int nrBytesLumaPlane = (format.bitsPerSample > 8) ? componentSizeLuma * 2 : componentSizeLuma;
  const int nrBytesChromaPlane = (format.bitsPerSample > 8) ? componentSizeChroma * 2 : componentSizeChroma;

  
  const unsigned char * restrict srcY = (unsigned char*)currentFrameRawYUVData.data();
  const unsigned char * restrict srcU = (format.planeOrder == Order_YUV || format.planeOrder == Order_YUVA) ? srcY + nrBytesLumaPlane : srcY + nrBytesLumaPlane + nrBytesChromaPlane;
  const unsigned char * restrict srcV = (format.planeOrder == Order_YUV || format.planeOrder == Order_YUVA) ? srcY + nrBytesLumaPlane + nrBytesChromaPlane: srcY + nrBytesLumaPlane;

  // Get the YUV data from the currentFrameRawYUVData
  const unsigned int offsetCoordinateY  = w * pixelPos.y() + pixelPos.x();
  const unsigned int offsetCoordinateUV = (w / srcPixelFormat.getSubsamplingHor() * (pixelPos.y() / srcPixelFormat.getSubsamplingVer())) + pixelPos.x() / srcPixelFormat.getSubsamplingHor();

  Y = getValueFromSource(srcY, offsetCoordinateY,  format.bitsPerSample, format.bigEndian);
  U = getValueFromSource(srcU, offsetCoordinateUV, format.bitsPerSample, format.bigEndian);
  V = getValueFromSource(srcV, offsetCoordinateUV, format.bitsPerSample, format.bigEndian);
}

//// Convert 8-bit YUV 4:2:0 to RGB888 using NearestNeighborInterpolation
//#if SSE_CONVERSION
//void videoHandlerYUV::convertYUV420ToRGB(byteArrayAligned &sourceBuffer, byteArrayAligned &targetBuffer)
//#else
//void videoHandlerYUV::convertYUV420ToRGB(QByteArray &sourceBuffer, QByteArray &targetBuffer, QSize size)
//#endif
//{
//  int frameWidth, frameHeight;
//  if (size.isValid())
//  {
//    frameWidth = size.width();
//    frameHeight = size.height();
//  }
//  else
//  {
//    frameWidth = frameSize.width();
//    frameHeight = frameSize.height();
//  }
//
//  // Round down the width and height to even values. Uneven values are not
//  // possible for a 4:2:0 format.
//  if (frameWidth % 2 != 0)
//    frameWidth -= 1;
//  if (frameHeight % 2 != 0)
//    frameHeight -= 1;
//
//  int componentLenghtY  = frameWidth * frameHeight;
//  int componentLengthUV = componentLenghtY >> 2;
//  Q_ASSERT( sourceBuffer.size() >= componentLenghtY + componentLengthUV+ componentLengthUV ); // YUV 420 must be (at least) 1.5*Y-area
//
//  // Resize target buffer if necessary
//#if SSE_CONVERSION_420_ALT
//  int targetBufferSize = frameWidth * frameHeight * 4;
//#else
//  int targetBufferSize = frameWidth * frameHeight * 3;
//#endif
//  if( targetBuffer.size() != targetBufferSize)
//    targetBuffer.resize(targetBufferSize);
//
//#if SSE_CONVERSION_420_ALT
//  quint8 *srcYRaw = (quint8*) sourceBuffer.data();
//  quint8 *srcURaw = srcYRaw + componentLenghtY;
//  quint8 *srcVRaw = srcURaw + componentLengthUV;
//
//  quint8 *dstBuffer = (quint8*)targetBuffer.data();
//  quint32 dstBufferStride = frameWidth*4;
//
//  yuv420_to_argb8888(srcYRaw,srcURaw,srcVRaw,frameWidth,frameWidth>>1,frameWidth,frameHeight,dstBuffer,dstBufferStride);
//  return;
//#endif
//
//#if SSE_CONVERSION
//  // Try to use SSE. If this fails use conventional algorithm
//
//  if (frameWidth % 32 == 0 && frameHeight % 2 == 0)
//  {
//    // We can use 16byte aligned read/write operations
//
//    quint8 *srcY = (quint8*) sourceBuffer.data();
//    quint8 *srcU = srcY + componentLenghtY;
//    quint8 *srcV = srcU + componentLengthUV;
//
//    __m128i yMult  = _mm_set_epi16(75, 75, 75, 75, 75, 75, 75, 75);
//    __m128i ySub   = _mm_set_epi16(16, 16, 16, 16, 16, 16, 16, 16);
//    __m128i ugMult = _mm_set_epi16(25, 25, 25, 25, 25, 25, 25, 25);
//    //__m128i sub16  = _mm_set_epi8(16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16);
//    __m128i sub128 = _mm_set_epi8(128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128);
//
//    //__m128i test = _mm_set_epi8(128, 0, 1, 2, 3, 245, 254, 255, 128, 128, 128, 128, 128, 128, 128, 128);
//
//    __m128i y, u, v, uMult, vMult;
//    __m128i RGBOut0, RGBOut1, RGBOut2;
//    __m128i tmp;
//
//    for (int yh=0; yh < frameHeight / 2; yh++)
//    {
//      for (int x=0; x < frameWidth / 32; x+=32)
//      {
//        // Load 16 bytes U/V
//        u  = _mm_load_si128((__m128i *) &srcU[x / 2]);
//        v  = _mm_load_si128((__m128i *) &srcV[x / 2]);
//        // Subtract 128 from each U/V value (16 values)
//        u = _mm_sub_epi8(u, sub128);
//        v = _mm_sub_epi8(v, sub128);
//
//        // Load 16 bytes Y from this line and the next one
//        y = _mm_load_si128((__m128i *) &srcY[x]);
//
//        // Get the lower 8 (8bit signed) Y values and put them into a 16bit register
//        tmp = _mm_srai_epi16(_mm_unpacklo_epi8(y, y), 8);
//        // Subtract 16 and multiply by 75
//        tmp = _mm_sub_epi16(tmp, ySub);
//        tmp = _mm_mullo_epi16(tmp, yMult);
//
//        // Now to add them to the 16 bit RGB output values
//        RGBOut0 = _mm_shuffle_epi32(tmp, _MM_SHUFFLE(1, 0, 1, 0));
//        RGBOut0 = _mm_shufflelo_epi16(RGBOut0, _MM_SHUFFLE(1, 0, 0, 0));
//        RGBOut0 = _mm_shufflehi_epi16(RGBOut0, _MM_SHUFFLE(2, 2, 1, 1));
//
//        RGBOut1 = _mm_shuffle_epi32(tmp, _MM_SHUFFLE(2, 1, 2, 1));
//        RGBOut1 = _mm_shufflelo_epi16(RGBOut1, _MM_SHUFFLE(1, 1, 1, 0));
//        RGBOut1 = _mm_shufflehi_epi16(RGBOut1, _MM_SHUFFLE(3, 2, 2, 2));
//
//        RGBOut2 = _mm_shuffle_epi32(tmp, _MM_SHUFFLE(3, 2, 3, 2));
//        RGBOut2 = _mm_shufflelo_epi16(RGBOut2, _MM_SHUFFLE(2, 2, 1, 1));
//        RGBOut2 = _mm_shufflehi_epi16(RGBOut2, _MM_SHUFFLE(3, 3, 3, 2));
//
//        //y2 = _mm_load_si128((__m128i *) &srcY[x + 16]);
//
//        // --- Start with the left 8 values from U/V
//
//        // Get the lower 8 (8bit signed) U/V values and put them into a 16bit register
//        uMult = _mm_srai_epi16(_mm_unpacklo_epi8(u, u), 8);
//        vMult = _mm_srai_epi16(_mm_unpacklo_epi8(v, v), 8);
//
//        // Multiply
//
//
//        /*y3 = _mm_load_si128((__m128i *) &srcY[x + frameWidth]);
//        y4 = _mm_load_si128((__m128i *) &srcY[x + frameWidth + 16]);*/
//      }
//    }
//
//
//
//    return;
//  }
//#endif
//
//  // Perform software based 420 to RGB conversion
//  static unsigned char clp_buf[384+256+384];
//  static unsigned char *clip_buf = clp_buf+384;
//  static bool clp_buf_initialized = false;
//
//  if (!clp_buf_initialized)
//  {
//    // Initialize clipping table. Because of the static bool, this will only be called once.
//    memset(clp_buf, 0, 384);
//    int i;
//    for (i = 0; i < 256; i++)
//      clp_buf[384+i] = i;
//    memset(clp_buf+384+256, 255, 384);
//    clp_buf_initialized = true;
//  }
//
//  const int yOffset = 16;
//  const int cZero = 128;
//  //const int rgbMax = (1<<8)-1;
//  int yMult, rvMult, guMult, gvMult, buMult;
//
//  unsigned char *dst = (unsigned char*)targetBuffer.data();
//  switch (yuvColorConversionType)
//  {
//    case YUVC601ColorConversionType:
//      yMult =   76309;
//      rvMult = 104597;
//      guMult = -25675;
//      gvMult = -53279;
//      buMult = 132201;
//      break;
//    case YUVC2020ColorConversionType:
//      yMult =   76309;
//      rvMult = 110013;
//      guMult = -12276;
//      gvMult = -42626;
//      buMult = 140363;
//      break;
//    case YUVC709ColorConversionType:
//    default:
//      yMult =   76309;
//      rvMult = 117489;
//      guMult = -13975;
//      gvMult = -34925;
//      buMult = 138438;
//      break;
//  }
//  const unsigned char * restrict srcY = (unsigned char*)sourceBuffer.data();
//  const unsigned char * restrict srcU = srcY + componentLenghtY;
//  const unsigned char * restrict srcV = srcU + componentLengthUV;
//  unsigned char * restrict dstMem = dst;
//
//  int yh;
//#if __MINGW32__ || __GNUC__
//#pragma omp parallel for default(none) private(yh) shared(srcY,srcU,srcV,dstMem,yMult,rvMult,guMult,gvMult,buMult,clip_buf,frameWidth,frameHeight)// num_threads(2)
//#else
//#pragma omp parallel for default(none) private(yh) shared(srcY,srcU,srcV,dstMem,yMult,rvMult,guMult,gvMult,buMult,clip_buf,frameWidth,frameHeight)// num_threads(2)
//#endif
//  for (yh=0; yh < frameHeight / 2; yh++)
//  {
//    // Process two lines at once, always 4 RGB values at a time (they have the same U/V components)
//
//    int dstAddr1 = yh * 2 * frameWidth * 3;         // The RGB output adress of line yh*2
//    int dstAddr2 = (yh * 2 + 1) * frameWidth * 3;   // The RGB output adress of line yh*2+1
//    int srcAddrY1 = yh * 2 * frameWidth;            // The Y source address of line yh*2
//    int srcAddrY2 = (yh * 2 + 1) * frameWidth;      // The Y source address of line yh*2+1
//    int srcAddrUV = yh * frameWidth / 2;            // The UV source address of both lines (UV are identical)
//
//    for (int xh=0, x=0; xh < frameWidth / 2; xh++, x+=2)
//    {
//      // Process four pixels (the ones for which U/V are valid
//
//      // Load UV and premultiply
//      const int U_tmp_G = ((int)srcU[srcAddrUV + xh] - cZero) * guMult;
//      const int U_tmp_B = ((int)srcU[srcAddrUV + xh] - cZero) * buMult;
//      const int V_tmp_R = ((int)srcV[srcAddrUV + xh] - cZero) * rvMult;
//      const int V_tmp_G = ((int)srcV[srcAddrUV + xh] - cZero) * gvMult;
//
//      // Pixel top left
//      {
//        const int Y_tmp = ((int)srcY[srcAddrY1 + x] - yOffset) * yMult;
//
//        const int R_tmp = (Y_tmp           + V_tmp_R ) >> 16;
//        const int G_tmp = (Y_tmp + U_tmp_G + V_tmp_G ) >> 16;
//        const int B_tmp = (Y_tmp + U_tmp_B           ) >> 16;
//
//        dstMem[dstAddr1]   = clip_buf[R_tmp];
//        dstMem[dstAddr1+1] = clip_buf[G_tmp];
//        dstMem[dstAddr1+2] = clip_buf[B_tmp];
//        dstAddr1 += 3;
//      }
//      // Pixel top right
//      {
//        const int Y_tmp = ((int)srcY[srcAddrY1 + x + 1] - yOffset) * yMult;
//
//        const int R_tmp = (Y_tmp           + V_tmp_R ) >> 16;
//        const int G_tmp = (Y_tmp + U_tmp_G + V_tmp_G ) >> 16;
//        const int B_tmp = (Y_tmp + U_tmp_B           ) >> 16;
//
//        dstMem[dstAddr1]   = clip_buf[R_tmp];
//        dstMem[dstAddr1+1] = clip_buf[G_tmp];
//        dstMem[dstAddr1+2] = clip_buf[B_tmp];
//        dstAddr1 += 3;
//      }
//      // Pixel bottom left
//      {
//        const int Y_tmp = ((int)srcY[srcAddrY2 + x] - yOffset) * yMult;
//
//        const int R_tmp = (Y_tmp           + V_tmp_R ) >> 16;
//        const int G_tmp = (Y_tmp + U_tmp_G + V_tmp_G ) >> 16;
//        const int B_tmp = (Y_tmp + U_tmp_B           ) >> 16;
//
//        dstMem[dstAddr2]   = clip_buf[R_tmp];
//        dstMem[dstAddr2+1] = clip_buf[G_tmp];
//        dstMem[dstAddr2+2] = clip_buf[B_tmp];
//        dstAddr2 += 3;
//      }
//      // Pixel bottom right
//      {
//        const int Y_tmp = ((int)srcY[srcAddrY2 + x + 1] - yOffset) * yMult;
//
//        const int R_tmp = (Y_tmp           + V_tmp_R ) >> 16;
//        const int G_tmp = (Y_tmp + U_tmp_G + V_tmp_G ) >> 16;
//        const int B_tmp = (Y_tmp + U_tmp_B           ) >> 16;
//
//        dstMem[dstAddr2]   = clip_buf[R_tmp];
//        dstMem[dstAddr2+1] = clip_buf[G_tmp];
//        dstMem[dstAddr2+2] = clip_buf[B_tmp];
//        dstAddr2 += 3;
//      }
//    }
//  }
//}

void videoHandlerYUV::setYUVPixelFormatByName(QString name, bool emitSignal)
{
  yuvPixelFormat newFormat(name);
  if (!newFormat.isValid())
    return;

  if (newFormat != srcPixelFormat)
  {
    if (controlsCreated)
    {
      // Check if the custom format it in the presets list. If not, add it
      int idx = yuvPresetsList.indexOf(newFormat);
      if (idx == -1)
      {
        // Valid pixel format with is not in the list. Add it...
        yuvPresetsList.append(newFormat);
        int nrItems = ui->yuvFormatComboBox->count();
        disconnect(ui->yuvFormatComboBox, SIGNAL(currentIndexChanged(int)), NULL, NULL);
        ui->yuvFormatComboBox->insertItem(nrItems-1, newFormat.getName() );
        // Setlect the added format
        idx = yuvPresetsList.indexOf(newFormat);
        ui->yuvFormatComboBox->setCurrentIndex(idx);
        connect(ui->yuvFormatComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotYUVFormatControlChanged(int)));
      }
      else
      {
        // Just select the format in the combo box
        disconnect(ui->yuvFormatComboBox, SIGNAL(currentIndexChanged(int)), NULL, NULL);
        ui->yuvFormatComboBox->setCurrentIndex(idx);
        connect(ui->yuvFormatComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotYUVFormatControlChanged(int)));
      }
    }

    setSrcPixelFormat(newFormat, emitSignal);
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

void videoHandlerYUV::invalidateAllBuffers()
{
  currentFrameRawYUVData_frameIdx = -1;
  rawYUVData_frameIdx = -1;
  videoHandler::invalidateAllBuffers();
}