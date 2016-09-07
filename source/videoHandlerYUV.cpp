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

// Return a list with all the packing formats that are supported with this subsampling
QStringList getSupportedPackingFormats(YUVSubsamplingType subsampling)
{
  if (subsampling == YUV_422)
    return QStringList() << "UYVY" << "VYUY" << "YUYV" << "YVYU";
  
  return QStringList();
}

// All values between 0 and this value are possible for the subsampling.
int getMaxPossibleChromaOffsetValues(bool horizontal, YUVSubsamplingType subsampling)
{
  if (subsampling == YUV_444)
    return 1;
  else if (subsampling == YUV_422)
    return (horizontal) ? 3 : 1;
  else if (subsampling == YUV_420)
    return 3;
  else if (subsampling == YUV_440)
    return (horizontal) ? 1 : 3;
  else if (subsampling == YUV_410)
    return 7;
  else if (subsampling == YUV_411)
    return (horizontal) ? 7 : 1;
  return 0;
}

// Is this the default chroma offset for the subsampling type?
bool isDefaultChromaFormat(int chromaOffset, bool offsetX, YUVSubsamplingType subsampling)
{
  if (subsampling == YUV_420 && !offsetX)
    // The default subsampling for YUV 420 has a Y offset of 1/2
    return chromaOffset == 1;
  return chromaOffset == 0;
}

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
    QRegExp rxYUVFormat("([YUVA]{3,6}) (4:[420]{1}:[420]{1}) ([0-9]{1,2})-bit[ ]?([BL]{1}E)?[ ]?(packed|packed-B)?[ ]?(Cx[0-9]+)?[ ]?(Cy[0-9]+)?");

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
        //static const QStringList orderNames = QStringList() << "YUV" << "YVU" << "AYUV" << "YUVA" << "UYVU" << "VYUY" << "YUYV" << "YVYU" << "YYYYUV" << "YYUYYV" << "UYYVYY" << "VYYUYY";
        static const QStringList orderNames = QStringList() << "UYVU" << "VYUY" << "YUYV" << "YVYU";
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

      // Get the chroma offsets
      setDefaultChromaOffset();
      if (rxYUVFormat.cap(6).startsWith("Cx"))
      {
        QString test = rxYUVFormat.cap(6);
        QString test2 = rxYUVFormat.cap(6).mid(2);
        newFormat.chromaOffset[0] = rxYUVFormat.cap(6).mid(2).toInt();
      }
      if (rxYUVFormat.cap(7).startsWith("Cy"))
        newFormat.chromaOffset[1] = rxYUVFormat.cap(7).mid(2).toInt();

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
        chromaOffset[0] = newFormat.chromaOffset[0];
        chromaOffset[1] = newFormat.chromaOffset[1];
      }
    }
  }

  bool yuvPixelFormat::isValid() const
  {
    if (!planar)
    {
      // Check the packing mode
      /*if ((packingOrder == Packing_YUV || packingOrder == Packing_YVU) && subsampling != YUV_444)
        return false;*/
      if ((packingOrder == Packing_UYVY || packingOrder == Packing_VYUY || packingOrder == Packing_YUYV || packingOrder == Packing_YVYU) && subsampling != YUV_422)
        return false;
      /*if ((packingOrder == Packing_YYYYUV || packingOrder == Packing_YYUYYV || packingOrder == Packing_UYYVYY || packingOrder == Packing_VYYUYY) && subsampling == YUV_420)
        return false;*/
      if (subsampling == YUV_444 || subsampling == YUV_420 || subsampling == YUV_440 || subsampling == YUV_410 || subsampling == YUV_411 || subsampling == YUV_400)
        // No support for packed formats with these subsamplings (yet)
        return false;
    }              
    // Check the chroma offsets
    if (chromaOffset[0] < 0 || chromaOffset[0] > getMaxPossibleChromaOffsetValues(true, subsampling))
      return false;
    if (chromaOffset[1] < 0 || chromaOffset[1] > getMaxPossibleChromaOffsetValues(false, subsampling))
      return false;
    // Check the bit depth
    if (bitsPerSample <= 0 || bitsPerSample < 7)
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
      QStringList orderNames = getSupportedPackingFormats(subsampling);
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

    // Add the Chroma offsets (if it is not the default offset)
    if (!isDefaultChromaFormat(chromaOffset[0], true, subsampling))
      name += QString(" Cx%1").arg(chromaOffset[0]);
    if (!isDefaultChromaFormat(chromaOffset[1], false, subsampling))
      name += QString(" Cy%1").arg(chromaOffset[1]);
     
    return name;
  }

  qint64 yuvPixelFormat::bytesPerFrame(QSize frameSize) const
  {
    qint64 bytes = 0;
    if (planar || !bytePacking)
    {
      // Add the bytes of the 3 (or 4) planes.
      // This also works for packed formats without byte packing. For these formats the number of bytes are identical to 
      // the not packed formats, the bytes are just sorted in another way.

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
      // This is a packed format with byte packing
      if (subsampling == YUV_422)
      {
        // All packing orders have 4 values per packed value (which has 2 Y samples)
        int bitsPerPixel = bitsPerSample * 4;
        return ((bitsPerPixel + 7) / 8) * (frameSize.width() / 2) * frameSize.height();
      }
      //// This is a packed format. The added number of bytes might be lower because of the packing.
      //if (subsampling == YUV_444)
      //{
      //  int bitsPerPixel = bitsPerSample * 3;
      //  if (packingOrder == Packing_AYUV || packingOrder == Packing_YUVA)
      //    bitsPerPixel += bitsPerSample;
      //  return ((bitsPerPixel + 7) / 8) * frameSize.width() * frameSize.height();
      //}
      //else if (subsampling == YUV_422 || subsampling == YUV_440)
      //{
      //  // All packing orders have 4 values per packed value (which has 2 Y samples)
      //  int bitsPerPixel = bitsPerSample * 4;
      //  return ((bitsPerPixel + 7) / 8) * (frameSize.width() / 2) * frameSize.height();
      //}
      //else if (subsampling == YUV_420)
      //{
      //  // All packing orders have 6 values per packed sample (which has 4 Y samples)
      //  int bitsPerPixel = bitsPerSample * 6;
      //  return ((bitsPerPixel + 7) / 8) * (frameSize.width() / 2) * (frameSize.height() / 2);
      //}
      //else
      //  return -1;  // Unknown subsampling
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
  void yuvPixelFormat::setDefaultChromaOffset()
  {
    chromaOffset[0] = 0;
    chromaOffset[1] = 0;
    if (subsampling == YUV_420)
      chromaOffset[1] = 1;
  }

  videoHandlerYUV_CustomFormatDialog::videoHandlerYUV_CustomFormatDialog(yuvPixelFormat yuvFormat)
  {
    setupUi(this);

    // Set all values correctly from the given yuvFormat
    // Set the chroma subsampling
    int idx = yuvFormat.subsampling;
    if (idx >= 0 && idx < YUV_NUM_SUBSAMPLINGS)
    {
      comboBoxChromaSubsampling->setCurrentIndex(idx);
      // The Q_Object auto connection is performed later so call the slot manually.
      on_comboBoxChromaSubsampling_currentIndexChanged(idx);
    }

    // Set the bit depth
    static const QList<int> bitDepths = QList<int>() << 8 << 9 << 10 << 12 << 14 << 16;
    idx = bitDepths.indexOf(yuvFormat.bitsPerSample);
    if (idx < 0 || idx >= 6)
      idx = 0;
    comboBoxBitDepth->setCurrentIndex(idx);
    comboBoxEndianess->setEnabled(idx != 0);

    // Set the endianess
    comboBoxEndianess->setCurrentIndex(yuvFormat.bigEndian ? 0 : 1);

    // Set the chroma offsets
    comboBoxChromaOffsetX->setCurrentIndex(yuvFormat.chromaOffset[0]);
    comboBoxChromaOffsetY->setCurrentIndex(yuvFormat.chromaOffset[1]);

    if (yuvFormat.planar)
    {
      // Set the plane order
      groupBoxPlanar->setChecked(true);
      idx = yuvFormat.planeOrder;
      if (idx >= 0 && idx < 4)
        comboBoxPlaneOrder->setCurrentIndex(idx);
    }
    else
    {
      // Set the packing order
      groupBoxPacked->setChecked(true);
      idx = yuvFormat.packingOrder;
      // The index in the combo box depends on the subsampling
      if (yuvFormat.subsampling == YUV_422)
        idx -= int(Packing_UYVY);       // The first packing format for 422
      //else if (yuvFormat.packingOrder == YUV_420)
      //  idx -= int(Packing_YYYYUV);     // The first packing format for 420

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
    // What packing types are supported?
    YUVSubsamplingType subsampling = static_cast<YUVSubsamplingType>(idx);
    QStringList packingTypes = getSupportedPackingFormats(subsampling);
    comboBoxPackingOrder->clear();
    comboBoxPackingOrder->addItems(packingTypes);

    bool packedSupported = (packingTypes.count() != 0);
    if (!packedSupported)
      // No packing supported for this subsampling. Select planar.
      groupBoxPlanar->setChecked(true);
    else
      comboBoxPackingOrder->setCurrentIndex(0);

    // What chroma offsets are possible?
    comboBoxChromaOffsetX->clear();
    int maxValsX = getMaxPossibleChromaOffsetValues(true, subsampling);
    if (maxValsX >= 1)
      comboBoxChromaOffsetX->addItems(QStringList() << "0" << "1/2");
    if (maxValsX >= 3)
      comboBoxChromaOffsetX->addItems(QStringList() << "1" << "3/2");
    if (maxValsX >= 7)
      comboBoxChromaOffsetX->addItems(QStringList() << "2" << "5/2" << "3" << "7/2");
    comboBoxChromaOffsetY->clear();
    int maxValsY = getMaxPossibleChromaOffsetValues(false, subsampling);
    if (maxValsY >= 1)
      comboBoxChromaOffsetY->addItems(QStringList() << "0" << "1/2");
    if (maxValsY >= 3)
      comboBoxChromaOffsetY->addItems(QStringList() << "1" << "3/2");
    if (maxValsY >= 7)
      comboBoxChromaOffsetY->addItems(QStringList() << "2" << "5/2" << "3" << "7/2");
    
    // Disable the combo boxes if there are no chroma components
    bool chromaPresent = (subsampling != YUV_400);
    groupBoxPacked->setEnabled(chromaPresent && packedSupported);
    groupBoxPlanar->setEnabled(chromaPresent);
  }

  yuvPixelFormat videoHandlerYUV_CustomFormatDialog::getYUVFormat() const
  {
    // Get all the values from the controls
    yuvPixelFormat format;
    
    // Set the subsampling format
    int idx = comboBoxChromaSubsampling->currentIndex();
    Q_ASSERT(idx >= 0 && idx < YUV_NUM_SUBSAMPLINGS);
    format.subsampling = static_cast<YUVSubsamplingType>(idx);

    // Set the bit depth
    idx = comboBoxBitDepth->currentIndex();
    static const int bitDepths[] = {8, 9, 10, 12, 14, 16};
    Q_ASSERT(idx >= 0 && idx < 6);
    format.bitsPerSample = bitDepths[idx];

    // Set the endianess
    format.bigEndian = (comboBoxEndianess->currentIndex() == 0);

    // Set the chroma offset
    format.chromaOffset[0] = comboBoxChromaOffsetX->currentIndex();
    format.chromaOffset[1] = comboBoxChromaOffsetY->currentIndex();
    
    // Planar or packed format?
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
      /*if (format.subsampling == YUV_422 || format.subsampling == YUV_440)
        offset = Packing_UYVY;*/
      if (format.subsampling == YUV_420)
        offset = Packing_YVYU + 1;
      Q_ASSERT(idx+offset >= 0 && idx+offset < Packing_NUM);
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
      else
      {
        // The format is already in the list. Select it without invoking another signal.
        disconnect(ui->yuvFormatComboBox, SIGNAL(currentIndexChanged(int)), NULL, NULL);
        ui->yuvFormatComboBox->setCurrentIndex(idx);
        connect(ui->yuvFormatComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotYUVFormatControlChanged(int)));
      }
    }
    else
    {
      // The user pressed cancel. Go back to the old format
      int idx = yuvPresetsList.indexOf(srcPixelFormat);
      Q_ASSERT(idx != -1);  // The previously selected format should always be in the list
      disconnect(ui->yuvFormatComboBox, SIGNAL(currentIndexChanged(int)), NULL, NULL);
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
  srcPixelFormat = format;

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

/* Get the pixels values so we can show them in the info part of the zoom box.
 * If a second frame handler is provided, the difference values from that item will be returned.
 */
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

/* Draw the YUV values of the pixels over the avtual pixels when zoomed in. The values are drawn at the position where
 * they are assumed. So also chroma shifts and subsamplings are drawn correctly.
 */
void videoHandlerYUV::drawPixelValues(QPainter *painter, const int frameIdx, const QRect videoRect, const double zoomFactor, frameHandler *item2, const bool markDifference)
{
  // Get the other YUV item (if any)
  videoHandlerYUV *yuvItem2 = (item2 == NULL) ? NULL : dynamic_cast<videoHandlerYUV*>(item2);
  const bool useDiffValues = (yuvItem2 != NULL);

  QSize size = frameSize;
  if (useDiffValues)
    // If the two items are not of equal size, use the minimum possible size.
    size = QSize(min(frameSize.width(), yuvItem2->frameSize.width()), min(frameSize.height(), yuvItem2->frameSize.height()));

  // For difference items, we support difference bit depths for the two items.
  // If the bit depth is different, we scale to value with the lower bit depth to the higher bit depth and calculate the difference there.
  // These values are only needed for difference values
  const int bps_in[2] = {srcPixelFormat.bitsPerSample, (useDiffValues) ? yuvItem2->srcPixelFormat.bitsPerSample : 0};
  const int bps_out = max(bps_in[0], bps_in[1]);
  // Which of the two input values has to be scaled up? Only one of these (or neither) can be set.
  const bool bitDepthScaling[2] = {bps_in[0] != bps_out, bps_in[1] != bps_out};
  // Scale the input up by this many bits
  const int depthScale = bps_out - (bitDepthScaling[0] ? bps_in[0] : bps_in[1]);
  // What are the maximum and middle value for the output bit depth
  const int diffZero = 128 << (bps_out-8);

  // First determine which pixels from this item are actually visible, because we only have to draw the pixel values
  // of the pixels that are actually visible
  QRect viewport = painter->viewport();
  QTransform worldTransform = painter->worldTransform();
    
  int xMin_tmp = (videoRect.width() / 2 - worldTransform.dx()) / zoomFactor;
  int yMin_tmp = (videoRect.height() / 2 - worldTransform.dy()) / zoomFactor;
  int xMax_tmp = (videoRect.width() / 2 - (worldTransform.dx() - viewport.width() )) / zoomFactor;
  int yMax_tmp = (videoRect.height() / 2 - (worldTransform.dy() - viewport.height() )) / zoomFactor;

  // Clip the min/max visible pixel values to the size of the item (no pixels outside of the
  // item have to be labeled)
  const int xMin = clip(xMin_tmp, 0, size.width()-1);
  const int yMin = clip(yMin_tmp, 0, size.height()-1);
  const int xMax = clip(xMax_tmp, 0, size.width()-1);
  const int yMax = clip(yMax_tmp, 0, size.height()-1);

  // The center point of the pixel (0,0).
  const QPoint centerPointZero = ( QPoint(-size.width(), -size.height()) * zoomFactor + QPoint(zoomFactor,zoomFactor) ) / 2;
  // This rect has the size of one pixel and is moved on top of each pixel to draw the text
  QRect pixelRect;
  pixelRect.setSize( QSize(zoomFactor, zoomFactor) );

  // We might change the pen doing this so backup the current pen and reset it later
  QPen backupPen = painter->pen();

  // If the Y is below this value, use white text, otherwise black text
  // If there is a second item, a difference will be drawn. A difference of 0 is displayed as gray.
  const int whiteLimit = (yuvItem2) ? 0 : 1 << (srcPixelFormat.bitsPerSample - 1);

  // The chroma offset in full luma pixels. This can range from 0 to 3.
  const int chromaOffsetFullX = srcPixelFormat.chromaOffset[0] / 2;
  const int chromaOffsetFullY = srcPixelFormat.chromaOffset[1] / 2;
  // Is the chroma offset by another half luma pixel?
  const bool chromaOffsetHalfX = (srcPixelFormat.chromaOffset[0] % 2 != 0);
  const bool chromaOffsetHalfY = (srcPixelFormat.chromaOffset[1] % 2 != 0);
  // By what factor is X and Y subsampled?
  const int subsamplingX = srcPixelFormat.getSubsamplingHor();
  const int subsamplingY = srcPixelFormat.getSubsamplingVer();
  
  for (int x = xMin; x <= xMax; x++)
  {
    for (int y = yMin; y <= yMax; y++)
    {
      // Calculate the center point of the pixel. (Each pixel is of size (zoomFactor,zoomFactor)) and move the pixelRect to that point.
      QPoint pixCenter = centerPointZero + QPoint(x * zoomFactor, y * zoomFactor);
      pixelRect.moveCenter(pixCenter);

      // Get the YUV values to show
      int Y,U,V;
      bool drawWhite;
      if (useDiffValues)
      {
        unsigned int Y0, U0, V0, Y1, U1, V1;
        getPixelValue(QPoint(x,y), frameIdx, Y0, U0, V0);
        yuvItem2->getPixelValue(QPoint(x,y), frameIdx, Y1, U1, V1);

        // Do we have to scale one of the values (bit depth different)
        if (bitDepthScaling[0])
        {
          Y0 = Y0 << depthScale;
          U0 = U0 << depthScale;
          V0 = V0 << depthScale;
        }
        else if (bitDepthScaling[1])
        {
          Y1 = Y1 << depthScale;
          U1 = U1 << depthScale;
          V1 = V1 << depthScale;
        }

        Y = Y0-Y1; U = U0-U1; V = V0-V1;

        if (markDifference)
          drawWhite = (Y == 0);
        else
          drawWhite = (mathParameters[Luma].invert) ? (Y > whiteLimit) : (Y < whiteLimit);
      }
      else
      {
        unsigned int Yu,Uu,Vu;
        getPixelValue(QPoint(x,y), frameIdx, Yu, Uu, Vu);
        Y = Yu; U = Uu; V = Vu;
        drawWhite = (mathParameters[Luma].invert) ? (Y > whiteLimit) : (Y < whiteLimit);
      }

      // Set the pen
      painter->setPen( drawWhite ? Qt::white : Qt::black );

      if ((x-chromaOffsetFullX) % subsamplingX == 0 && (y-chromaOffsetFullY) % subsamplingY == 0)
      {
        QString valText;
        if (chromaOffsetHalfX || chromaOffsetHalfY)
          // We will only draw the Y value at the center of this pixel
          valText = QString("Y%1").arg(Y);
        else
          // We also draw the U and V value at this position
          valText = QString("Y%1\nU%2\nV%3").arg(Y).arg(U).arg(V);
        // Draw
        painter->drawText(pixelRect, Qt::AlignCenter, valText);

        if (chromaOffsetHalfX || chromaOffsetHalfY)
        {
          // Draw the U and V values shifted half a pixel right and/or down
          valText = QString("U%1\nV%2").arg(U).arg(V);

          // Move the rect by half a pixel
          if (chromaOffsetHalfX)
            pixelRect.translate(zoomFactor/2, 0);
          if (chromaOffsetHalfY)
            pixelRect.translate(0, zoomFactor/2);

          // Draw
          painter->drawText(pixelRect, Qt::AlignCenter, valText);
        }
      }
      else
      {
        // We only draw the luma value for this pixel
        QString valText = QString("Y%1").arg(Y);
        painter->drawText(pixelRect, Qt::AlignCenter, valText);
      }
    }
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

  // Secondly, all packed formats

  for (int s = 0; s < YUV_NUM_SUBSAMPLINGS; s++)
  {
    YUVSubsamplingType subsampling = static_cast<YUVSubsamplingType>(s);
    // What packing formats are supported by this subsampling?
    QStringList packingOrderList = getSupportedPackingFormats(subsampling);
    for (int o = 0; o < packingOrderList.count(); o++)
    {
      YUVPackingOrder packing = static_cast<YUVPackingOrder>(o);

      Q_FOREACH(int bitDepth, bitDepthList)
      {
        QStringList endianessList = QStringList() << "le";
        if (bitDepth > 8)
          endianessList << "be";

        Q_FOREACH(QString endianess, endianessList)
        {
          QString formatName = packingOrderList[o].toLower() + subsamplingNameList[s];
          if (bitDepth > 8)
            formatName += QString::number(bitDepth) + endianess;

          // Check if this format is in the file name
          if (fileName.contains(formatName))
          {
            // Check if the format and the file size match
            yuvPixelFormat fmt = yuvPixelFormat(subsampling, bitDepth, packing, false, endianess=="be");
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

  // Ok that did not work so far. Try other guesses.
  // TODO.
 
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
    convertYUVToPixmap(currentFrameRawYUVData, currentFrame, tmpBufferRGB, srcPixelFormat, frameSize);
    currentFrameIdx = frameIndex;
  }
}

void videoHandlerYUV::loadFrameForCaching(int frameIndex, QPixmap &frameToCache)
{
  DEBUG_YUV( "videoHandlerYUV::loadFrameForCaching %d", frameIndex );

  // Get the YUV format and the size here, so that the caching process does not crash if this changes.
  yuvPixelFormat yuvFormat = srcPixelFormat;
  const QSize curFrameSize = frameSize;

  requestDataMutex.lock();
  emit signalRequesRawData(frameIndex);
  tmpBufferRawYUVDataCaching = rawYUVData;
  requestDataMutex.unlock();

  if (frameIndex != rawYUVData_frameIdx)
  {
    // Loading failed
    currentFrameIdx = -1;
    return;
  }

  // Convert YUV to pixmap. This can then be cached.
  QByteArray tmpBufferRGBCaching;
  convertYUVToPixmap(tmpBufferRawYUVDataCaching, frameToCache, tmpBufferRGBCaching, srcPixelFormat, curFrameSize);
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

inline int getValueFromSource(const unsigned char * restrict src, const int idx, const int bps, const bool bigEndian)
{
  if (bps > 8)
    // Read two bytes in the right order
    return (bigEndian) ? src[idx*2] << 8 | src[idx*2+1] : src[idx*2] | src[idx*2+1] << 8;
  else
    // Just read one byte
    return src[idx];
}

inline void setValueInBuffer(unsigned char * restrict dst, const int val, const int idx, const int bps, const bool bigEndian)
{
  if (bps > 8)
  {
    // Write two bytes
    if (bigEndian)
    {
      dst[idx*2] = val >> 8;
      dst[idx*2+1] = val & 0xff;
    }
    else
    {
      dst[idx*2] = val & 0xff;
      dst[idx*2+1] = val >> 8;
    }
  }
  else
    // Write one byte
    *dst = val;
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

// Depending on offsetX8 (which can be 1 to 7), interpolate one of the 6 given positions between prev and cur.
inline int interpolateUV8Pos(int prev, int cur, const int offsetX8)
{
  if (offsetX8 == 4)
    return (prev + cur + 1) / 2;
  if (offsetX8 == 2)
    return (prev + cur*3 + 2) / 4;
  if (offsetX8 == 6)
    return (prev*3 + cur + 2) / 4;
  if (offsetX8 == 1)
    return (prev + cur*7 + 4) / 8;
  if (offsetX8 == 3)
    return (prev*3 + cur*5 + 4) / 8;
  if (offsetX8 == 5)
    return (prev*5 + cur*3 + 4) / 8;
  if (offsetX8 == 7)
    return (prev*7 + cur + 4) / 8;
}

// Resample the chroma component so that the chroma samples and the luma samples are alligned after this operation.
inline void UVPlaneResamplingChromaOffset(const yuvPixelFormat format, const int w, const int h, unsigned char * restrict srcU, unsigned char * restrict srcV)
{
  // We can perform linear interpolation for 7 positions (6 in between) two pixels.
  // Which of these position is needed depends on the chromaOffset and the subsampling.
  const int possibleValsX = getMaxPossibleChromaOffsetValues(true,  format.subsampling);
  const int possibleValsY = getMaxPossibleChromaOffsetValues(false, format.subsampling);
  const int offsetX8 = (possibleValsX == 1) ? format.chromaOffset[0] * 4 : (possibleValsX == 3) ? format.chromaOffset[0] * 2 : format.chromaOffset[0];
  const int offsetY8 = (possibleValsY == 1) ? format.chromaOffset[1] * 4 : (possibleValsY == 3) ? format.chromaOffset[1] * 2 : format.chromaOffset[1];
  
  // Copy the pointers so that we can walk in them
  unsigned char * restrict U = srcU;
  unsigned char * restrict V = srcV;

  // The format to use for input/output
  const bool bigEndian = format.bigEndian;
  const int bps = format.bitsPerSample;

  const int stride = bps > 8 ? w*2 : w;
  if (offsetX8 != 0)
  {
    // Perform horizontal resampling
    for (int y = 0; y < h; y++)
    {
      // On the left side, there is no previous sample, so the first value is never changed.
      int prevU = getValueFromSource(U, 0, bps, bigEndian);
      int prevV = getValueFromSource(V, 0, bps, bigEndian);

      for (int x = 0; x < w-1; x++)
      {
        // Calculate the new current value using the previous and the current value
        int curU = getValueFromSource(U, x+1, bps, bigEndian);
        int curV = getValueFromSource(V, x+1, bps, bigEndian);

        // Perform interpolation and save the value for the current UV value. Goto next value.
        int newU = interpolateUV8Pos(prevU, curU, offsetX8);
        int newV = interpolateUV8Pos(prevV, curV, offsetX8);
        setValueInBuffer(U, newU, x+1, bps, bigEndian);
        setValueInBuffer(V, newV, x+1, bps, bigEndian);

        prevU = curU;
        prevV = curV;
      }

      // Goto the next Y line
      U += stride;
      V += stride;
    }
  }

  if (offsetY8 != 0)
  {
    // Perform vertical resampling. It works exactly like horizontal upsampling but x and y are switched.
    for (int x = 0; x < w; x++)
    {
      // Reset the pointers to the correct value at the top line
      U = srcU + (bps > 8 ? x*2 : x);
      V = srcV + (bps > 8 ? x*2 : x);

      // On the top, there is no previous sample, so the first value is never changed.
      int prevU = getValueFromSource(U, 0, bps, bigEndian);
      int prevV = getValueFromSource(V, 0, bps, bigEndian);
      // Goto the next line (y)
      U += stride;
      V += stride;

      for (int y = 0; y < h-1; y++)
      {
        // Calculate the new current value using the previous and the current value
        int curU = getValueFromSource(U, 0, bps, bigEndian);
        int curV = getValueFromSource(V, 0, bps, bigEndian);

        // Perform interpolation and save the value for the current UV value. Goto next value.
        int newU = interpolateUV8Pos(prevU, curU, offsetY8);
        int newV = interpolateUV8Pos(prevV, curV, offsetY8);
        setValueInBuffer(U, newU, 0, bps, bigEndian);
        setValueInBuffer(V, newV, 0, bps, bigEndian);

        // Goto the next line (y)
        U += stride;
        V += stride;
        prevU = curU;
        prevV = curV;
      }
    }
  }
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

bool videoHandlerYUV::convertYUVPackedToPlanar(QByteArray &sourceBuffer, QByteArray &targetBuffer, const QSize curFrameSize, yuvPixelFormat &sourceBufferFormat)
{
  const yuvPixelFormat format = sourceBufferFormat;
  const YUVPackingOrder packing = format.packingOrder;

  // Make sure that the target buffer is big enough. It should be as big as the input buffer.
  if (targetBuffer.size() != sourceBuffer.size())
    targetBuffer.resize(sourceBuffer.size());

  const int w = curFrameSize.width();
  const int h = curFrameSize.height();

  if (format.subsampling == YUV_422)
  {
    // Bytes per sample
    const int bps = (format.bitsPerSample > 8) ? 2 : 1;

    // The data is arranged in blocks of 4 samples. How many of these are there?
    const int nr4Samples = w*h/2;
    const int offsetU = w*h*bps;
    const int offsetV = offsetU + w/2*h*bps;

    // What are the offsets withing the 4 samples for the components?
    const int oY = (packing == Packing_YUYV || packing == Packing_YVYU) ? 0 : 1;
    const int oU = (packing == Packing_UYVY) ? 0 : (packing == Packing_YUYV) ? 1 : (packing == Packing_VYUY) ? 2 : 3;
    const int oV = (packing == Packing_VYUY) ? 0 : (packing == Packing_YVYU) ? 1 : (packing == Packing_UYVY) ? 2 : 3;
    
    if (bps == 1)
    {
      // One byte per sample.
      const unsigned char * restrict src = (unsigned char*)sourceBuffer.data();
      unsigned char * restrict dstY = (unsigned char*)targetBuffer.data();
      unsigned char * restrict dstU = dstY + w*h;
      unsigned char * restrict dstV = dstU + w/2*h;

      for (int i = 0; i < nr4Samples; i++)
      {
        *dstY++ = src[oY];
        *dstY++ = src[oY+2];
        *dstU++ = src[oU];
        *dstV++ = src[oV];
        src += 4; // Goto the next 4 samples
      }
    }
    else
    {
      // Two bytes per sample.
      const unsigned short * restrict src = (unsigned short*)sourceBuffer.data();
      unsigned short * restrict dstY = (unsigned short*)targetBuffer.data();
      unsigned short * restrict dstU = dstY + w*h;
      unsigned short * restrict dstV = dstU + w/2*h;

      for (int i = 0; i < nr4Samples; i++)
      {
        *dstY++ = src[oY];
        *dstY++ = src[oY+2];
        *dstU++ = src[oU];
        *dstV++ = src[oV];
        src += 4; // Goto the next 4 samples
      }
    }

    // The output buffer is planar and YUV.
    sourceBufferFormat.planar = true;
    sourceBufferFormat.planeOrder = Order_YUV;
  }
  else
    return false;

  return true;
}

bool videoHandlerYUV::convertYUVPlanarToRGB(QByteArray &sourceBuffer, QByteArray &targetBuffer, const QSize curFrameSize, const yuvPixelFormat sourceBufferFormat) const
{
  // These are constant for the runtime of this function. This way, the compiler can optimize the
  // hell out of this function.
  const yuvPixelFormat format = sourceBufferFormat;
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
    if (format.chromaOffset[0] != 0 || format.chromaOffset[1])
    {
      // We have to perform prefiltering for the U and V positions, because there is an offset between the pixel positions of Y and U/V
      unsigned char * restrict srcY = (unsigned char*)sourceBuffer.data();
      unsigned char * restrict srcU = (format.planeOrder == Order_YUV || format.planeOrder == Order_YUVA) ? srcY + nrBytesLumaPlane : srcY + nrBytesLumaPlane + nrBytesChromaPlane;
      unsigned char * restrict srcV = (format.planeOrder == Order_YUV || format.planeOrder == Order_YUVA) ? srcY + nrBytesLumaPlane + nrBytesChromaPlane: srcY + nrBytesLumaPlane;
      UVPlaneResamplingChromaOffset(format, w / format.getSubsamplingHor(), h / format.getSubsamplingVer(), srcU, srcV);
    }

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
void videoHandlerYUV::convertYUVToPixmap(QByteArray sourceBuffer, QPixmap &outputPixmap, QByteArray &tmpRGBBuffer, const yuvPixelFormat yuvFormat, const QSize curFrameSize)
{
  DEBUG_YUV( "videoHandlerYUV::convertYUVToPixmap" );
  
  // Convert the source to RGB
  bool convOK = true;
  if (yuvFormat.planar)
    convOK = convertYUVPlanarToRGB(sourceBuffer, tmpRGBBuffer, curFrameSize, yuvFormat);
  else
  {
    // Convert to a planar format first
    QByteArray tmpPlanarYUVSource;
    // This is the current format of the buffer. The conversion function will change this.
    yuvPixelFormat bufferPixelFormat = yuvFormat;
    convOK &= convertYUVPackedToPlanar(sourceBuffer, tmpPlanarYUVSource, curFrameSize, bufferPixelFormat);

    if (convOK)
      convOK &= convertYUVPlanarToRGB(tmpPlanarYUVSource, tmpRGBBuffer, curFrameSize, bufferPixelFormat);
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

  const int w = frameSize.width();
  const int h = frameSize.height();

  if (format.planar)
  {
    // The luma component has full resolution. The size of each chroma components depends on the subsampling.
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
  else
  {
    if (format.subsampling == YUV_422)
    {
      // The data is arranged in blocks of 4 samples. How many of these are there?
      // What are the offsets withing the 4 samples for the components?
      const YUVPackingOrder packing = format.packingOrder;
      const int oY = (packing == Packing_YUYV || packing == Packing_YVYU) ? 0 : 1;
      const int oU = (packing == Packing_UYVY) ? 0 : (packing == Packing_YUYV) ? 1 : (packing == Packing_VYUY) ? 2 : 3;
      const int oV = (packing == Packing_VYUY) ? 0 : (packing == Packing_YVYU) ? 1 : (packing == Packing_UYVY) ? 2 : 3;
      
      const unsigned int offsetCoordinate4Block = w * pixelPos.y() + ((pixelPos.x() >> 2) << 2);
      const unsigned char * restrict src = (unsigned char*)currentFrameRawYUVData.data() + offsetCoordinate4Block;
      
      Y = getValueFromSource(src, (pixelPos.x() % 2 == 0) ? oY : oY + 2,  format.bitsPerSample, format.bigEndian);
      U = getValueFromSource(src, oU, format.bitsPerSample, format.bigEndian);
      V = getValueFromSource(src, oV, format.bitsPerSample, format.bigEndian);
    }
  }

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

bool videoHandlerYUV::markDifferencesYUVPlanarToRGB(QByteArray &sourceBuffer, QByteArray &targetBuffer, const QSize curFrameSize, const yuvPixelFormat sourceBufferFormat) const
{
  // These are constant for the runtime of this function. This way, the compiler can optimize the
  // hell out of this function.
  const yuvPixelFormat format = sourceBufferFormat;
  const int w = curFrameSize.width();
  const int h = curFrameSize.height();

  const int bps = format.bitsPerSample;
  const int cZero = 128<<(bps-8);

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

  // Is this big endian (actually the difference buffer should always be big endian)
  const bool bigEndian = format.bigEndian;

  // A pointer to the output
  unsigned char * restrict dst = (unsigned char*)targetBuffer.data();

  // Get the pointers to the source planes (8 bit per sample)
  const unsigned char * restrict srcY = (unsigned char*)sourceBuffer.data();
  const unsigned char * restrict srcU = (format.planeOrder == Order_YUV || format.planeOrder == Order_YUVA) ? srcY + nrBytesLumaPlane : srcY + nrBytesLumaPlane + nrBytesChromaPlane;
  const unsigned char * restrict srcV = (format.planeOrder == Order_YUV || format.planeOrder == Order_YUVA) ? srcY + nrBytesLumaPlane + nrBytesChromaPlane: srcY + nrBytesLumaPlane;

  const int sampleBlocksX = format.getSubsamplingHor();
  const int sampleBlocksY = format.getSubsamplingVer();

  const int strideC = w / sampleBlocksX;  // How many samples to the next y line?
  for (int y = 0; y < h; y+=sampleBlocksY)
    for (int x = 0; x < w; x+=sampleBlocksX)
    {
      // Get the U/V difference value. For all values within the sub-block this is constant.
      int uvIndex = (y/sampleBlocksY)*strideC + x / sampleBlocksX;
      int valU = getValueFromSource(srcU, uvIndex, bps, bigEndian);
      int valV = getValueFromSource(srcV, uvIndex, bps, bigEndian);

      for (int yInBlock = 0; yInBlock < sampleBlocksY; yInBlock++)
      {
        for (int xInBlock = 0; xInBlock < sampleBlocksX; xInBlock++)
        {
          // Get the Y difference value
          int valY = getValueFromSource(srcY, (y+yInBlock)*w+x+xInBlock, bps, bigEndian);

          //select RGB color
          unsigned char R = 0,G = 0,B = 0;
          if (valY == cZero)
          {
            G = (valU == cZero) ? 0 : 70;
            B = (valV == cZero) ? 0 : 70;
          }
          else
          {
            // Y difference
            if (valU == cZero && valV == cZero)
            {
              R = 70;
              G = 70;
              B = 70;
            }
            else
            {
              G = (valU == cZero) ? 0 : 255;
              B = (valV == cZero) ? 0 : 255;
            }
          }

          // Set the RGB value for the output
          dst[((y+yInBlock)*w+x+xInBlock)*3  ] = R;
          dst[((y+yInBlock)*w+x+xInBlock)*3+1] = G;
          dst[((y+yInBlock)*w+x+xInBlock)*3+2] = B;
        }
      }
    }

  return true;
}


QPixmap videoHandlerYUV::calculateDifference(frameHandler *item2, const int frame, QList<infoItem> &differenceInfoList, const int amplificationFactor, const bool markDifference)
{
  videoHandlerYUV *yuvItem2 = dynamic_cast<videoHandlerYUV*>(item2);
  if (yuvItem2 == NULL)
    // The given item is not a yuv source. We cannot compare YUV values to non YUV values.
    // Call the base class comparison function to compare the items using the RGB values.
    return videoHandler::calculateDifference(item2, frame, differenceInfoList, amplificationFactor, markDifference);

  if (srcPixelFormat.subsampling != yuvItem2->srcPixelFormat.subsampling)
    // The two items have different subsamplings. Compare RGB values instead.
    return videoHandler::calculateDifference(item2, frame, differenceInfoList, amplificationFactor, markDifference);

  // Get/Set the bit depth of the input and output
  // If the bit depth if the two items is different, we will scale the item with the lower bit depth up.
  const int bps_in[2] = {srcPixelFormat.bitsPerSample, yuvItem2->srcPixelFormat.bitsPerSample};
  const int bps_out = max(bps_in[0], bps_in[1]);
  // Which of the two input values has to be scaled up? Only one of these (or neither) can be set.
  const bool bitDepthScaling[2] = {bps_in[0] != bps_out, bps_in[1] != bps_out};
  // Scale the input up by this many bits
  const int depthScale = bps_out - (bitDepthScaling[0] ? bps_in[0] : bps_in[1]);
  // Add a warning if the bit dephs of the two inputs don't agree
  if (bitDepthScaling[0] || bitDepthScaling[1])
    differenceInfoList.append(infoItem("Warning", "The bit depth of the two items differs.", "The bit depth of the two input items is different. The lower bit depth will be scaled up and the difference is calculated."));
  // What are the maximum and middle value for the output bit depth
  const int diffZero = 128 << (bps_out-8);
  const int maxVal = (1 << bps_out) - 1;

  // Do we aplify the values?
  const bool amplification = (amplificationFactor != 1 && !markDifference);
  
  // Load the right raw YUV data (if not already loaded).
  // This will just update the raw YUV data. No conversion to pixmap (RGB) is performed. This is either
  // done on request if the frame is actually shown or has already been done by the caching process.
  if (!loadRawYUVData(frame))
    return QPixmap();  // Loading failed
  if (!yuvItem2->loadRawYUVData(frame))
    return QPixmap();  // Loading failed

  // The items can be of different size (we then diff the top left aligned part)
  const int w_in[2] = {frameSize.width(), yuvItem2->frameSize.width()};
  const int h_in[2] = {frameSize.height(), yuvItem2->frameSize.height()};
  const int w_out = qMin(w_in[0], w_in[1]);
  const int h_out = qMin(h_in[0], h_in[1]);
  // Append a warning if the frame sizes are different
  if (frameSize != yuvItem2->frameSize)
    differenceInfoList.append(infoItem("Warning", "The size of the two items differs.", "The size of the two input items is different. The difference of the top left aligned part that overlaps will be calculated."));

  // Create a buffer for the yuv difference values ans resize it to the same size as the input values.
#if SSE_CONVERSION
  byteArrayAligned tmpDiffYUV;
#else
  QByteArray tmpDiffYUV;
#endif
  
  // Get subsamplings (they are identical for both inputs and the output)
  const int subH = srcPixelFormat.getSubsamplingHor();
  const int subV = srcPixelFormat.getSubsamplingVer();

  // Get the endianess of the inputs
  const bool bigEndian[2] = {srcPixelFormat.bigEndian, yuvItem2->srcPixelFormat.bigEndian};

  // Get pointers to the inputs
  const int componentSizeLuma_In[2] = {w_in[0]*h_in[0], w_in[1]*h_in[1]};
  const int componentSizeChroma_In[2] = {(w_in[0]/subH)*(h_in[0]/subV), (w_in[1]/subH)*(h_in[1]/subV)};
  const int nrBytesLumaPlane_In[2] = {bps_in[0] > 8 ? 2 * componentSizeLuma_In[0] : componentSizeLuma_In[0], bps_in[1] > 8 ? 2 * componentSizeLuma_In[1] : componentSizeLuma_In[1]};
  const int nrBytesChromaPlane_In[2] = {bps_in[0] > 8 ? 2 * componentSizeChroma_In[0] : componentSizeChroma_In[0], bps_in[1] > 8 ? 2 * componentSizeChroma_In[1] : componentSizeChroma_In[1]};
  // Current item
  const unsigned char * restrict srcY1 = (unsigned char*)currentFrameRawYUVData.data();
  const unsigned char * restrict srcU1 = (srcPixelFormat.planeOrder == Order_YUV || srcPixelFormat.planeOrder == Order_YUVA) ? srcY1 + nrBytesLumaPlane_In[0] : srcY1 + nrBytesLumaPlane_In[0] + nrBytesChromaPlane_In[0];
  const unsigned char * restrict srcV1 = (srcPixelFormat.planeOrder == Order_YUV || srcPixelFormat.planeOrder == Order_YUVA) ? srcY1 + nrBytesLumaPlane_In[0] + nrBytesChromaPlane_In[0]: srcY1 + nrBytesLumaPlane_In[0];
  // The other item
  const unsigned char * restrict srcY2 = (unsigned char*)yuvItem2->currentFrameRawYUVData.data();
  const unsigned char * restrict srcU2 = (yuvItem2->srcPixelFormat.planeOrder == Order_YUV || yuvItem2->srcPixelFormat.planeOrder == Order_YUVA) ? srcY2 + nrBytesLumaPlane_In[1] : srcY2 + nrBytesLumaPlane_In[1] + nrBytesChromaPlane_In[1];
  const unsigned char * restrict srcV2 = (yuvItem2->srcPixelFormat.planeOrder == Order_YUV || yuvItem2->srcPixelFormat.planeOrder == Order_YUVA) ? srcY2 + nrBytesLumaPlane_In[1] + nrBytesChromaPlane_In[1]: srcY2 + nrBytesLumaPlane_In[1];

  // Get pointers to the output
  const int componentSizeLuma_out = w_out*h_out * (bps_out > 8 ? 2 : 1); // Size in bytes
  const int componentSizeChroma_out = (w_out/subH) * (h_out/subV) * (bps_out > 8 ? 2 : 1);
  // Resize the output buffer to the right size
  tmpDiffYUV.resize(componentSizeLuma_out + 2*componentSizeChroma_out);
  unsigned char * restrict dstY = (unsigned char*)tmpDiffYUV.data();
  unsigned char * restrict dstU = dstY + componentSizeLuma_out;
  unsigned char * restrict dstV = dstU + componentSizeChroma_out;

  // Also calculate the MSE while we're at it (Y,U,V)
  // TODO: Bug: MSE is not scaled correctly in all YUV format cases
  qint64 mseAdd[3] = {0, 0, 0};

  // Diff Luma samples
  const int stride_in[2] = {bps_in[0] > 8 ? w_in[0]*2 : w_in[0], bps_in[1] > 8 ? w_in[1]*2 : w_in[1]};  // How many bytes to the next y line?
  for (int y = 0; y < h_out; y++)
  {
    for (int x = 0; x < w_out; x++)
    {
      int val1 = getValueFromSource(srcY1, x, bps_in[0], bigEndian[0]);
      int val2 = getValueFromSource(srcY2, x, bps_in[1], bigEndian[1]);

      // Scale (if neccessary)
      if (bitDepthScaling[0])
        val1 = val1 << depthScale;
      else if (bitDepthScaling[1])
        val2 = val2 << depthScale;
      
      // Calculate the differencec, add mse, (amplify) and clip the difference value
      int diff = val1 - val2;
      mseAdd[0] += diff * diff;
      if (amplification)
        diff *= amplificationFactor;
      diff = clip(diff + diffZero, 0, maxVal);

      setValueInBuffer(dstY, diff, 0, bps_out, true);
      dstY += (bps_out > 8) ? 2 : 1;
    }

    // Goto the next y line
    srcY1 += stride_in[0];
    srcY2 += stride_in[1];
  }

  // Next U/V
  const int strideC_in[2] = {w_in[0] / subH * (bps_in[0] > 8 ? 2 : 1), w_in[1] / subH * (bps_in[1] > 8 ? 2 : 1)};  // How many bytes to the next U/V y line
  for (int y = 0; y < h_out / subV; y++)
  {
    for (int x = 0; x < w_out / subH; x++)
    {
      int valU1 = getValueFromSource(srcU1, x, bps_in[0], bigEndian[0]);
      int valU2 = getValueFromSource(srcU2, x, bps_in[1], bigEndian[1]);
      int valV1 = getValueFromSource(srcV1, x, bps_in[0], bigEndian[0]);
      int valV2 = getValueFromSource(srcV2, x, bps_in[1], bigEndian[1]);

      // Scale (if nevessary)
      if (bitDepthScaling[0])
      {
        valU1 = valU1 << depthScale;
        valV1 = valV1 << depthScale;
      }
      else if (bitDepthScaling[1])
      {
        valU2 = valU2 << depthScale;
        valV2 = valV2 << depthScale;
      }

      // Calculate the differencec, add mse, (amplify) and clip the difference value
      int diffU = valU1 - valU2;
      int diffV = valV1 - valV2;
      mseAdd[1] += diffU * diffU;
      mseAdd[2] += diffV * diffV;
      if (amplification)
      {
        diffU *= amplificationFactor;
        diffU *= amplificationFactor; 
      }
      diffU = clip(diffU + diffZero, 0, maxVal);
      diffV = clip(diffV + diffZero, 0, maxVal);

      setValueInBuffer(dstU, diffU, 0, bps_out, true);
      setValueInBuffer(dstV, diffU, 0, bps_out, true);
      dstU += (bps_out > 8) ? 2 : 1;
      dstV += (bps_out > 8) ? 2 : 1;
    }

    // Goto the next y line
    srcU1 += strideC_in[0];
    srcV1 += strideC_in[0];
    srcU2 += strideC_in[1];
    srcV2 += strideC_in[1];
  }

  // Next we convert the difference YUV image to RGB, either using the normal conversion function or 
  // another function that only marks the difference values.
#if SSE_CONVERSION
  byteArrayAligned tmpDiffBufferRGB;
#else
  QByteArray tmpDiffBufferRGB;
#endif

  yuvPixelFormat tmpDiffYUVFormat(srcPixelFormat.subsampling, bps_out, Order_YUV, true);
  if (markDifference)
    // We don't want to see the actual difference but just where differences are.
    markDifferencesYUVPlanarToRGB(tmpDiffYUV, tmpDiffBufferRGB, QSize(w_out, h_out), tmpDiffYUVFormat);
  else
    // Get the format of the tmpDiffYUV buffer and convert it to RGB
    
    convertYUVPlanarToRGB(tmpDiffYUV, tmpDiffBufferRGB, QSize(w_out, h_out), tmpDiffYUVFormat);

  // Append the conversion information that will be returned
  QStringList yuvSubsamplings = QStringList() << "4:4:4" << "4:2:2" << "4:2:0" << "4:4:0" << "4:1:0" << "4:1:1" << "4:0:0";
  differenceInfoList.append( infoItem("Difference Type",QString("YUV %1").arg(yuvSubsamplings[srcPixelFormat.subsampling])) );
  double mse[4];
  mse[0] = double(mseAdd[0]) / (w_out * h_out);
  mse[1] = double(mseAdd[1]) / (w_out * h_out);
  mse[2] = double(mseAdd[2]) / (w_out * h_out);
  mse[3] = mse[0] + mse[1] + mse[2];
  differenceInfoList.append( infoItem("MSE Y",QString("%1").arg(mse[0])) );
  differenceInfoList.append( infoItem("MSE U",QString("%1").arg(mse[1])) );
  differenceInfoList.append( infoItem("MSE V",QString("%1").arg(mse[2])) );
  differenceInfoList.append( infoItem("MSE All",QString("%1").arg(mse[3])) );

  // Convert the image in tmpDiffBufferRGB to a QPixmap using a QImage intermediate.
  // TODO: Isn't there a faster way to do this? Maybe load a pixmap from "BMP"-like data?
  QImage tmpImage((unsigned char*)tmpDiffBufferRGB.data(), w_out, h_out, QImage::Format_RGB888);
  QPixmap retPixmap;
  retPixmap.convertFromImage(tmpImage);
  return retPixmap;
}

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