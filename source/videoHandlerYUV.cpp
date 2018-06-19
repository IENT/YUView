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

#include "videoHandlerYUV.h"

#include <algorithm>
#include <cstdio>
#include <xmmintrin.h>
#include <QDir>
#include <QPainter>
#include "fileInfoWidget.h"

using namespace YUV_Internals;

// The conversion parameters for the components of the different supported YUV->RGB conversions
// The first index is the index of the ColorConversion enum. The second index is [Y, cRV, cGU, cGV, cBU].
const int yuvRgbConvCoeffs[6][5] =
{
  {76309, 117489, -13975, -34925, 138438}, // BT709_LimitedRange
  {65536, 103206, -12276, -30679, 121608}, // BT709_FullRange
  {76309, 104597, -25675, -53279, 132201}, // BT601_LimitedRange
  {65536,  91881, -22553, -46802, 116129}, // BT601_FullRange
  {76309, 110013, -12276, -42626, 140363}, // BT2020_LimitedRange
  {65536,  96638, -10783, -37444, 123299}  // BT2020_FullRange
};
// Scale a Y value with limited range (16 ... 245) to the full range (0 ... 255) for output.
const int yuvRgbConvScaleLuma[255] = 
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 8, 9, 10, 11, 12, 13, 15, 16, 17, 18, 19, 20, 22, 23, 24, 25, 26, 27, 29, 30, 31, 32, 33, 34, 36, 37, 38, 39, 40, 41, 43, 44, 45, 46, 47, 48, 50, 51, 52, 53, 54, 55, 57, 58, 59, 60, 61, 62, 64, 65, 66, 67, 68, 69, 71, 72, 73, 74, 75, 76, 78, 79, 80, 81, 82, 83, 85, 86, 87, 88, 89, 90, 91, 93, 94, 95, 96, 97, 98, 100, 101, 102, 103, 104, 105, 107, 108, 109, 110, 111, 112, 114, 115, 116, 117, 118, 119, 121, 122, 123, 124, 125, 126, 128, 129, 130, 131, 132, 133, 135, 136, 137, 138, 139, 140, 142, 143, 144, 145, 146, 147, 149, 150, 151, 152, 153, 154, 156, 157, 158, 159, 160, 161, 163, 164, 165, 166, 167, 168, 170, 171, 172, 173, 174, 175, 176, 178, 179, 180, 181, 182, 183, 185, 186, 187, 188, 189, 190, 192, 193, 194, 195, 196, 197, 199, 200, 201, 202, 203, 204, 206, 207, 208, 209, 210, 211, 213, 214, 215, 216, 217, 218, 220, 221, 222, 223, 224, 225, 227, 228, 229, 230, 231, 232, 234, 235, 236, 237, 238, 239, 241, 242, 243, 244, 245, 246, 248, 249, 250, 251, 252, 253, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 };

// Activate this if you want to know when which buffer is loaded/converted to image and so on.
#define VIDEOHANDLERYUV_DEBUG_LOADING 0
#if VIDEOHANDLERYUV_DEBUG_LOADING && !NDEBUG
#include <QDebug>
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
QList<YUVPackingOrder> getSupportedPackingFormats(YUVSubsamplingType subsampling)
{
  if (subsampling == YUV_422)
    return QList<YUVPackingOrder>() << Packing_UYVY << Packing_VYUY << Packing_YUYV << Packing_YVYU;
  if (subsampling == YUV_444)
    return QList<YUVPackingOrder>() << Packing_YUV << Packing_YVU << Packing_AYUV << Packing_YUVA;

  return QList<YUVPackingOrder>();
}

QString getPackingFormatString(YUVPackingOrder packing)
{
  if (packing == Packing_YUV)
    return "YUV";
  if (packing == Packing_YVU)
    return "YVU";
  if (packing == Packing_AYUV)
    return "AYUV";
  if (packing == Packing_YUVA)
    return "YUVA";
  if (packing == Packing_UYVY)
    return "UYVY";
  if (packing == Packing_VYUY)
    return "VYUY";
  if (packing == Packing_YUYV)
    return "YUYV";
  if (packing == Packing_YVYU)
    return "YVYU";
  return "";
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
  else if (subsampling == YUV_400)
    return true;
  return chromaOffset == 0;
}

// Compute the MSE between the given char sources for numPixels bytes
template<typename T>
double computeMSE(T ptr, T ptr2, int numPixels)
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
  yuvPixelFormat::yuvPixelFormat()
  { 
    subsampling = YUV_444; 
    bitsPerSample = -1;
    bigEndian = false;
    planar = false;
    setDefaultChromaOffset();
    planeOrder = Order_YUV;
    uvInterleaved = false;
    packingOrder = Packing_YUV;
    bytePacking = false;
  }  // invalid format

  yuvPixelFormat::yuvPixelFormat(const QString &name)
  {
    QRegExp rxYUVFormat("([YUVA]{3,6}(?:\\(IL\\))?) (4:[420]{1}:[420]{1}) ([0-9]{1,2})-bit[ ]?([BL]{1}E)?[ ]?(packed|packed-B)?[ ]?(Cx[0-9]+)?[ ]?(Cy[0-9]+)?");

    if (rxYUVFormat.exactMatch(name))
    {
      yuvPixelFormat newFormat;

      // Is this a packed format or not?
      QString packed = rxYUVFormat.cap(5);
      newFormat.planar = packed.isEmpty();
      if (!newFormat.planar)
        newFormat.bytePacking = (packed == "packed-B");

      // Parse the YUV order (planar or packed)
      if (newFormat.planar)
      {
        QString yuvName = rxYUVFormat.cap(1);
        if (yuvName.endsWith("(IL)"))
        {
          newFormat.uvInterleaved = true;
          yuvName.chop(4);
        }

        static const QStringList orderNames = QStringList() << "YUV" << "YVU" << "YUVA" << "YVUA";
        int idx = orderNames.indexOf(yuvName);
        if (idx == -1)
          return;
        newFormat.planeOrder = static_cast<YUVPlaneOrder>(idx);
      }
      else
      {
        //static const QStringList orderNames = QStringList() << "YUV" << "YVU" << "AYUV" << "YUVA" << "UYVU" << "VYUY" << "YUYV" << "YVYU" << "YYYYUV" << "YYUYYV" << "UYYVYY" << "VYYUYY";
        static const QStringList orderNames = QStringList() << "YUV" << "YVU" << "AYUV" << "YUVA" << "UYVU" << "VYUY" << "YUYV" << "YVYU";
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
        uvInterleaved = newFormat.uvInterleaved;
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
      if ((packingOrder == Packing_YUV || packingOrder == Packing_YVU || packingOrder == Packing_AYUV || packingOrder == Packing_YUVA) && subsampling != YUV_444)
        return false;
      if ((packingOrder == Packing_UYVY || packingOrder == Packing_VYUY || packingOrder == Packing_YUYV || packingOrder == Packing_YVYU) && subsampling != YUV_422)
        return false;
      if (packingOrder >= Packing_NUM)
        return false;
      /*if ((packingOrder == Packing_YYYYUV || packingOrder == Packing_YYUYYV || packingOrder == Packing_UYYVYY || packingOrder == Packing_VYYUYY) && subsampling == YUV_420)
        return false;*/
      if (subsampling == YUV_420 || subsampling == YUV_440 || subsampling == YUV_410 || subsampling == YUV_411 || subsampling == YUV_400)
        // No support for packed formats with this subsampling (yet)
        return false;
      if (uvInterleaved)
        // This can only be set for planar formats
        return false;
    }
    if (subsampling != YUV_400)
    {
      // There are chroma components. Check the chroma offsets.
      if (chromaOffset[0] < 0 || chromaOffset[0] > getMaxPossibleChromaOffsetValues(true, subsampling))
        return false;
      if (chromaOffset[1] < 0 || chromaOffset[1] > getMaxPossibleChromaOffsetValues(false, subsampling))
        return false;
    }
    // Check the bit depth
    if (bitsPerSample < 7)
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

      if (uvInterleaved)
        name += "(IL)";
    }
    else
      name += getPackingFormatString(packingOrder);

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

    if (!planar && subsampling != YUV_400)
      name += bytePacking ? " packed-B" : " packed";

    // Add the Chroma offsets (if it is not the default offset)
    if (!isDefaultChromaFormat(chromaOffset[0], true, subsampling))
      name += QString(" Cx%1").arg(chromaOffset[0]);
    if (!isDefaultChromaFormat(chromaOffset[1], false, subsampling))
      name += QString(" Cy%1").arg(chromaOffset[1]);

    return name;
  }

  qint64 yuvPixelFormat::bytesPerFrame(const QSize &frameSize) const
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

      if (planar && (planeOrder == Order_YUVA || planeOrder == Order_YVUA))
        // There is an additional alpha plane. The alpha plane is not subsampled
        bytes += frameSize.width() * frameSize.height() * bytesPerSample; // Alpha plane
      if (!planar && subsampling == YUV_444 && (packingOrder == Packing_AYUV || packingOrder == Packing_YUVA))
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
      // This is a packed format. The added number of bytes might be lower because of the packing.
      if (subsampling == YUV_444)
      {
        int bitsPerPixel = bitsPerSample * 3;
        if (packingOrder == Packing_AYUV || packingOrder == Packing_YUVA)
          bitsPerPixel += bitsPerSample;
        return ((bitsPerPixel + 7) / 8) * frameSize.width() * frameSize.height();
      }
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

  videoHandlerYUV_CustomFormatDialog::videoHandlerYUV_CustomFormatDialog(const yuvPixelFormat &yuvFormat)
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
      // Set UV(A) interleaved
      checkBoxUVInterleaved->setChecked(yuvFormat.uvInterleaved);
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
    append(yuvPixelFormat(YUV_420, 8, Order_YUV)); // YUV 4:2:0
    append(yuvPixelFormat(YUV_420, 10, Order_YUV)); // YUV 4:2:0 10 bit
    append(yuvPixelFormat(YUV_422, 8, Order_YUV)); // YUV 4:2:2
    append(yuvPixelFormat(YUV_444, 8, Order_YUV)); // YUV 4:4:4    
  }

  // Put all the names of the YUVFormatList into a list and return it
  QStringList YUVFormatList::getFormattedNames()
  {
    QStringList l;
    for (int i = 0; i < count(); i++)
    {
      l.append(at(i).getName());
    }
    return l;
  }

  void videoHandlerYUV_CustomFormatDialog::on_comboBoxChromaSubsampling_currentIndexChanged(int idx)
  {
    // What packing types are supported?
    YUVSubsamplingType subsampling = static_cast<YUVSubsamplingType>(idx);
    QList<YUVPackingOrder> packingTypes = getSupportedPackingFormats(subsampling);
    comboBoxPackingOrder->clear();
    for (YUVPackingOrder packing : packingTypes)
      comboBoxPackingOrder->addItem(getPackingFormatString(packing));

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
    comboBoxChromaOffsetX->setEnabled(chromaPresent);
    comboBoxChromaOffsetY->setEnabled(chromaPresent);
  }

  void videoHandlerYUV_CustomFormatDialog::on_groupBoxPlanar_toggled(bool checked)
  { 
    if (!checked && !groupBoxPacked->isEnabled())
      // If a packed format is not supported, do not allow the user to activate this
      groupBoxPlanar->setChecked(true); 
    else 
      groupBoxPacked->setChecked(!checked); 
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
      format.uvInterleaved = checkBoxUVInterleaved->isChecked();
    }
    else
    {
      idx = comboBoxPackingOrder->currentIndex();
      int offset = 0;
      if (format.subsampling == YUV_422)
        offset = Packing_UYVY;
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

videoHandlerYUV::videoHandlerYUV() : videoHandler()
{
  // preset internal values
  interpolationMode = NearestNeighborInterpolation;
  componentDisplayMode = DisplayAll;
  yuvColorConversionType = BT709_LimitedRange;

  // Set the default YUV transformation parameters.
  // TODO: Why is the offset 125 for Luma??
  mathParameters[Luma] = yuvMathParameters(1, 125, false);
  mathParameters[Chroma] = yuvMathParameters(1, 128, false);

  // If we know nothing about the YUV format, assume YUV 4:2:0 8 bit planar by default.
  srcPixelFormat = yuvPixelFormat(YUV_420, 8, Order_YUV);

  currentFrameRawYUVData_frameIdx = -1;
  rawYUVData_frameIdx = -1;
  showPixelValuesAsDiff = false;
}

videoHandlerYUV::~videoHandlerYUV()
{
  DEBUG_YUV("videoHandlerYUV destruction");
}

void videoHandlerYUV::loadValues(const QSize &newFramesize, const QString &sourcePixelFormat)
{
  Q_UNUSED(sourcePixelFormat);
  setFrameSize(newFramesize);
}

void videoHandlerYUV::drawFrame(QPainter *painter, int frameIdx, double zoomFactor, bool drawRawData)
{
  QString msg;
  if (!canConvertToRGB(srcPixelFormat, frameSize, &msg))
  {
    // The conversion to RGB can not be performed. Draw a text about this
    msg.prepend("With the given settings, the YUV data can not be converted to RGB:\n");
    // Get the size of the text and create a QRect of that size which is centered at (0,0)
    QFont displayFont = painter->font();
    displayFont.setPointSizeF(painter->font().pointSizeF() * zoomFactor);
    painter->setFont(displayFont);
    QSize textSize = painter->fontMetrics().size(0, msg);
    QRect textRect;
    textRect.setSize(textSize);
    textRect.moveCenter(QPoint(0,0));

    // Draw the text
    painter->drawText(textRect, msg);
  }
  else
    videoHandler::drawFrame(painter, frameIdx, zoomFactor, drawRawData);
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
  ysub  = _mm_set1_epi32(0x00100010); // value 16 for subtraction
  uvsub = _mm_set1_epi32(0x00800080); // value 128

  // multiplication factors bit shifted by 6
  facy  = _mm_set1_epi32(0x004a004a);
  facrv = _mm_set1_epi32(0x00660066);
  facgu = _mm_set1_epi32(0x00190019);
  facgv = _mm_set1_epi32(0x00340034);
  facbu = _mm_set1_epi32(0x00810081);

  zero  = _mm_set1_epi32(0x00000000);

  for(y = 0; y < height; y += 2)
  {
    srcy128r0 = (__m128i *)(yp + sy*y);
    srcy128r1 = (__m128i *)(yp + sy*y + sy);
    srcu64 = (__m64 *)(up + suv*(y/2));
    srcv64 = (__m64 *)(vp + suv*(y/2));

    // dst row 0 and row 1
    dstrgb128r0 = (__m128i *)(rgb + srgb*y);
    dstrgb128r1 = (__m128i *)(rgb + srgb*y + srgb);

    for(x = 0; x < width; x += 16)
    {
      u0 = _mm_loadl_epi64((__m128i *)srcu64); srcu64++;
      v0 = _mm_loadl_epi64((__m128i *)srcv64); srcv64++;

      y0r0 = _mm_load_si128(srcy128r0++);
      y0r1 = _mm_load_si128(srcy128r1++);

      // expand to 16 bit, subtract and multiply constant y factors
      y00r0 = _mm_mullo_epi16(_mm_sub_epi16(_mm_unpacklo_epi8(y0r0, zero), ysub), facy);
      y01r0 = _mm_mullo_epi16(_mm_sub_epi16(_mm_unpackhi_epi8(y0r0, zero), ysub), facy);
      y00r1 = _mm_mullo_epi16(_mm_sub_epi16(_mm_unpacklo_epi8(y0r1, zero), ysub), facy);
      y01r1 = _mm_mullo_epi16(_mm_sub_epi16(_mm_unpackhi_epi8(y0r1, zero), ysub), facy);

      // expand u and v so they're aligned with y values
      u0  = _mm_unpacklo_epi8(u0,  zero);
      u00 = _mm_sub_epi16(_mm_unpacklo_epi16(u0, u0), uvsub);
      u01 = _mm_sub_epi16(_mm_unpackhi_epi16(u0, u0), uvsub);

      v0  = _mm_unpacklo_epi8(v0,  zero);
      v00 = _mm_sub_epi16(_mm_unpacklo_epi16(v0, v0), uvsub);
      v01 = _mm_sub_epi16(_mm_unpackhi_epi16(v0, v0), uvsub);

      // common factors on both rows.
      rv00 = _mm_mullo_epi16(facrv, v00);
      rv01 = _mm_mullo_epi16(facrv, v01);
      gu00 = _mm_mullo_epi16(facgu, u00);
      gu01 = _mm_mullo_epi16(facgu, u01);
      gv00 = _mm_mullo_epi16(facgv, v00);
      gv01 = _mm_mullo_epi16(facgv, v01);
      bu00 = _mm_mullo_epi16(facbu, u00);
      bu01 = _mm_mullo_epi16(facbu, u01);

      // add together and bit shift to the right
      r00 = _mm_srai_epi16(_mm_add_epi16(y00r0, rv00), 6);
      r01 = _mm_srai_epi16(_mm_add_epi16(y01r0, rv01), 6);
      g00 = _mm_srai_epi16(_mm_sub_epi16(_mm_sub_epi16(y00r0, gu00), gv00), 6);
      g01 = _mm_srai_epi16(_mm_sub_epi16(_mm_sub_epi16(y01r0, gu01), gv01), 6);
      b00 = _mm_srai_epi16(_mm_add_epi16(y00r0, bu00), 6);
      b01 = _mm_srai_epi16(_mm_add_epi16(y01r0, bu01), 6);

      r00 = _mm_packus_epi16(r00, r01);
      g00 = _mm_packus_epi16(g00, g01);
      b00 = _mm_packus_epi16(b00, b01);

      // shuffle back together to lower 0rgb0rgb...
      r01     = _mm_unpacklo_epi8(r00,  zero); // 0r0r...
      gbgb    = _mm_unpacklo_epi8(b00,  g00);  // gbgb...
      rgb0123 = _mm_unpacklo_epi16(gbgb, r01);  // lower 0rgb0rgb...
      rgb4567 = _mm_unpackhi_epi16(gbgb, r01);  // upper 0rgb0rgb...

      // shuffle back together to upper 0rgb0rgb...
      r01     = _mm_unpackhi_epi8(r00,  zero);
      gbgb    = _mm_unpackhi_epi8(b00,  g00);
      rgb89ab = _mm_unpacklo_epi16(gbgb, r01);
      rgbcdef = _mm_unpackhi_epi16(gbgb, r01);

      // write to dst
      _mm_store_si128(dstrgb128r0++, rgb0123);
      _mm_store_si128(dstrgb128r0++, rgb4567);
      _mm_store_si128(dstrgb128r0++, rgb89ab);
      _mm_store_si128(dstrgb128r0++, rgbcdef);

      // row 1
      r00 = _mm_srai_epi16(_mm_add_epi16(y00r1, rv00), 6);
      r01 = _mm_srai_epi16(_mm_add_epi16(y01r1, rv01), 6);
      g00 = _mm_srai_epi16(_mm_sub_epi16(_mm_sub_epi16(y00r1, gu00), gv00), 6);
      g01 = _mm_srai_epi16(_mm_sub_epi16(_mm_sub_epi16(y01r1, gu01), gv01), 6);
      b00 = _mm_srai_epi16(_mm_add_epi16(y00r1, bu00), 6);
      b01 = _mm_srai_epi16(_mm_add_epi16(y01r1, bu01), 6);

      r00 = _mm_packus_epi16(r00, r01);
      g00 = _mm_packus_epi16(g00, g01);
      b00 = _mm_packus_epi16(b00, b01);

      r01     = _mm_unpacklo_epi8(r00,  zero);
      gbgb    = _mm_unpacklo_epi8(b00,  g00);
      rgb0123 = _mm_unpacklo_epi16(gbgb, r01);
      rgb4567 = _mm_unpackhi_epi16(gbgb, r01);

      r01     = _mm_unpackhi_epi8(r00,  zero);
      gbgb    = _mm_unpackhi_epi8(b00,  g00);
      rgb89ab = _mm_unpacklo_epi16(gbgb, r01);
      rgbcdef = _mm_unpackhi_epi16(gbgb, r01);

      _mm_store_si128(dstrgb128r1++, rgb0123);
      _mm_store_si128(dstrgb128r1++, rgb4567);
      _mm_store_si128(dstrgb128r1++, rgb89ab);
      _mm_store_si128(dstrgb128r1++, rgbcdef);

    }
  }
}
#endif

QLayout *videoHandlerYUV::createYUVVideoHandlerControls(bool isSizeFixed)
{
  // Absolutely always only call this function once!
  assert(!ui.created());

  QVBoxLayout *newVBoxLayout = nullptr;
  if (!isSizeFixed)
  {
    // Our parent (videoHandler) also has controls to add. Create a new vBoxLayout and append the parent controls
    // and our controls into that layout, separated by a line. Return that layout
    newVBoxLayout = new QVBoxLayout;
    newVBoxLayout->addLayout(frameHandler::createFrameHandlerControls(isSizeFixed));

    QFrame *line = new QFrame;
    line->setObjectName(QStringLiteral("line"));
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    newVBoxLayout->addWidget(line);
  }

  // Create the UI and setup all the controls
  ui.setupUi();

  // Add the preset YUV formats. If the current format is in the list, add it and select it.
  ui.yuvFormatComboBox->addItems(yuvPresetsList.getFormattedNames());
  int idx = yuvPresetsList.indexOf(srcPixelFormat);
  if (idx == -1)
  {
    // The currently set pixel format is not in the presets list. Add and select it.
    ui.yuvFormatComboBox->addItem(srcPixelFormat.getName());
    yuvPresetsList.append(srcPixelFormat);
    idx = yuvPresetsList.indexOf(srcPixelFormat);
  }
  ui.yuvFormatComboBox->setCurrentIndex(idx);
  // Add the custom... entry that allows the user to add custom formats
  ui.yuvFormatComboBox->addItem("Custom...");
  ui.yuvFormatComboBox->setEnabled(!isSizeFixed);

  // Set all the values of the properties widget to the values of this class
  ui.colorComponentsComboBox->addItems(QStringList() << "Y'CbCr" << "Luma (Y) Only" << "Cb only" << "Cr only");
  ui.colorComponentsComboBox->setCurrentIndex((int)componentDisplayMode);
  ui.chromaInterpolationComboBox->addItems(QStringList() << "Nearest neighbor" << "Bilinear");
  ui.chromaInterpolationComboBox->setCurrentIndex((int)interpolationMode);
  ui.chromaInterpolationComboBox->setEnabled(srcPixelFormat.subsampled());
  ui.colorConversionComboBox->addItems(QStringList() << "ITU-R.BT709" << "ITU-R.BT709 Full Range" << "ITU-R.BT601" << "ITU-R.BT601 Full Range" << "ITU-R.BT2020" << "ITU-R.BT2020 Full Range" );
  ui.colorConversionComboBox->setCurrentIndex((int)yuvColorConversionType);
  ui.lumaScaleSpinBox->setValue(mathParameters[Luma].scale);
  ui.lumaOffsetSpinBox->setMaximum(1000);
  ui.lumaOffsetSpinBox->setValue(mathParameters[Luma].offset);
  ui.lumaInvertCheckBox->setChecked(mathParameters[Luma].invert);
  ui.chromaScaleSpinBox->setValue(mathParameters[Chroma].scale);
  ui.chromaOffsetSpinBox->setMaximum(1000);
  ui.chromaOffsetSpinBox->setValue(mathParameters[Chroma].offset);
  ui.chromaInvertCheckBox->setChecked(mathParameters[Chroma].invert);

  // Connect all the change signals from the controls to "connectWidgetSignals()"
  connect(ui.yuvFormatComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &videoHandlerYUV::slotYUVFormatControlChanged);
  connect(ui.colorComponentsComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &videoHandlerYUV::slotYUVControlChanged);
  connect(ui.chromaInterpolationComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &videoHandlerYUV::slotYUVControlChanged);
  connect(ui.colorConversionComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &videoHandlerYUV::slotYUVControlChanged);
  connect(ui.lumaScaleSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &videoHandlerYUV::slotYUVControlChanged);
  connect(ui.lumaOffsetSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &videoHandlerYUV::slotYUVControlChanged);
  connect(ui.lumaInvertCheckBox, &QCheckBox::stateChanged, this, &videoHandlerYUV::slotYUVControlChanged);
  connect(ui.chromaScaleSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &videoHandlerYUV::slotYUVControlChanged);
  connect(ui.chromaOffsetSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &videoHandlerYUV::slotYUVControlChanged);
  connect(ui.chromaInvertCheckBox, &QCheckBox::stateChanged, this, &videoHandlerYUV::slotYUVControlChanged);

  if (!isSizeFixed && newVBoxLayout)
    newVBoxLayout->addLayout(ui.topVBoxLayout);

  return (isSizeFixed) ? ui.topVBoxLayout : newVBoxLayout;
}

void videoHandlerYUV::slotYUVFormatControlChanged(int idx)
{
  yuvPixelFormat newFormat;

  if (idx == yuvPresetsList.count())
  {
    // The user selected the "custom format..." option
    videoHandlerYUV_CustomFormatDialog dialog(srcPixelFormat);
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
        int nrItems = ui.yuvFormatComboBox->count();
        const QSignalBlocker blocker(ui.yuvFormatComboBox);
        ui.yuvFormatComboBox->insertItem(nrItems-1, newFormat.getName());
        // Select the added format
        idx = yuvPresetsList.indexOf(newFormat);
        ui.yuvFormatComboBox->setCurrentIndex(idx);
      }
      else
      {
        // The format is already in the list. Select it without invoking another signal.
        const QSignalBlocker blocker(ui.yuvFormatComboBox);
        ui.yuvFormatComboBox->setCurrentIndex(idx);
      }
    }
    else
    {
      // The user pressed cancel. Go back to the old format
      int idx = yuvPresetsList.indexOf(srcPixelFormat);
      Q_ASSERT(idx != -1);  // The previously selected format should always be in the list
      const QSignalBlocker blocker(ui.yuvFormatComboBox);
      ui.yuvFormatComboBox->setCurrentIndex(idx);
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
  // Store the number bytes per frame of the old pixel format
  qint64 oldFormatBytesPerFrame = srcPixelFormat.bytesPerFrame(frameSize);

  // Set the new pixel format. Lock the mutex, so that no background process is running wile the format changes.
  srcPixelFormat = format;

  if (ui.created())
    // Every time the pixel format changed, see if the interpolation combo box is enabled/disabled
    ui.chromaInterpolationComboBox->setEnabled(format.subsampled());
  
  if (emitSignal)
  {
    // Set the current buffers to be invalid and emit the signal that this item needs to be redrawn.
    currentImageIdx = -1;
    currentImage_frameIndex = -1;

    // Set the cache to invalid until it is cleared an recached
    setCacheInvalid();

    if (srcPixelFormat.bytesPerFrame(frameSize) != oldFormatBytesPerFrame)
      // The number of bytes per frame changed. The raw YUV data buffer is also out of date
      currentFrameRawYUVData_frameIdx = -1;

    // The number of frames in the sequence might have changed as well
    emit signalUpdateFrameLimits();

    emit signalHandlerChanged(true, RECACHE_CLEAR);
  }
}

void videoHandlerYUV::slotYUVControlChanged()
{
  // The control that caused the slot to be called
  QObject *sender = QObject::sender();

  if (sender == ui.colorComponentsComboBox ||
           sender == ui.chromaInterpolationComboBox ||
           sender == ui.colorConversionComboBox ||
           sender == ui.lumaScaleSpinBox ||
           sender == ui.lumaOffsetSpinBox ||
           sender == ui.lumaInvertCheckBox ||
           sender == ui.chromaScaleSpinBox ||
           sender == ui.chromaOffsetSpinBox ||
           sender == ui.chromaInvertCheckBox)
  {
    componentDisplayMode = (ComponentDisplayMode)ui.colorComponentsComboBox->currentIndex();
    interpolationMode = (InterpolationMode)ui.chromaInterpolationComboBox->currentIndex();
    yuvColorConversionType = (ColorConversion)ui.colorConversionComboBox->currentIndex();
    mathParameters[Luma].scale = ui.lumaScaleSpinBox->value();
    mathParameters[Luma].offset = ui.lumaOffsetSpinBox->value();
    mathParameters[Luma].invert = ui.lumaInvertCheckBox->isChecked();
    mathParameters[Chroma].scale = ui.chromaScaleSpinBox->value();
    mathParameters[Chroma].offset = ui.chromaOffsetSpinBox->value();
    mathParameters[Chroma].invert = ui.chromaInvertCheckBox->isChecked();

    // Set the current frame in the buffer to be invalid and clear the cache.
    // Emit that this item needs redraw and the cache needs updating.
    currentImageIdx = -1;
    currentImage_frameIndex = -1;
    setCacheInvalid();
    emit signalHandlerChanged(true, RECACHE_CLEAR);
  }
  else if (sender == ui.yuvFormatComboBox)
  {
    qint64 oldFormatBytesPerFrame = srcPixelFormat.bytesPerFrame(frameSize);

    // Set the new YUV format
    //setSrcPixelFormat(yuvFormatList.getFromName(ui.yuvFormatComboBox->currentText()));

    // Check if the new format changed the number of frames in the sequence
    emit signalUpdateFrameLimits();

    // Set the current frame in the buffer to be invalid and clear the cache.
    // Emit that this item needs redraw and the cache needs updating.
    currentImageIdx = -1;
    currentImage_frameIndex = -1;
    if (srcPixelFormat.bytesPerFrame(frameSize) != oldFormatBytesPerFrame)
      // The number of bytes per frame changed. The raw YUV data buffer also has to be updated.
      currentFrameRawYUVData_frameIdx = -1;
    setCacheInvalid();
    emit signalHandlerChanged(true, RECACHE_CLEAR);
  }
}

/* Get the pixels values so we can show them in the info part of the zoom box.
 * If a second frame handler is provided, the difference values from that item will be returned.
 */
ValuePairList videoHandlerYUV::getPixelValues(const QPoint &pixelPos, int frameIdx, frameHandler *item2, const int frameIdx1)
{
  ValuePairList values;

  if (item2 != nullptr)
  {
    videoHandlerYUV *yuvItem2 = dynamic_cast<videoHandlerYUV*>(item2);
    if (yuvItem2 == nullptr)
      // The given item is not a YUV source. We cannot compare YUV values to non YUV values.
      // Call the base class comparison function to compare the items using the RGB values.
      return frameHandler::getPixelValues(pixelPos, frameIdx, item2, frameIdx1);

    if (srcPixelFormat.bitsPerSample != yuvItem2->srcPixelFormat.bitsPerSample)
      // The two items have different bit depths. Compare RGB values instead.
      // TODO: Or should we do this in the YUV domain somehow?
      return frameHandler::getPixelValues(pixelPos, frameIdx, item2, frameIdx1);

    // Do not get the pixel values if the buffer for the raw YUV values is out of date.
    if (currentFrameRawYUVData_frameIdx != frameIdx || yuvItem2->currentFrameRawYUVData_frameIdx != frameIdx1)
      return ValuePairList();

    int width  = qMin(frameSize.width(), yuvItem2->frameSize.width());
    int height = qMin(frameSize.height(), yuvItem2->frameSize.height());

    if (pixelPos.x() < 0 || pixelPos.x() >= width || pixelPos.y() < 0 || pixelPos.y() >= height)
      return ValuePairList();

    unsigned int Y0, U0, V0, Y1, U1, V1;
    getPixelValue(pixelPos, Y0, U0, V0);
    yuvItem2->getPixelValue(pixelPos, Y1, U1, V1);

    // Append the values to the list
    values.append(ValuePair("Y", QString::number((int)Y0-(int)Y1)));
    if (srcPixelFormat.subsampling != YUV_400)
    {
      values.append(ValuePair("U", QString::number((int)U0-(int)U1)));
      values.append(ValuePair("V", QString::number((int)V0-(int)V1)));
    }
  }
  else
  {
    int width = frameSize.width();
    int height = frameSize.height();

    // Do not get the pixel values if the buffer for the raw YUV values is out of date.
    if (currentFrameRawYUVData_frameIdx != frameIdx)
      return ValuePairList();

    if (pixelPos.x() < 0 || pixelPos.x() >= width || pixelPos.y() < 0 || pixelPos.y() >= height)
      return ValuePairList();

    unsigned int Y,U,V;
    getPixelValue(pixelPos, Y, U, V);

    if (showPixelValuesAsDiff)
    {
      // If 'showPixelValuesAsDiff' is set, this is the zero value
      const int differenceZeroValue = 1 << (srcPixelFormat.bitsPerSample - 1);

      // Append the values to the list
      values.append(ValuePair("Y", QString::number(int(Y)-differenceZeroValue)));
      if (srcPixelFormat.subsampling != YUV_400)
      {
        values.append(ValuePair("U", QString::number(int(U)-differenceZeroValue)));
        values.append(ValuePair("V", QString::number(int(V)-differenceZeroValue)));
      }
    }
    else
    {
      // Append the values to the list
      values.append(ValuePair("Y", QString::number(Y)));
      if (srcPixelFormat.subsampling != YUV_400)
      {
        values.append(ValuePair("U", QString::number(U)));
        values.append(ValuePair("V", QString::number(V)));
      }
    }
  }

  return values;
}

/* Draw the YUV values of the pixels over the actual pixels when zoomed in. The values are drawn at the position where
 * they are assumed. So also chroma shifts and subsampling modes are drawn correctly.
 */
void videoHandlerYUV::drawPixelValues(QPainter *painter, const int frameIdx, const QRect &videoRect, const double zoomFactor, frameHandler *item2, const bool markDifference, const int frameIdxItem1)
{
  // Get the other YUV item (if any)
  videoHandlerYUV *yuvItem2 = (item2 == nullptr) ? nullptr : dynamic_cast<videoHandlerYUV*>(item2);
  if (item2 != nullptr && yuvItem2 == nullptr)
  {
    // The other item is not a yuv item
    frameHandler::drawPixelValues(painter, frameIdx, videoRect, zoomFactor, item2, markDifference, frameIdxItem1);
    return;
  }

  QSize size = frameSize;
  const bool useDiffValues = (yuvItem2 != nullptr);
  if (useDiffValues)
    // If the two items are not of equal size, use the minimum possible size.
    size = QSize(std::min(frameSize.width(), yuvItem2->frameSize.width()), std::min(frameSize.height(), yuvItem2->frameSize.height()));

  // Check if the raw YUV values are up to date. If not, do not draw them. Do not trigger loading of data here. The needsLoadingRawValues 
  // function will return that loading is needed. The caching in the background should then trigger loading of them.
  if (currentFrameRawYUVData_frameIdx != frameIdx)
    return;
  if (yuvItem2 && yuvItem2->currentFrameRawYUVData_frameIdx != frameIdxItem1)
    return;

  // For difference items, we support difference bit depths for the two items.
  // If the bit depth is different, we scale to value with the lower bit depth to the higher bit depth and calculate the difference there.
  // These values are only needed for difference values
  const int bps_in[2] = {srcPixelFormat.bitsPerSample, (useDiffValues) ? yuvItem2->srcPixelFormat.bitsPerSample : 0};
  const int bps_out = std::max(bps_in[0], bps_in[1]);
  // Which of the two input values has to be scaled up? Only one of these (or neither) can be set.
  const bool bitDepthScaling[2] = {bps_in[0] != bps_out, bps_in[1] != bps_out};
  // Scale the input up by this many bits
  const int depthScale = bps_out - (bitDepthScaling[0] ? bps_in[0] : bps_in[1]);
  // What are the maximum and middle value for the output bit depth
  const int diffZero = 128 << (bps_out-8);
  Q_UNUSED(diffZero);

  // First determine which pixels from this item are actually visible, because we only have to draw the pixel values
  // of the pixels that are actually visible
  QRect viewport = painter->viewport();
  QTransform worldTransform = painter->worldTransform();

  int xMin_tmp = (videoRect.width() / 2 - worldTransform.dx()) / zoomFactor;
  int yMin_tmp = (videoRect.height() / 2 - worldTransform.dy()) / zoomFactor;
  int xMax_tmp = (videoRect.width() / 2 - (worldTransform.dx() - viewport.width())) / zoomFactor;
  int yMax_tmp = (videoRect.height() / 2 - (worldTransform.dy() - viewport.height())) / zoomFactor;

  // Clip the min/max visible pixel values to the size of the item (no pixels outside of the
  // item have to be labeled)
  const int xMin = clip(xMin_tmp, 0, size.width()-1);
  const int yMin = clip(yMin_tmp, 0, size.height()-1);
  const int xMax = clip(xMax_tmp, 0, size.width()-1);
  const int yMax = clip(yMax_tmp, 0, size.height()-1);

  // The center point of the pixel (0,0).
  const QPoint centerPointZero = (QPoint(-size.width(), -size.height()) * zoomFactor + QPoint(zoomFactor,zoomFactor)) / 2;
  // This QRect has the size of one pixel and is moved on top of each pixel to draw the text
  QRect pixelRect;
  pixelRect.setSize(QSize(zoomFactor, zoomFactor));

  // We might change the pen doing this so backup the current pen and reset it later
  QPen backupPen = painter->pen();

  // If the Y is below this value, use white text, otherwise black text
  // If there is a second item, a difference will be drawn. A difference of 0 is displayed as gray.
  const int whiteLimit = (yuvItem2) ? 0 : 1 << (srcPixelFormat.bitsPerSample - 1);

  // Are there chroma components?
  const bool chromaPresent = (srcPixelFormat.subsampling != YUV_400);
  // The chroma offset in full luma pixels. This can range from 0 to 3.
  const int chromaOffsetFullX = srcPixelFormat.chromaOffset[0] / 2;
  const int chromaOffsetFullY = srcPixelFormat.chromaOffset[1] / 2;
  // Is the chroma offset by another half luma pixel?
  const bool chromaOffsetHalfX = (srcPixelFormat.chromaOffset[0] % 2 != 0);
  const bool chromaOffsetHalfY = (srcPixelFormat.chromaOffset[1] % 2 != 0);
  // By what factor is X and Y subsampled?
  const int subsamplingX = srcPixelFormat.getSubsamplingHor();
  const int subsamplingY = srcPixelFormat.getSubsamplingVer();

  // If 'showPixelValuesAsDiff' is set, this is the zero value
  const int differenceZeroValue = 1 << (srcPixelFormat.bitsPerSample - 1);

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
        getPixelValue(QPoint(x,y), Y0, U0, V0);
        yuvItem2->getPixelValue(QPoint(x,y), Y1, U1, V1);

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
      else if (showPixelValuesAsDiff)
      {
        unsigned int Yu,Uu,Vu;
        getPixelValue(QPoint(x,y), Yu, Uu, Vu);
        Y = Yu - differenceZeroValue; 
        U = Uu - differenceZeroValue; 
        V = Vu - differenceZeroValue;

        drawWhite = (mathParameters[Luma].invert) ? (Y > 0) : (Y < 0);
      }
      else
      {
        unsigned int Yu,Uu,Vu;
        getPixelValue(QPoint(x,y), Yu, Uu, Vu);
        Y = Yu; U = Uu; V = Vu;
        drawWhite = (mathParameters[Luma].invert) ? (Y > whiteLimit) : (Y < whiteLimit);
      }

      // Set the pen
      painter->setPen(drawWhite ? Qt::white : Qt::black);

      if (chromaPresent && (x-chromaOffsetFullX) % subsamplingX == 0 && (y-chromaOffsetFullY) % subsamplingY == 0)
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

          // Move the QRect by half a pixel
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

void videoHandlerYUV::setFormatFromSizeAndName(const QSize size, int bitDepth, qint64 fileSize, const QFileInfo &fileInfo)
{
  // We are going to check two strings (one after the other) for indicators on the YUV format.
  // 1: The file name, 2: The folder name that the file is contained in.
  QStringList checkStrings;

  // The full name of the file
  QString fileName = fileInfo.fileName();
  if (fileName.isEmpty())
    return;
  checkStrings.append(fileName);

  // The name of the folder that the file is in
  QString dirName = fileInfo.absoluteDir().dirName();
  checkStrings.append(dirName);

  if (fileInfo.suffix() == "nv21")
  {
    // This should be a 8 bit planar yuv 4:2:0 file with interleaved UV components and YVU order
    yuvPixelFormat fmt = yuvPixelFormat(YUV_420, 8, Order_YVU);
    fmt.uvInterleaved = true;
    int bpf = fmt.bytesPerFrame(size);
    if (bpf != 0 && (fileSize % bpf) == 0)
    {
      // Bits per frame and file size match
      setSrcPixelFormat(fmt, false);
      return;
    }
  }

  for (const QString &name : checkStrings)
  {
    // First, lets see if there is a YUV format defined as FFMpeg names them:
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
        for (int bitDepth : bitDepthList)
        {
          QStringList endianessList = QStringList() << "le";
          if (bitDepth > 8)
            endianessList << "be";

          for (const QString &endianess : endianessList)
          {
            QString formatName = planarYUVOrderList[o] + subsamplingNameList[s] + "p";
            if (bitDepth > 8)
              formatName += QString::number(bitDepth) + endianess;

            // Check if this format is in the file name
            if (name.contains(formatName))
            {
              // Check if the format and the file size match
              yuvPixelFormat fmt = yuvPixelFormat(subsampling, bitDepth, planeOrder, endianess=="be");
              int bpf = fmt.bytesPerFrame(size);
              if (bpf != 0 && (fileSize % bpf) == 0)
              {
                // Bits per frame and file size match
                setSrcPixelFormat(fmt, false);
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
      QList<YUVPackingOrder> packingTypes = getSupportedPackingFormats(subsampling);
      for (YUVPackingOrder packing : packingTypes)
      {
        for (int bitDepth : bitDepthList)
        {
          QStringList endianessList = QStringList() << "le";
          if (bitDepth > 8)
            endianessList << "be";

          for (const QString &endianess : endianessList)
          {
            QString formatName = getPackingFormatString(packing).toLower() + subsamplingNameList[s];
            if (bitDepth > 8)
              formatName += QString::number(bitDepth) + endianess;

            // Check if this format is in the file name
            if (name.contains(formatName))
            {
              // Check if the format and the file size match
              yuvPixelFormat fmt = yuvPixelFormat(subsampling, bitDepth, packing, false, endianess=="be");
              int bpf = fmt.bytesPerFrame(size);
              if (bpf != 0 && (fileSize % bpf) == 0)
              {
                // Bits per frame and file size match
                setSrcPixelFormat(fmt, false);
                return;
              }
            }
          }
        }
      }
    }
    // One more FFMpeg format description that does not match the pattern above is: "ayuv64le"
    if (name.contains("ayuv64le"))
    {
      // Check if the format and the file size match
      yuvPixelFormat fmt = yuvPixelFormat(YUV_444, 16, Packing_AYUV, false, false);
      int bpf = fmt.bytesPerFrame(size);
      if (bpf != 0 && (fileSize % bpf) == 0)
      {
        // Bits per frame and file size match
        setSrcPixelFormat(fmt, false);
        return;
      }
    }

    // OK that did not work so far. Try other things. Just check if the file name contains one of the subsampling strings.
    // Further parameters: YUV plane order, little endian. The first format to match the file size, wins.
    QList<int> bitDepths;
    if (bitDepth != -1)
      // We already extracted a bit depth from the name. Only try that.
      bitDepths << bitDepth;
    else
      // We don't know the bit depth. Try different values.
      bitDepths << 9 << 10 << 12 << 14 << 16 << 8;
    for (int s = 0; s < YUV_NUM_SUBSAMPLINGS; s++)
    {
      YUVSubsamplingType subsampling = static_cast<YUVSubsamplingType>(s);
      if (name.contains(subsamplingNameList[s], Qt::CaseInsensitive))
      {
        // Try this subsampling with all bitDepths
        for (int bd : bitDepths)
        {
          yuvPixelFormat fmt = yuvPixelFormat(subsampling, bd, Order_YUV);
          int bpf = fmt.bytesPerFrame(size);
          if (bpf != 0 && (fileSize % bpf) == 0)
          {
            // Bits per frame and file size match
            setSrcPixelFormat(fmt, false);
            return;
          }
        }
      }
    }
  }

  // Nothing using the name worked so far. Check some formats. The first one that matches the file size wins.
  const QList<YUVSubsamplingType> testSubsamplings = QList<YUVSubsamplingType>() << YUV_420 << YUV_444 << YUV_422;

  QList<int> testBitDepths;
  if (bitDepth != -1)
    // We already extracted a bit depth from the name. Only try that.
    testBitDepths << bitDepth;
  else
    // We don't know the bit depth. Try different values.
    testBitDepths << 8 << 9 << 10 << 12 << 14 << 16;

  for (const YUVSubsamplingType &subsampling : testSubsamplings)
  {
    for (int bd : testBitDepths)
    {
      yuvPixelFormat fmt = yuvPixelFormat(subsampling, bd, Order_YUV);
      int bpf = fmt.bytesPerFrame(size);
      if (bpf != 0 && (fileSize % bpf) == 0)
      {
        // Bits per frame and file size match
        setSrcPixelFormat(fmt, false);
        return;
      }
    }
  }
}

/** Try to guess the format of the raw YUV data. A list of candidates is tried (candidateModes) and it is checked if
  * the file size matches and if the correlation of the first two frames is below a threshold.
  * radData must contain at least two frames of the video sequence. Only formats that two frames of could fit into rawData
  * are tested. E.g. the biggest format that is tested here is 1080p YUV 4:2:2 8 bit which is 4147200 bytes per frame. So
  * make sure rawData contains 8294400 bytes so that all formats are tested.
  * If a file size is given, we test if the candidates frame size is a multiple of the fileSize. If fileSize is -1, this test
  * is skipped.
  */
void videoHandlerYUV::setFormatFromCorrelation(const QByteArray &rawYUVData, qint64 fileSize)
{
  if(rawYUVData.size() < 1)
    return;

  class testFormatAndSize
  {
  public:
    testFormatAndSize(const QSize &size, yuvPixelFormat format) : size(size), format(format) { interesting = false; mse = 0; }
    QSize size;
    yuvPixelFormat format;
    bool interesting;
    double mse;
  };

  // The candidates for the size
  const QList<QSize> testSizes = QList<QSize>()
    << QSize(176, 144)
    << QSize(352, 240)
    << QSize(352, 288)
    << QSize(480, 480)
    << QSize(480, 576)
    << QSize(704, 480)
    << QSize(720, 480)
    << QSize(704, 576)
    << QSize(720, 576)
    << QSize(1024, 768)
    << QSize(1280, 720)
    << QSize(1280, 960)
    << QSize(1920, 1072)
    << QSize(1920, 1080);

  // Test bit depths 8, 10 and 16
  QList<testFormatAndSize> formatList;
  for (int b = 0; b < 3; b++)
  {
    int bits = (b==0) ? 8 : (b==1) ? 10 : 16;
    // Test all subsampling modes
    for (int i = 0; i < YUV_NUM_SUBSAMPLINGS; i++)
    {
      YUVSubsamplingType subsampling = static_cast<YUVSubsamplingType>(i);
      for (const QSize &size : testSizes)
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

    for (testFormatAndSize &testFormat : formatList)
    {
      qint64 picSize = testFormat.format.bytesPerFrame(testFormat.size);

      if(fileSize >= (picSize*2))       // at least 2 pictures for correlation analysis
      {
        if((fileSize % picSize) == 0)   // important: file size must be multiple of the picture size
        {
          testFormat.interesting = true;  // test passed
          found = true;
        }
      }
    }

    if(!found)
      // No candidate matches the file size
      return;
  }

  // calculate max. correlation for first two frames, use max. candidate frame size
  for (testFormatAndSize &testFormat : formatList)
  {
    if (testFormat.interesting)
    {
      qint64 picSize = testFormat.format.bytesPerFrame(testFormat.size);
      int lumaSamples = testFormat.size.width() * testFormat.size.height();

      // Calculate the MSE for 2 frames
      if (testFormat.format.bitsPerSample == 8)
      {
        unsigned char *ptr = (unsigned char*) rawYUVData.data();
        testFormat.mse = computeMSE(ptr, ptr + picSize, lumaSamples);
      }
      else if (testFormat.format.bitsPerSample > 8 && testFormat.format.bitsPerSample <= 16)
      {
        unsigned short *ptr = (unsigned short*) rawYUVData.data();
        testFormat.mse = computeMSE(ptr, ptr + picSize/2, lumaSamples);
      }
      else
        // Not handled here
        continue;
    }
  }

  // step3: select best candidate
  double leastMSE = std::numeric_limits<double>::max(); // large error...
  yuvPixelFormat bestFormat;
  QSize bestSize;
  for (const testFormatAndSize &testFormat : formatList)
  {
    if (testFormat.interesting && testFormat.mse < leastMSE)
    {
      bestFormat = testFormat.format;
      bestSize = testFormat.size;
      leastMSE = testFormat.mse;
    }
  }

  if(leastMSE < 400)
  {
    // MSE is below threshold. Choose the candidate.
    setSrcPixelFormat(bestFormat, false);
    setFrameSize(bestSize);
  }
}

void videoHandlerYUV::loadFrame(int frameIndex, bool loadToDoubleBuffer)
{
  DEBUG_YUV("videoHandlerYUV::loadFrame %d\n", frameIndex);

  if (!isFormatValid())
    // We cannot load a frame if the format is not known
    return;

  // Does the data in currentFrameRawYUVData need to be updated?
  if (!loadRawYUVData(frameIndex))
    // Loading failed or it is still being performed in the background
    return;

  // The data in currentFrameRawYUVData is now up to date. If necessary
  // convert the data to RGB.
  if (loadToDoubleBuffer)
  {
    QImage newImage;
    convertYUVToImage(currentFrameRawYUVData, newImage, srcPixelFormat, frameSize);
    doubleBufferImage = newImage;
    doubleBufferImageFrameIdx = frameIndex;
  }
  else if (currentImageIdx != frameIndex)
  {
    QImage newImage;
    convertYUVToImage(currentFrameRawYUVData, newImage, srcPixelFormat, frameSize);
    QMutexLocker setLock(&currentImageSetMutex);    
    currentImage = newImage;
    currentImageIdx = frameIndex;
  }
}

void videoHandlerYUV::loadFrameForCaching(int frameIndex, QImage &frameToCache)
{
  DEBUG_YUV("videoHandlerYUV::loadFrameForCaching %d", frameIndex);

  // Get the YUV format and the size here, so that the caching process does not crash if this changes.
  yuvPixelFormat yuvFormat = srcPixelFormat;
  const QSize curFrameSize = frameSize;

  requestDataMutex.lock();
  emit signalRequestRawData(frameIndex, true);
  QByteArray tmpBufferRawYUVDataCaching = rawYUVData;
  requestDataMutex.unlock();

  if (frameIndex != rawYUVData_frameIdx)
  {
    // Loading failed
    DEBUG_YUV("videoHandlerYUV::loadFrameForCaching Loading failed");
    return;
  }

  // Convert YUV to image. This can then be cached.
  convertYUVToImage(tmpBufferRawYUVDataCaching, frameToCache, yuvFormat, curFrameSize);
}

// Load the raw YUV data for the given frame index into currentFrameRawYUVData.
bool videoHandlerYUV::loadRawYUVData(int frameIndex)
{
  if (currentFrameRawYUVData_frameIdx == frameIndex)
    // Buffer already up to date
    return true;

  DEBUG_YUV("videoHandlerYUV::loadRawYUVData %d", frameIndex);

  // The function loadFrameForCaching also uses the signalRequesRawYUVData to request raw data.
  // However, only one thread can use this at a time.
  requestDataMutex.lock();
  emit signalRequestRawData(frameIndex, false);

  if (frameIndex != rawYUVData_frameIdx)
  {
    // Loading failed
    DEBUG_YUV("videoHandlerYUV::loadRawYUVData Loading failed");
    requestDataMutex.unlock();
    return false;
  }

  currentFrameRawYUVData = rawYUVData;
  currentFrameRawYUVData_frameIdx = frameIndex;
  requestDataMutex.unlock();
  
  DEBUG_YUV("videoHandlerYUV::loadRawYUVData %d Done", frameIndex);
  return true;
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

inline void convertYUVToRGB8Bit(const unsigned int valY, const unsigned int valU, const unsigned int valV, int &valR, int &valG, int &valB, const int RGBConv[5], const bool fullRange, const int bps)
{
  if (bps > 14)
  {
    // The bit depth of an int (32) is not enough to perform a YUV -> RGB conversion for a bit depth > 14 bits.
    // We could use 64 bit values but for what? We are clipping the result to 8 bit anyways so let's just
    // get rid of 2 of the bits for the YUV values.
    const int yOffset = (fullRange ? 0 : 16<<(bps-10));
    const int cZero = 128<<(bps-10);

    const int Y_tmp = ((valY >> 2) - yOffset) * RGBConv[0];
    const int U_tmp = (valU >> 2) - cZero;
    const int V_tmp = (valV >> 2) - cZero;

    const int R_tmp = (Y_tmp                      + V_tmp * RGBConv[1]) >> (16 + bps - 10); //32 to 16 bit conversion by right shifting
    const int G_tmp = (Y_tmp + U_tmp * RGBConv[2] + V_tmp * RGBConv[3]) >> (16 + bps - 10);
    const int B_tmp = (Y_tmp + U_tmp * RGBConv[4]                     ) >> (16 + bps - 10);

    valR = (R_tmp < 0) ? 0 : (R_tmp > 255) ? 255 : R_tmp;
    valG = (G_tmp < 0) ? 0 : (G_tmp > 255) ? 255 : G_tmp;
    valB = (B_tmp < 0) ? 0 : (B_tmp > 255) ? 255 : B_tmp;
  }
  else
  {
    const int yOffset = (fullRange ? 0 : 16<<(bps-8));
    const int cZero = 128<<(bps-8);

    const int Y_tmp = (valY - yOffset) * RGBConv[0];
    const int U_tmp = valU - cZero;
    const int V_tmp = valV - cZero;

    const int R_tmp = (Y_tmp                      + V_tmp * RGBConv[1]) >> (16 + bps - 8); //32 to 16 bit conversion by right shifting
    const int G_tmp = (Y_tmp + U_tmp * RGBConv[2] + V_tmp * RGBConv[3]) >> (16 + bps - 8);
    const int B_tmp = (Y_tmp + U_tmp * RGBConv[4]                     ) >> (16 + bps - 8);

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
    dst[idx] = val;
}

// For every input sample in src, apply YUV transformation, (scale to 8 bit if required) and set the value as RGB (monochrome).
// inValSkip: skip this many values in the input for every value. For pure planar formats, this 1. If the UV components are interleaved, this is 2 or 3.
inline void YUVPlaneToRGBMonochrome_444(const int componentSize, const yuvMathParameters math, const unsigned char * restrict src, unsigned char * restrict dst,
                                        const int inMax, const int bps, const bool bigEndian, const int inValSkip, const bool fullRange)
{
  const bool applyMath = math.yuvMathRequired();
  const int shiftTo8Bit = bps - 8;
  for (int i = 0; i < componentSize; ++i)
  {
    int newVal = getValueFromSource(src, i*inValSkip, bps, bigEndian);
    if (applyMath)
      newVal = transformYUV(math.invert, math.scale, math.offset, newVal, inMax);

    // Scale to 8 bit (if required)
    if (shiftTo8Bit > 0)
      newVal = clip8Bit(newVal >> shiftTo8Bit);
    if (!fullRange)
    {
      assert(newVal >= 0 && newVal <= 255);
      newVal = yuvRgbConvScaleLuma[newVal];
    }

    // Set the value for R, G and B (BGRA)
    dst[i*4  ] = (unsigned char)newVal;
    dst[i*4+1] = (unsigned char)newVal;
    dst[i*4+2] = (unsigned char)newVal;
    dst[i*4+3] = (unsigned char)255;
  }
}

// For every input sample in the YZV 422 src, apply interpolation (sample and hold), apply YUV transformation, (scale to 8 bit if required)
// and set the value as RGB (monochrome).
inline void YUVPlaneToRGBMonochrome_422(const int componentSize, const yuvMathParameters math, const unsigned char * restrict src, unsigned char * restrict dst,
                                        const int inMax, const int bps, const bool bigEndian, const int inValSkip, const bool fullRange)
{
  const bool applyMath = math.yuvMathRequired();
  const int shiftTo8Bit = bps - 8;
  for (int i = 0; i < componentSize; ++i)
  {
    int newVal = getValueFromSource(src, i*inValSkip, bps, bigEndian);
    if (applyMath)
      newVal = transformYUV(math.invert, math.scale, math.offset, newVal, inMax);

    // Scale and clip to 8 bit
    if (shiftTo8Bit > 0)
      newVal = clip8Bit(newVal >> shiftTo8Bit);
    if (!fullRange)
    {
      assert(newVal >= 0 && newVal <= 255);
      newVal = yuvRgbConvScaleLuma[newVal];
    }
    // Set the value for R, G and B of 2 pixels (BGRA)
    dst[i*8  ] = (unsigned char)newVal;
    dst[i*8+1] = (unsigned char)newVal;
    dst[i*8+2] = (unsigned char)newVal;
    dst[i*8+3] = (unsigned char)255;
    dst[i*8+4] = (unsigned char)newVal;
    dst[i*8+5] = (unsigned char)newVal;
    dst[i*8+6] = (unsigned char)newVal;
    dst[i*8+7] = (unsigned char)255;
  }
}

inline void YUVPlaneToRGBMonochrome_420(const int w, const int h, const yuvMathParameters math, const unsigned char * restrict src, unsigned char * restrict dst,
                                        const int inMax, const int bps, const bool bigEndian, const int inValSkip, const bool fullRange)
{
  const bool applyMath = math.yuvMathRequired();
  const int shiftTo8Bit = bps - 8;
  for (int y = 0; y < h/2; y++)
    for (int x = 0; x < w/2; x++)
    {
      const int srcIdx = y*(w/2)+x;
      int newVal = getValueFromSource(src, srcIdx*inValSkip, bps, bigEndian);
      if (applyMath)
        newVal = transformYUV(math.invert, math.scale, math.offset, newVal, inMax);

      // Scale and clip to 8 bit
      if (shiftTo8Bit > 0)
        newVal = clip8Bit(newVal >> shiftTo8Bit);
      if (!fullRange)
      {
        assert(newVal >= 0 && newVal <= 255);
        newVal = yuvRgbConvScaleLuma[newVal];
      }
      // Set the value for R, G and B of 4 pixels (BGRA)
      int o = (y*2*w + x*2)*4;
      dst[o  ] = (unsigned char)newVal;
      dst[o+1] = (unsigned char)newVal;
      dst[o+2] = (unsigned char)newVal;
      dst[o+3] = (unsigned char)255;
      dst[o+4] = (unsigned char)newVal;
      dst[o+5] = (unsigned char)newVal;
      dst[o+6] = (unsigned char)newVal;
      dst[o+7] = (unsigned char)255;
      o += w*4;   // Goto next line
      dst[o  ] = (unsigned char)newVal;
      dst[o+1] = (unsigned char)newVal;
      dst[o+2] = (unsigned char)newVal;
      dst[o+3] = (unsigned char)255;
      dst[o+4] = (unsigned char)newVal;
      dst[o+5] = (unsigned char)newVal;
      dst[o+6] = (unsigned char)newVal;
      dst[o+7] = (unsigned char)255;
    }
}

inline void YUVPlaneToRGBMonochrome_440(const int w, const int h, const yuvMathParameters math, const unsigned char * restrict src, unsigned char * restrict dst,
                                        const int inMax, const int bps, const bool bigEndian, const int inValSkip, const bool fullRange)
{
  const bool applyMath = math.yuvMathRequired();
  const int shiftTo8Bit = bps - 8;
  for (int y = 0; y < h/2; y++)
    for (int x = 0; x < w; x++)
    {
      const int srcIdx = y*w+x;
      int newVal = getValueFromSource(src, srcIdx*inValSkip, bps, bigEndian);
      if (applyMath)
        newVal = transformYUV(math.invert, math.scale, math.offset, newVal, inMax);

      // Scale and clip to 8 bit
      if (shiftTo8Bit > 0)
        newVal = clip8Bit(newVal >> shiftTo8Bit);
      if (!fullRange)
      {
        assert(newVal >= 0 && newVal <= 255);
        newVal = yuvRgbConvScaleLuma[newVal];
      }
      // Set the value for R, G and B of 2 pixels (BGRA)
      const int pos1 = (y*2*w+x)*4;
      const int pos2 = pos1 + w*4;  // Next line
      dst[pos1  ] = (unsigned char)newVal;
      dst[pos1+1] = (unsigned char)newVal;
      dst[pos1+2] = (unsigned char)newVal;
      dst[pos1+3] = (unsigned char)255;
      dst[pos2  ] = (unsigned char)newVal;
      dst[pos2+1] = (unsigned char)newVal;
      dst[pos2+2] = (unsigned char)newVal;
      dst[pos2+3] = (unsigned char)255;
    }
}

inline void YUVPlaneToRGBMonochrome_410(const int w, const int h, const yuvMathParameters math, const unsigned char * restrict src, unsigned char * restrict dst,
  const int inMax, const int bps, const bool bigEndian, const int inValSkip, const bool fullRange)
{
  // Horizontal subsampling by 4, vertical subsampling by 4
  const bool applyMath = math.yuvMathRequired();
  const int shiftTo8Bit = bps - 8;
  for (int y = 0; y < h/4; y++)
    for (int x = 0; x < w/4; x++)
    {
      const int srcIdx = y*(w/4)+x;
      int newVal = getValueFromSource(src, srcIdx*inValSkip, bps, bigEndian);

      if (applyMath)
        newVal = transformYUV(math.invert, math.scale, math.offset, newVal, inMax);

      // Scale and clip to 8 bit
      if (shiftTo8Bit > 0)
        newVal = clip8Bit(newVal >> shiftTo8Bit);
      if (!fullRange)
      {
        assert(newVal >= 0 && newVal <= 255);
        newVal = yuvRgbConvScaleLuma[newVal];
      }
      // Set the value as RGB for 4 pixels in this line and the next 3 lines (BGRA)
      for (int yo = 0; yo < 4; yo++)
        for (int xo = 0; xo < 4; xo++)
        {
          const int pos = ((y*4+yo)*w+(x*4+xo))*4;
          dst[pos  ] = (unsigned char)newVal;
          dst[pos+1] = (unsigned char)newVal;
          dst[pos+2] = (unsigned char)newVal;
          dst[pos+3] = (unsigned char)255;
        }
    }
}

inline void YUVPlaneToRGBMonochrome_411(const int componentSize, const yuvMathParameters math, const unsigned char * restrict src, unsigned char * restrict dst,
                                        const int inMax, const int bps, const bool bigEndian, const int inValSkip, const bool fullRange)
{
  // Horizontally U and V are subsampled by 4
  const bool applyMath = math.yuvMathRequired();
  const int shiftTo8Bit = bps - 8;
  for (int i = 0; i < componentSize; ++i)
  {
    int newVal = getValueFromSource(src, i*inValSkip, bps, bigEndian);
    if (applyMath)
      newVal = transformYUV(math.invert, math.scale, math.offset, newVal, inMax);

    // Scale and clip to 8 bit
    if (shiftTo8Bit > 0)
      newVal = clip8Bit(newVal >> shiftTo8Bit);
    if (!fullRange)
    {
      assert(newVal >= 0 && newVal <= 255);
      newVal = yuvRgbConvScaleLuma[newVal];
    }
    // Set the value for R, G and B of 4 pixels (BGRA)
    dst[i*16   ] = (unsigned char)newVal;
    dst[i*16+1 ] = (unsigned char)newVal;
    dst[i*16+2 ] = (unsigned char)newVal;
    dst[i*16+3 ] = (unsigned char)255;
    dst[i*16+4 ] = (unsigned char)newVal;
    dst[i*16+5 ] = (unsigned char)newVal;
    dst[i*16+6 ] = (unsigned char)newVal;
    dst[i*16+7 ] = (unsigned char)255;
    dst[i*16+8 ] = (unsigned char)newVal;
    dst[i*16+9 ] = (unsigned char)newVal;
    dst[i*16+10] = (unsigned char)newVal;
    dst[i*16+11] = (unsigned char)255;
    dst[i*16+12] = (unsigned char)newVal;
    dst[i*16+13] = (unsigned char)newVal;
    dst[i*16+14] = (unsigned char)newVal;
    dst[i*16+15] = (unsigned char)255;
  }
}

inline int interpolateUVSample(const InterpolationMode mode, const int sample1, const int sample2)
{
  if (mode == BiLinearInterpolation)
    // Interpolate linearly between sample1 and sample2
    return ((sample1 + sample2) + 1) >> 1;
  return sample1; // Sample and hold
}

inline int interpolateUVSampleQ(const InterpolationMode mode, const int sample1, const int sample2, const int quarterPos)
{
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
  return sample1; // Sample and hold
}

// TODO: Consider sample position
inline int interpolateUVSample2D(const InterpolationMode mode, const int sample1, const int sample2, const int sample3, const int sample4)
{
  if (mode == BiLinearInterpolation)
    // Interpolate linearly between sample1 - sample 4
    return ((sample1 + sample2 + sample3 + sample4) + 2) >> 2;
  return sample1;   // Sample and hold
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
  Q_ASSERT(false); // offsetX8 should always be between 1 and 7 (inclusive)
  return 0;
}

// Re-sample the chroma component so that the chroma samples and the luma samples are aligned after this operation.
inline void UVPlaneResamplingChromaOffset(const yuvPixelFormat format, const int w, const int h, 
                                          const unsigned char * restrict srcU, const unsigned char * restrict srcV, const int inValSkip,
                                          unsigned char * restrict dstU, unsigned char * restrict dstV)
{
  // We can perform linear interpolation for 7 positions (6 in between) two pixels.
  // Which of these position is needed depends on the chromaOffset and the subsampling.
  const int possibleValsX = getMaxPossibleChromaOffsetValues(true,  format.subsampling);
  const int possibleValsY = getMaxPossibleChromaOffsetValues(false, format.subsampling);
  const int offsetX8 = (possibleValsX == 1) ? format.chromaOffset[0] * 4 : (possibleValsX == 3) ? format.chromaOffset[0] * 2 : format.chromaOffset[0];
  const int offsetY8 = (possibleValsY == 1) ? format.chromaOffset[1] * 4 : (possibleValsY == 3) ? format.chromaOffset[1] * 2 : format.chromaOffset[1];

  // The format to use for input/output
  const bool bigEndian = format.bigEndian;
  const int bps = format.bitsPerSample;

  const int stride = bps > 8 ? w*2 : w;
  if (offsetX8 != 0)
  {
    // Perform horizontal re-sampling
    for (int y = 0; y < h; y++)
    {
      // On the left side, there is no previous sample, so the first value is never changed.
      const int srcIdx = y * stride * inValSkip;
      int prevU = getValueFromSource(srcU, srcIdx, bps, bigEndian);
      int prevV = getValueFromSource(srcV, srcIdx, bps, bigEndian);
      setValueInBuffer(dstU, prevU, y*stride, bps, bigEndian);
      setValueInBuffer(dstV, prevV, y*stride, bps, bigEndian);

      for (int x = 0; x < w-1; x++)
      {
        // Calculate the new current value using the previous and the current value
        const int srcIdxInLine = srcIdx + (x+1)*inValSkip;
        int curU = getValueFromSource(srcU, srcIdxInLine, bps, bigEndian);
        int curV = getValueFromSource(srcV, srcIdxInLine, bps, bigEndian);

        // Perform interpolation and save the value for the current UV value. Goto next value.
        int newU = interpolateUV8Pos(prevU, curU, offsetX8);
        int newV = interpolateUV8Pos(prevV, curV, offsetX8);
        setValueInBuffer(dstU, newU, y*stride+x, bps, bigEndian);
        setValueInBuffer(dstV, newV, y*stride+x, bps, bigEndian);

        prevU = curU;
        prevV = curV;
      }
    }
  }

  // For the second step, use the filtered values (or the source if no filtering was applied)
  const unsigned char *srcUStep2 = (offsetX8 == 0) ? srcU : dstU;
  const unsigned char *srcVStep2 = (offsetX8 == 0) ? srcV : dstV;
  const int valSkipStep2 = (offsetX8 == 0) ? inValSkip : 1;

  if (offsetY8 != 0)
  {
    // Perform vertical re-sampling. It works exactly like horizontal up-sampling but x and y are switched.
    for (int x = 0; x < w; x++)
    {
      // On the top, there is no previous sample, so the first value is never changed.
      int prevU = getValueFromSource(srcUStep2, x*valSkipStep2, bps, bigEndian);
      int prevV = getValueFromSource(srcVStep2, x*valSkipStep2, bps, bigEndian);
      setValueInBuffer(dstU, prevU, x, bps, bigEndian);
      setValueInBuffer(dstV, prevV, x, bps, bigEndian);

      for (int y = 0; y < h-1; y++)
      {
        // Calculate the new current value using the previous and the current value
        const int srcIdx = (y+1) * w + x;
        int curU = getValueFromSource(srcUStep2, srcIdx*valSkipStep2, bps, bigEndian);
        int curV = getValueFromSource(srcVStep2, srcIdx*valSkipStep2, bps, bigEndian);

        // Perform interpolation and save the value for the current UV value. Goto next value.
        int newU = interpolateUV8Pos(prevU, curU, offsetY8);
        int newV = interpolateUV8Pos(prevV, curV, offsetY8);
        setValueInBuffer(dstU, newU, srcIdx, bps, bigEndian);
        setValueInBuffer(dstV, newV, srcIdx, bps, bigEndian);

        prevU = curU;
        prevV = curV;
      }
    }
  }
}

inline void YUVPlaneToRGB_444(const int componentSize, const yuvMathParameters mathY, const yuvMathParameters mathC,
                              const unsigned char * restrict srcY, const unsigned char * restrict srcU, const unsigned char * restrict srcV,
                              unsigned char * restrict dst, const int RGBConv[5], const bool fullRange, const int inMax, const int bps, const bool bigEndian, const int inValSkip)
{
  const bool applyMathLuma = mathY.yuvMathRequired();
  const bool applyMathChroma = mathC.yuvMathRequired();

  for (int i = 0; i < componentSize; ++i)
  {
    unsigned int valY = getValueFromSource(srcY, i, bps, bigEndian);
    unsigned int valU = getValueFromSource(srcU, i*inValSkip, bps, bigEndian);
    unsigned int valV = getValueFromSource(srcV, i*inValSkip, bps, bigEndian);

    if (applyMathLuma)
      valY = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY, inMax);
    if (applyMathChroma)
    {
      valU = transformYUV(mathC.invert, mathC.scale, mathC.offset, valU, inMax);
      valV = transformYUV(mathC.invert, mathC.scale, mathC.offset, valV, inMax);
    }

    // Get the RGB values for this sample
    int valR, valG, valB;
    convertYUVToRGB8Bit(valY, valU, valV, valR, valG, valB, RGBConv, fullRange, bps);

    // Save the RGB values
    dst[i*4  ] = valB;
    dst[i*4+1] = valG;
    dst[i*4+2] = valR;
    dst[i*4+3] = 255;
  }
}

inline void YUVPlaneToRGB_422(const int w, const int h, const yuvMathParameters mathY, const yuvMathParameters mathC,
                              const unsigned char * restrict srcY, const unsigned char * restrict srcU, const unsigned char * restrict srcV,
                              unsigned char * restrict dst, const int RGBConv[5], const bool fullRange, const int inMax, const InterpolationMode interpolation, const int bps, const bool bigEndian, const int inValSkip)
{
  const bool applyMathLuma = mathY.yuvMathRequired();
  const bool applyMathChroma = mathC.yuvMathRequired();
  // Horizontal up-sampling is required. Process two Y values at a time
  for (int y = 0; y < h; y++)
  {
    const int srcIdxUV = y*w/2;
    int curUSample = getValueFromSource(srcU, srcIdxUV*inValSkip, bps, bigEndian);
    int curVSample = getValueFromSource(srcV, srcIdxUV*inValSkip, bps, bigEndian);
    if (applyMathChroma)
    {
      curUSample = transformYUV(mathC.invert, mathC.scale, mathC.offset, curUSample, inMax);
      curVSample = transformYUV(mathC.invert, mathC.scale, mathC.offset, curVSample, inMax);
    }

    for (int x = 0; x < (w/2)-1; x++)
    {
      // Get the next U/V sample
      const int srcPosLineUV = srcIdxUV + x + 1;
      int nextUSample = getValueFromSource(srcU, srcPosLineUV*inValSkip, bps, bigEndian);
      int nextVSample = getValueFromSource(srcV, srcPosLineUV*inValSkip, bps, bigEndian);
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

      // Convert to 2 RGB values and save them (BGRA)
      int valR1, valR2, valG1, valG2, valB1, valB2;
      convertYUVToRGB8Bit(valY1, curUSample   , curVSample   , valR1, valG1, valB1, RGBConv, fullRange, bps);
      convertYUVToRGB8Bit(valY2, interpolatedU, interpolatedV, valR2, valG2, valB2, RGBConv, fullRange, bps);
      const int pos = (y*w+x*2)*4;
      dst[pos  ] = valB1;
      dst[pos+1] = valG1;
      dst[pos+2] = valR1;
      dst[pos+3] = 255;
      dst[pos+4] = valB2;
      dst[pos+5] = valG2;
      dst[pos+6] = valR2;
      dst[pos+7] = 255;

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
    convertYUVToRGB8Bit(valY1, curUSample, curVSample, valR1, valG1, valB1, RGBConv, fullRange, bps);
    convertYUVToRGB8Bit(valY2, curUSample, curVSample, valR2, valG2, valB2, RGBConv, fullRange, bps);
    const int pos = ((y+1)*w)*4;
    dst[pos-8] = valB1;
    dst[pos-7] = valG1;
    dst[pos-6] = valR1;
    dst[pos-5] = 255;
    dst[pos-4] = valB2;
    dst[pos-3] = valG2;
    dst[pos-2] = valR2;
    dst[pos-1] = 255;
  }
}

inline void YUVPlaneToRGB_440(const int w, const int h, const yuvMathParameters mathY, const yuvMathParameters mathC,
                              const unsigned char * restrict srcY, const unsigned char * restrict srcU, const unsigned char * restrict srcV,
                              unsigned char * restrict dst, const int RGBConv[5], const bool fullRange,const int inMax, const InterpolationMode interpolation, const int bps, const bool bigEndian, const int inValSkip)
{
  const bool applyMathLuma = mathY.yuvMathRequired();
  const bool applyMathChroma = mathC.yuvMathRequired();
  // Vertical up-sampling is required. Process two Y values at a time

  for (int x = 0; x < w; x++)
  {
    int curUSample = getValueFromSource(srcU, x*inValSkip, bps, bigEndian);
    int curVSample = getValueFromSource(srcV, x*inValSkip, bps, bigEndian);
    if (applyMathChroma)
    {
      curUSample = transformYUV(mathC.invert, mathC.scale, mathC.offset, curUSample, inMax);
      curVSample = transformYUV(mathC.invert, mathC.scale, mathC.offset, curVSample, inMax);
    }

    for (int y = 0; y < (h/2)-1; y++)
    {
      // Get the next U/V sample
      const int srcIdxUV = y*w+x;
      int nextUSample = getValueFromSource(srcU, srcIdxUV*inValSkip, bps, bigEndian);
      int nextVSample = getValueFromSource(srcV, srcIdxUV*inValSkip, bps, bigEndian);
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
      convertYUVToRGB8Bit(valY1, curUSample   , curVSample   , valR1, valG1, valB1, RGBConv, fullRange, bps);
      convertYUVToRGB8Bit(valY2, interpolatedU, interpolatedV, valR2, valG2, valB2, RGBConv, fullRange, bps);
      const int pos1 = (y*2*w+x)*4;
      const int pos2 = pos1 + 4*w;
      dst[pos1  ] = valB1;
      dst[pos1+1] = valG1;
      dst[pos1+2] = valR1;
      dst[pos1+3] = 255;
      dst[pos2  ] = valB2;
      dst[pos2+1] = valG2;
      dst[pos2+2] = valR2;
      dst[pos2+3] = 255;

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
    convertYUVToRGB8Bit(valY1, curUSample, curVSample, valR1, valG1, valB1, RGBConv, fullRange, bps);
    convertYUVToRGB8Bit(valY2, curUSample, curVSample, valR2, valG2, valB2, RGBConv, fullRange, bps);
    const int pos1 = ((h-2)*w+x)*4;
    const int pos2 = pos1 + w*4;
    dst[pos1  ] = valB1;
    dst[pos1+1] = valG1;
    dst[pos1+2] = valR1;
    dst[pos1+3] = 255;
    dst[pos2  ] = valB2;
    dst[pos2+1] = valG2;
    dst[pos2+2] = valR2;
    dst[pos2+3] = 255;
  }
}

inline void YUVPlaneToRGB_420(const int w, const int h, const yuvMathParameters mathY, const yuvMathParameters mathC,
                              const unsigned char * restrict srcY, const unsigned char * restrict srcU, const unsigned char * restrict srcV,
                              unsigned char * restrict dst, const int RGBConv[5], const bool fullRange,const int inMax, const InterpolationMode interpolation, const int bps, const bool bigEndian, const int inValSkip)
{
  const bool applyMathLuma = mathY.yuvMathRequired();
  const bool applyMathChroma = mathC.yuvMathRequired();
  // Format is YUV 4:2:0. Horizontal and vertical up-sampling is required. Process 4 Y positions at a time
  const int hh = h/2; // The half values
  const int wh = w/2;
  for (int y = 0; y < hh-1; y++)
  {
    // Get the current U/V samples for this y line and the next one (_NL)
    const int srcIdxUV0 = y*wh;
    const int srcIdxUV1 = (y+1)*wh;
    int curU    = getValueFromSource(srcU, srcIdxUV0*inValSkip, bps, bigEndian);
    int curV    = getValueFromSource(srcV, srcIdxUV0*inValSkip, bps, bigEndian);
    int curU_NL = getValueFromSource(srcU, srcIdxUV1*inValSkip, bps, bigEndian);
    int curV_NL = getValueFromSource(srcV, srcIdxUV1*inValSkip, bps, bigEndian);
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
      const int srcIdxUVLine0 = srcIdxUV0 + x + 1;
      const int srcIdxUVLine1 = srcIdxUV1 + x + 1;
      int nextU    = getValueFromSource(srcU, srcIdxUVLine0*inValSkip, bps, bigEndian);
      int nextV    = getValueFromSource(srcV, srcIdxUVLine0*inValSkip, bps, bigEndian);
      int nextU_NL = getValueFromSource(srcU, srcIdxUVLine1*inValSkip, bps, bigEndian);
      int nextV_NL = getValueFromSource(srcV, srcIdxUVLine1*inValSkip, bps, bigEndian);
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
      convertYUVToRGB8Bit(valY1, curU             , curV             , valR1, valG1, valB1, RGBConv, fullRange, bps);
      convertYUVToRGB8Bit(valY2, interpolatedU_Hor, interpolatedV_Hor, valR2, valG2, valB2, RGBConv, fullRange, bps);
      const int pos1 = (y*2*w+x*2)*4;
      dst[pos1  ] = valB1;
      dst[pos1+1] = valG1;
      dst[pos1+2] = valR1;
      dst[pos1+3] = 255;
      dst[pos1+4] = valB2;
      dst[pos1+5] = valG2;
      dst[pos1+6] = valR2;
      dst[pos1+7] = 255;
      convertYUVToRGB8Bit(valY3, interpolatedU_Ver, interpolatedV_Ver, valR1, valG1, valB1, RGBConv, fullRange, bps);  // Second line
      convertYUVToRGB8Bit(valY4, interpolatedU_Bi , interpolatedV_Bi , valR2, valG2, valB2, RGBConv, fullRange, bps);
      const int pos2 = pos1 + w*4;  // Next line
      dst[pos2  ] = valB1;
      dst[pos2+1] = valG1;
      dst[pos2+2] = valR1;
      dst[pos2+3] = 255;
      dst[pos2+4] = valB2;
      dst[pos2+5] = valG2;
      dst[pos2+6] = valR2;
      dst[pos2+7] = 255;

      // The next one is now the current one
      curU = nextU;
      curV = nextV;
      curU_NL = nextU_NL;
      curV_NL = nextV_NL;
    }

    // For the last x value (the right border), there is no next value. Just sample and hold. Only vertical interpolation is required.
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
    convertYUVToRGB8Bit(valY1, curU, curV, valR1, valG1, valB1, RGBConv, fullRange, bps);
    convertYUVToRGB8Bit(valY2, curU, curV, valR2, valG2, valB2, RGBConv, fullRange, bps);
    const int pos1 = ((y*2+1)*w)*4;
    dst[pos1-8] = valB1;
    dst[pos1-7] = valG1;
    dst[pos1-6] = valR1;
    dst[pos1-5] = 255;
    dst[pos1-4] = valB2;
    dst[pos1-3] = valG2;
    dst[pos1-2] = valR2;
    dst[pos1-1] = 255;
    convertYUVToRGB8Bit(valY3, interpolatedU_Ver, interpolatedV_Ver, valR1, valG1, valB1, RGBConv, fullRange, bps);  // Second line
    convertYUVToRGB8Bit(valY4, interpolatedU_Ver, interpolatedV_Ver, valR2, valG2, valB2, RGBConv, fullRange, bps);
    const int pos2 = pos1 + w*4;  // Next line
    dst[pos2-8] = valB1;
    dst[pos2-7] = valG1;
    dst[pos2-6] = valR1;
    dst[pos2-5] = 255;
    dst[pos2-4] = valB2;
    dst[pos2-3] = valG2;
    dst[pos2-2] = valR2;
    dst[pos2-1] = 255;
  }

  // At the last Y line (the bottom line) a similar scenario occurs. There is no next Y line. Just sample and hold. Only horizontal interpolation is required.

  // Get the current U/V samples for this y line
  const int y = hh-1;  // Just process the last y line
  const int y2 = (hh-1)*2;

  // Get 2 chroma samples from this line
  const int srcIdxUV = y*wh;
  int curU = getValueFromSource(srcU, srcIdxUV*inValSkip, bps, bigEndian);
  int curV = getValueFromSource(srcV, srcIdxUV*inValSkip, bps, bigEndian);
  if (applyMathChroma)
  {
    curU = transformYUV(mathC.invert, mathC.scale, mathC.offset, curU, inMax);
    curV = transformYUV(mathC.invert, mathC.scale, mathC.offset, curV, inMax);
  }

  for (int x = 0; x < (w/2)-1; x++)
  {
    // Get the next U/V sample for this line and the next one
    const int srcIdxLineUV = srcIdxUV + x + 1;
    int nextU = getValueFromSource(srcU, srcIdxLineUV*inValSkip, bps, bigEndian);
    int nextV = getValueFromSource(srcV, srcIdxLineUV*inValSkip, bps, bigEndian);
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
    convertYUVToRGB8Bit(valY1, curU             , curV             , valR1, valG1, valB1, RGBConv, fullRange, bps);
    convertYUVToRGB8Bit(valY2, interpolatedU_Hor, interpolatedV_Hor, valR2, valG2, valB2, RGBConv, fullRange, bps);
    const int pos1 = (y2*w+x*2)*4;
    dst[pos1  ] = valB1;
    dst[pos1+1] = valG1;
    dst[pos1+2] = valR1;
    dst[pos1+3] = 255;
    dst[pos1+4] = valB2;
    dst[pos1+5] = valG2;
    dst[pos1+6] = valR2;
    dst[pos1+7] = 255;
    convertYUVToRGB8Bit(valY3, curU             , curV             , valR1, valG1, valB1, RGBConv, fullRange, bps);  // Second line
    convertYUVToRGB8Bit(valY4, interpolatedU_Hor, interpolatedV_Hor, valR2, valG2, valB2, RGBConv, fullRange, bps);
    const int pos2 = pos1 + w*4;  // Next line
    dst[pos2  ] = valB1;
    dst[pos2+1] = valG1;
    dst[pos2+2] = valR1;
    dst[pos2+3] = 255;
    dst[pos2+4] = valB2;
    dst[pos2+5] = valG2;
    dst[pos2+6] = valR2;
    dst[pos2+7] = 255;

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
  convertYUVToRGB8Bit(valY1, curU, curV, valR1, valG1, valB1, RGBConv, fullRange, bps);
  convertYUVToRGB8Bit(valY2, curU, curV, valR2, valG2, valB2, RGBConv, fullRange, bps);
  const int pos1 = (y2+1)*w*4;
  dst[pos1-8] = valB1;
  dst[pos1-7] = valG1;
  dst[pos1-6] = valR1;
  dst[pos1-5] = 255;
  dst[pos1-4] = valB2;
  dst[pos1-3] = valG2;
  dst[pos1-2] = valR2;
  dst[pos1-1] = 255;
  convertYUVToRGB8Bit(valY3, curU, curV, valR1, valG1, valB1, RGBConv, fullRange, bps);  // Second line
  convertYUVToRGB8Bit(valY4, curU, curV, valR2, valG2, valB2, RGBConv, fullRange, bps);
  const int pos2 = pos1 + w*4;  // Next line
  dst[pos2-8] = valB1;
  dst[pos2-7] = valG1;
  dst[pos2-6] = valR1;
  dst[pos2-5] = 255;
  dst[pos2-4] = valB2;
  dst[pos2-3] = valG2;
  dst[pos2-2] = valR2;
  dst[pos2-1] = 255;
}

inline void YUVPlaneToRGB_410(const int w, const int h, const yuvMathParameters mathY, const yuvMathParameters mathC,
                              const unsigned char * restrict srcY, const unsigned char * restrict srcU, const unsigned char * restrict srcV,
                              unsigned char * restrict dst, const int RGBConv[5], const bool fullRange,const int inMax, const InterpolationMode interpolation, const int bps, const bool bigEndian, const int inValSkip)
{
  const bool applyMathLuma = mathY.yuvMathRequired();
  const bool applyMathChroma = mathC.yuvMathRequired();
  // Format is YUV 4:1:0. Horizontal and vertical up-sampling is required. Process 4 Y positions of 2 lines at a time
  // Horizontal subsampling by 4, vertical subsampling by 2
  const int hq = h/4; // The quarter values
  const int wq = w/4;

  for (int y = 0; y < hq; y++)
  {
    // Get the current U/V samples for this y line and the next one (_NL)
    const int srcIdxUV0 = y*wq;
    const int srcIdxUV1 = (y+1)*wq;
    int curU    = getValueFromSource(srcU, srcIdxUV0*inValSkip, bps, bigEndian);
    int curV    = getValueFromSource(srcV, srcIdxUV0*inValSkip, bps, bigEndian);
    int curU_NL = (y < hq-1) ? getValueFromSource(srcU, srcIdxUV1*inValSkip, bps, bigEndian) : curU;
    int curV_NL = (y < hq-1) ? getValueFromSource(srcV, srcIdxUV1*inValSkip, bps, bigEndian) : curV;
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
      const int srcIdxUVLine0 = srcIdxUV0 + x + 1;
      const int srcIdxUVLine1 = srcIdxUV1 + x + 1;
      int nextU    = (x < wq-1) ? getValueFromSource(srcU, srcIdxUVLine0*inValSkip, bps, bigEndian) : curU;
      int nextV    = (x < wq-1) ? getValueFromSource(srcV, srcIdxUVLine0*inValSkip, bps, bigEndian) : curV;
      int nextU_NL = (x < wq-1) ? getValueFromSource(srcU, srcIdxUVLine1*inValSkip, bps, bigEndian) : curU_NL;
      int nextV_NL = (x < wq-1) ? getValueFromSource(srcV, srcIdxUVLine1*inValSkip, bps, bigEndian) : curV_NL;
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

          // Convert to RGB and save (BGRA)
          int R, G, B;
          const int pos = ((y*4+yo)*w+x*4+xo)*4;
          convertYUVToRGB8Bit(Y, U, V, R, G, B, RGBConv, fullRange, bps);
          dst[pos  ] = B;
          dst[pos+1] = G;
          dst[pos+2] = R;
          dst[pos+3] = 255;
        }
      }

      curU = nextU;
      curV = nextV;
      curU_NL = nextU_NL;
      curV_NL = nextV_NL;
    }
  }
}

inline void YUVPlaneToRGB_411(const int w, const int h, const yuvMathParameters mathY, const yuvMathParameters mathC,
  const unsigned char * restrict srcY, const unsigned char * restrict srcU, const unsigned char * restrict srcV,
  unsigned char * restrict dst, const int RGBConv[5], const bool fullRange,const int inMax, const InterpolationMode interpolation, const int bps, const bool bigEndian, const int inValSkip)
{
  // Chroma: quarter horizontal resolution
  const bool applyMathLuma = mathY.yuvMathRequired();
  const bool applyMathChroma = mathC.yuvMathRequired();

  // Horizontal up-sampling is required. Process four Y values at a time.
  for (int y = 0; y < h; y++)
  {
    const int srcIdxUV = y*w/4;
    int curUSample = getValueFromSource(srcU, srcIdxUV*inValSkip, bps, bigEndian);
    int curVSample = getValueFromSource(srcV, srcIdxUV*inValSkip, bps, bigEndian);
    if (applyMathChroma)
    {
      curUSample = transformYUV(mathC.invert, mathC.scale, mathC.offset, curUSample, inMax);
      curVSample = transformYUV(mathC.invert, mathC.scale, mathC.offset, curVSample, inMax);
    }

    for (int x = 0; x < (w/4)-1; x++)
    {
      // Get the next U/V sample
      const int srcIdxUVLine = srcIdxUV + x + 1;
      int nextUSample = getValueFromSource(srcU, srcIdxUVLine*inValSkip, bps, bigEndian);
      int nextVSample = getValueFromSource(srcV, srcIdxUVLine*inValSkip, bps, bigEndian);
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
      const int pos = (y*w+x*4)*4;
      convertYUVToRGB8Bit(valY1, curUSample, curVSample, valR, valG, valB, RGBConv, fullRange, bps);
      dst[pos  ] = valB;
      dst[pos+1] = valG;
      dst[pos+2] = valR;
      dst[pos+3] = 255;
      convertYUVToRGB8Bit(valY2, interpolatedU1, interpolatedV1, valR, valG, valB, RGBConv, fullRange, bps);
      dst[pos+4] = valB;
      dst[pos+5] = valG;
      dst[pos+6] = valR;
      dst[pos+7] = 255;
      convertYUVToRGB8Bit(valY3, interpolatedU2, interpolatedV2, valR, valG, valB, RGBConv, fullRange, bps);
      dst[pos+8] = valB;
      dst[pos+9] = valG;
      dst[pos+10] = valR;
      dst[pos+11] = 255;
      convertYUVToRGB8Bit(valY4, interpolatedU3, interpolatedV3, valR, valG, valB, RGBConv, fullRange, bps);
      dst[pos+12] = valB;
      dst[pos+13] = valG;
      dst[pos+14] = valR;
      dst[pos+15] = 255;

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
    const int pos = ((y+1)*w)*4;
    convertYUVToRGB8Bit(valY1, curUSample, curVSample, valR, valG, valB, RGBConv, fullRange, bps);
    dst[pos-16] = valB;
    dst[pos-15] = valG;
    dst[pos-14] = valR;
    dst[pos-13] = 255;
    convertYUVToRGB8Bit(valY2, curUSample, curVSample, valR, valG, valB, RGBConv, fullRange, bps);
    dst[pos-12] = valB;
    dst[pos-11] = valG;
    dst[pos-10] = valR;
    dst[pos-9] = 255;
    convertYUVToRGB8Bit(valY3, curUSample, curVSample, valR, valG, valB, RGBConv, fullRange, bps);
    dst[pos-8] = valB;
    dst[pos-7] = valG;
    dst[pos-6] = valR;
    dst[pos-5] = 255;
    convertYUVToRGB8Bit(valY4, curUSample, curVSample, valR, valG, valB, RGBConv, fullRange, bps);
    dst[pos-4] = valB;
    dst[pos-3] = valG;
    dst[pos-2] = valR;
    dst[pos-1] = 255;
  }
}

bool videoHandlerYUV::convertYUVPackedToPlanar(const QByteArray &sourceBuffer, QByteArray &targetBuffer, const QSize &curFrameSize, yuvPixelFormat &sourceBufferFormat)
{
  const yuvPixelFormat format = sourceBufferFormat;
  const YUVPackingOrder packing = format.packingOrder;

  // Make sure that the target buffer is big enough. It should be as big as the input buffer.
  if (targetBuffer.size() != sourceBuffer.size())
    targetBuffer.resize(sourceBuffer.size());

  const int w = curFrameSize.width();
  const int h = curFrameSize.height();

  // Bytes per sample
  const int bps = (format.bitsPerSample > 8) ? 2 : 1;

  if (format.subsampling == YUV_422)
  {
    // The data is arranged in blocks of 4 samples. How many of these are there?
    const int nr4Samples = w*h/2;

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
  }
  else if (format.subsampling == YUV_444)
  {
    // What are the offsets withing the 3 or 4 bytes per sample?
    const int oY = (packing == Packing_AYUV) ? 1 : 0;
    const int oU = (packing == Packing_YUV || packing == Packing_YUVA) ? 1 : 2;
    const int oV = (packing == Packing_YVU) ? 1 : (packing == Packing_AYUV) ? 3 : 2;

    // How many samples to the next sample?
    const int offsetNext = (packing == Packing_YUV || packing == Packing_YVU ? 3 : 4);

    if (bps == 1)
    {
      // One byte per sample.
      const unsigned char * restrict src = (unsigned char*)sourceBuffer.data();
      unsigned char * restrict dstY = (unsigned char*)targetBuffer.data();
      unsigned char * restrict dstU = dstY + w*h;
      unsigned char * restrict dstV = dstU + w*h;

      for (int i = 0; i < w*h; i++)
      {
        *dstY++ = src[oY];
        *dstU++ = src[oU];
        *dstV++ = src[oV];
        src += offsetNext; // Goto the next sample
      }
    }
    else
    {
      // Two bytes per sample.
      const unsigned short * restrict src = (unsigned short*)sourceBuffer.data();
      unsigned short * restrict dstY = (unsigned short*)targetBuffer.data();
      unsigned short * restrict dstU = dstY + w*h;
      unsigned short * restrict dstV = dstU + w*h;

      for (int i = 0; i < w*h; i++)
      {
        *dstY++ = src[oY];
        *dstU++ = src[oU];
        *dstV++ = src[oV];
        src += offsetNext; // Goto the next sample
      }
    }
  }
  else
    return false;

  // The output buffer is planar with the same subsampling as before
  sourceBufferFormat.planar = true;
  sourceBufferFormat.planeOrder = Order_YUV;

  return true;
}

bool videoHandlerYUV::convertYUVPlanarToRGB(const QByteArray &sourceBuffer, uchar *targetBuffer, const QSize &curFrameSize, const yuvPixelFormat &sourceBufferFormat) const
{
  // These are constant for the runtime of this function. This way, the compiler can optimize the
  // hell out of this function.
  const yuvPixelFormat format = sourceBufferFormat;
  const InterpolationMode interpolation = interpolationMode;
  const ComponentDisplayMode component = componentDisplayMode;
  const ColorConversion conversion = yuvColorConversionType;
  const int w = curFrameSize.width();
  const int h = curFrameSize.height();
  Q_UNUSED(conversion);

  // Do we have to apply YUV math?
  const yuvMathParameters mathY = mathParameters[Luma];
  const yuvMathParameters mathC = mathParameters[Chroma];
  const bool applyMathLuma   = mathY.yuvMathRequired();
  const bool applyMathChroma = mathC.yuvMathRequired();
  Q_UNUSED(applyMathLuma);
  Q_UNUSED(applyMathChroma);

  const int bps = format.bitsPerSample;
  const bool fullRange = (conversion == BT709_FullRange || conversion == BT601_FullRange || conversion == BT2020_FullRange);
  const int yOffset = 16<<(bps-8);
  const int cZero = 128<<(bps-8);
  const int inputMax = (1<<bps)-1;
  Q_UNUSED(yOffset);
  Q_UNUSED(cZero);

  // The luma component has full resolution. The size of each chroma components depends on the subsampling.
  const int componentSizeLuma = (w * h);
  const int componentSizeChroma = (w / format.getSubsamplingHor()) * (h / format.getSubsamplingVer());

  // How many bytes are in each component?
  const int nrBytesLumaPlane = (bps > 8) ? componentSizeLuma * 2 : componentSizeLuma;
  const int nrBytesChromaPlane = (bps > 8) ? componentSizeChroma * 2 : componentSizeChroma;

  // If the U and V (and A if present) components are interlevaed, we have to skip every nth value in the input when reading U and V
  const int inputValSkip = format.uvInterleaved ? ((format.planeOrder == Order_YUV || format.planeOrder == Order_YVU) ? 2 : 3) : 1;

  // A pointer to the output
  unsigned char * restrict dst = targetBuffer;

  if (component != DisplayAll || format.subsampling == YUV_400)
  {
    // We only display (or there is only) one of the color components (possibly with YUV math)
    if (component == DisplayY || format.subsampling == YUV_400)
    {
      // Luma only. The chroma subsampling does not matter.
      const unsigned char * restrict srcY = (unsigned char*)sourceBuffer.data();
      YUVPlaneToRGBMonochrome_444(componentSizeLuma, mathY, srcY, dst, inputMax, bps, format.bigEndian, 1, fullRange);
    }
    else
    {
      // Display only the U or V component
      bool firstComponent = (((format.planeOrder == Order_YUV || format.planeOrder == Order_YUVA) && component == DisplayCb) ||
                             ((format.planeOrder == Order_YVU || format.planeOrder == Order_YVUA) && component == DisplayCr));
      
      int srcOffset = nrBytesLumaPlane;
      if (!firstComponent)
      {
        if (format.uvInterleaved)
          srcOffset += (bps > 8) ? 2 : 1;
        else
          srcOffset += nrBytesChromaPlane;
      }

      const unsigned char * restrict srcC = (unsigned char*)sourceBuffer.data() + srcOffset;
      if (format.subsampling == YUV_444)
        YUVPlaneToRGBMonochrome_444(componentSizeChroma, mathC, srcC, dst, inputMax, bps, format.bigEndian, inputValSkip, fullRange);
      else if (format.subsampling == YUV_422)
        YUVPlaneToRGBMonochrome_422(componentSizeChroma, mathC, srcC, dst, inputMax, bps, format.bigEndian, inputValSkip, fullRange);
      else if (format.subsampling == YUV_420)
        YUVPlaneToRGBMonochrome_420(w, h, mathC, srcC, dst, inputMax, bps, format.bigEndian, inputValSkip, fullRange);
      else if (format.subsampling == YUV_440)
        YUVPlaneToRGBMonochrome_440(w, h, mathC, srcC, dst, inputMax, bps, format.bigEndian, inputValSkip, fullRange);
      else if (format.subsampling == YUV_410)
        YUVPlaneToRGBMonochrome_410(w, h, mathC, srcC, dst, inputMax, bps, format.bigEndian, inputValSkip, fullRange);
      else if (format.subsampling == YUV_411)
        YUVPlaneToRGBMonochrome_411(componentSizeChroma, mathC, srcC, dst, inputMax, bps, format.bigEndian, inputValSkip, fullRange);
      else
        return false;
    }
  }
  else
  {
    // Is the U plane the first or the second?
    const bool uPlaneFirst = (format.planeOrder == Order_YUV || format.planeOrder == Order_YUVA);

    // In case the U and V (and A if present) components are interleaved, the skip to the next plane is just 1 (or 2) bytes
    int nrBytesToNextChromaPlane = nrBytesChromaPlane;
    if (format.uvInterleaved)
      nrBytesToNextChromaPlane = (bps > 8) ? 2 : 1;

    // Get/set the parameters used for YUV -> RGB conversion
    const int RGBConv[5] = { 
      yuvRgbConvCoeffs[yuvColorConversionType][0],
      yuvRgbConvCoeffs[yuvColorConversionType][1],
      yuvRgbConvCoeffs[yuvColorConversionType][2],
      yuvRgbConvCoeffs[yuvColorConversionType][3],
      yuvRgbConvCoeffs[yuvColorConversionType][4]
    };

    // We are displaying all components, so we have to perform conversion to RGB (possibly including interpolation and YUV math)
    if (format.subsampling != YUV_400 && (format.chromaOffset[0] != 0 || format.chromaOffset[1] != 0))
    {
      // If there is a chroma offset, we must resample the chroma components before we convert them to RGB.
      // If so, the resampled chroma values are saved in these arrays.
      QByteArray uvPlaneChromaResampled[2];
      uvPlaneChromaResampled[0].resize(nrBytesChromaPlane);
      uvPlaneChromaResampled[1].resize(nrBytesChromaPlane);

      // We have to perform pre-filtering for the U and V positions, because there is an offset between the pixel positions of Y and U/V
      unsigned char *restrict dstU = (unsigned char*)uvPlaneChromaResampled[0].data();
      unsigned char *restrict dstV = (unsigned char*)uvPlaneChromaResampled[1].data();

      unsigned char * restrict srcY = (unsigned char*)sourceBuffer.data();
      unsigned char * restrict srcU = uPlaneFirst ? srcY + nrBytesLumaPlane : srcY + nrBytesLumaPlane + nrBytesToNextChromaPlane;
      unsigned char * restrict srcV = uPlaneFirst ? srcY + nrBytesLumaPlane + nrBytesToNextChromaPlane: srcY + nrBytesLumaPlane;
      UVPlaneResamplingChromaOffset(format, w / format.getSubsamplingHor(), h / format.getSubsamplingVer(), srcU, srcV, inputValSkip, dstU, dstV);

      if (format.subsampling == YUV_444)
        YUVPlaneToRGB_444(componentSizeLuma, mathY, mathC, srcY, dstU, dstV, dst, RGBConv, fullRange, inputMax, bps, format.bigEndian, 1);
      else if (format.subsampling == YUV_422)
        YUVPlaneToRGB_422(w, h, mathY, mathC, srcY, dstU, dstV, dst, RGBConv, fullRange, inputMax, interpolation, bps, format.bigEndian, 1);
      else if (format.subsampling == YUV_420)
        YUVPlaneToRGB_420(w, h, mathY, mathC, srcY, dstU, dstV, dst, RGBConv, fullRange, inputMax, interpolation, bps, format.bigEndian, 1);
      else if (format.subsampling == YUV_440)
        YUVPlaneToRGB_440(w, h, mathY, mathC, srcY, dstU, dstV, dst, RGBConv, fullRange, inputMax, interpolation, bps, format.bigEndian, 1);
      else if (format.subsampling == YUV_410)
        YUVPlaneToRGB_410(w, h, mathY, mathC, srcY, dstU, dstV, dst, RGBConv, fullRange, inputMax, interpolation, bps, format.bigEndian, 1);
      else if (format.subsampling == YUV_411)
        YUVPlaneToRGB_411(w, h, mathY, mathC, srcY, dstU, dstV, dst, RGBConv, fullRange, inputMax, interpolation, bps, format.bigEndian, 1);
      else
        return false;
    }
    else
    {
      // Get the pointers to the source planes (8 bit per sample)
      const unsigned char * restrict srcY = (unsigned char*)sourceBuffer.data();
      const unsigned char * restrict srcU = uPlaneFirst ? srcY + nrBytesLumaPlane : srcY + nrBytesLumaPlane + nrBytesToNextChromaPlane;
      const unsigned char * restrict srcV = uPlaneFirst ? srcY + nrBytesLumaPlane + nrBytesToNextChromaPlane: srcY + nrBytesLumaPlane;

      if (format.subsampling == YUV_444)
        YUVPlaneToRGB_444(componentSizeLuma, mathY, mathC, srcY, srcU, srcV, dst, RGBConv, fullRange, inputMax, bps, format.bigEndian, inputValSkip);
      else if (format.subsampling == YUV_422)
        YUVPlaneToRGB_422(w, h, mathY, mathC, srcY, srcU, srcV, dst, RGBConv, fullRange, inputMax, interpolation, bps, format.bigEndian, inputValSkip);
      else if (format.subsampling == YUV_420)
        YUVPlaneToRGB_420(w, h, mathY, mathC, srcY, srcU, srcV, dst, RGBConv, fullRange, inputMax, interpolation, bps, format.bigEndian, inputValSkip);
      else if (format.subsampling == YUV_440)
        YUVPlaneToRGB_440(w, h, mathY, mathC, srcY, srcU, srcV, dst, RGBConv, fullRange, inputMax, interpolation, bps, format.bigEndian, inputValSkip);
      else if (format.subsampling == YUV_410)
        YUVPlaneToRGB_410(w, h, mathY, mathC, srcY, srcU, srcV, dst, RGBConv, fullRange, inputMax, interpolation, bps, format.bigEndian, inputValSkip);
      else if (format.subsampling == YUV_411)
        YUVPlaneToRGB_411(w, h, mathY, mathC, srcY, srcU, srcV, dst, RGBConv, fullRange, inputMax, interpolation, bps, format.bigEndian, inputValSkip);
      else if (format.subsampling == YUV_400)
        YUVPlaneToRGBMonochrome_444(componentSizeLuma, mathY, srcY, dst, fullRange, inputMax, bps, format.bigEndian, 1);
      else
        return false;
    }
  }

  return true;
}

// Convert the given raw YUV data in sourceBuffer (using srcPixelFormat) to image (RGB-888), using the
// buffer tmpRGBBuffer for intermediate RGB values.
void videoHandlerYUV::convertYUVToImage(const QByteArray &sourceBuffer, QImage &outputImage, const yuvPixelFormat &yuvFormat, const QSize &curFrameSize)
{
  if (!canConvertToRGB(yuvFormat, curFrameSize))
  {
    outputImage = QImage();
    return;
  }

  DEBUG_YUV("videoHandlerYUV::convertYUVToImage");

  // Create the output image in the right format.
  // In both cases, we will set the alpha channel to 255. The format of the raw buffer is: BGRA (each 8 bit).
  // Internally, this is how QImage allocates the number of bytes per line (with depth = 32):
  // const int bytes_per_line = ((width * depth + 31) >> 5) << 2; // bytes per scanline (must be multiple of 4)
  if (is_Q_OS_WIN || is_Q_OS_MAC)
    outputImage = QImage(curFrameSize, platformImageFormat());
  else if (is_Q_OS_LINUX)
  {
    QImage::Format f = platformImageFormat();
    if (f == QImage::Format_ARGB32_Premultiplied || f == QImage::Format_ARGB32)
      outputImage = QImage(curFrameSize, f);
    else
      outputImage = QImage(curFrameSize, QImage::Format_RGB32);
  }

  // Check the image buffer size before we write to it
  assert(outputImage.byteCount() >= curFrameSize.width() * curFrameSize.height() * 4);
  
  // Convert the source to RGB
  bool convOK = true;
  if (yuvFormat.planar)
  {
    if (yuvFormat.bitsPerSample == 8 && yuvFormat.subsampling == YUV_420 && interpolationMode == NearestNeighborInterpolation &&
        yuvFormat.chromaOffset[0] == 0 && yuvFormat.chromaOffset[1] == 1 &&
        componentDisplayMode == DisplayAll && !yuvFormat.uvInterleaved &&
        !mathParameters[Luma].yuvMathRequired() && !mathParameters[Chroma].yuvMathRequired())
      // 8 bit 4:2:0, nearest neighbor, chroma offset (0,1) (the default for 4:2:0), all components displayed and no yuv math.
      // We can use a specialized function for this.
      convOK = convertYUV420ToRGB(sourceBuffer, outputImage.bits(), curFrameSize, yuvFormat);
    else
      convOK = convertYUVPlanarToRGB(sourceBuffer, outputImage.bits(), curFrameSize, yuvFormat);
  }
  else
  {
    // Convert to a planar format first
    QByteArray tmpPlanarYUVSource;
    // This is the current format of the buffer. The conversion function will change this.
    yuvPixelFormat bufferPixelFormat = yuvFormat;
    convOK &= convertYUVPackedToPlanar(sourceBuffer, tmpPlanarYUVSource, curFrameSize, bufferPixelFormat);

    if (convOK)
      convOK &= convertYUVPlanarToRGB(tmpPlanarYUVSource, outputImage.bits(), curFrameSize, bufferPixelFormat);
  }

  assert(convOK);

  if (is_Q_OS_LINUX)
  {
    // On linux, we may have to convert the image to the platform image format if it is not one of the
    // RGBA formats.
    QImage::Format f = platformImageFormat();
    if (f != QImage::Format_ARGB32_Premultiplied && f != QImage::Format_ARGB32 && f != QImage::Format_RGB32)
      outputImage = outputImage.convertToFormat(f);
  }

  DEBUG_YUV("videoHandlerYUV::convertYUVToImage Done");
}

void videoHandlerYUV::getPixelValue(const QPoint &pixelPos, unsigned int &Y, unsigned int &U, unsigned int &V)
{
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

    // Luma first
    const unsigned char * restrict srcY = (unsigned char*)currentFrameRawYUVData.data();
    const unsigned int offsetCoordinateY  = w * pixelPos.y() + pixelPos.x();
    Y = getValueFromSource(srcY, offsetCoordinateY,  format.bitsPerSample, format.bigEndian);

    U = 0;
    V = 0;
    if (format.subsampling != YUV_400)
    {
      // Now Chroma
      const bool uFirst = (format.planeOrder == Order_YUV || format.planeOrder == Order_YUVA);
      const bool hasAlpha = (format.planeOrder == Order_YUVA || format.planeOrder == Order_YVUA);
      if (format.uvInterleaved)
      {
        // U, V (and alpha) are interleaved
        const unsigned char * restrict srcUVA = srcY + nrBytesLumaPlane;
        const unsigned int mult = hasAlpha ? 3 : 2;
        const unsigned int offsetCoordinateUV = ((w / format.getSubsamplingHor() * (pixelPos.y() / format.getSubsamplingVer())) + pixelPos.x() / format.getSubsamplingHor()) * mult;

        U = getValueFromSource(srcUVA, offsetCoordinateUV + (uFirst ? 0 : 1), format.bitsPerSample, format.bigEndian);
        V = getValueFromSource(srcUVA, offsetCoordinateUV + (uFirst ? 1 : 0), format.bitsPerSample, format.bigEndian);
      }
      else
      {
        const unsigned char * restrict srcU = uFirst ? srcY + nrBytesLumaPlane : srcY + nrBytesLumaPlane + nrBytesChromaPlane;
        const unsigned char * restrict srcV = uFirst ? srcY + nrBytesLumaPlane + nrBytesChromaPlane: srcY + nrBytesLumaPlane;

        // Get the YUV data from the currentFrameRawYUVData
        const unsigned int offsetCoordinateUV = (w / format.getSubsamplingHor() * (pixelPos.y() / format.getSubsamplingVer())) + pixelPos.x() / format.getSubsamplingHor();
        
        U = getValueFromSource(srcU, offsetCoordinateUV, format.bitsPerSample, format.bigEndian);
        V = getValueFromSource(srcV, offsetCoordinateUV, format.bitsPerSample, format.bigEndian);
      }
    }
  }
  else
  {
    const YUVPackingOrder packing = format.packingOrder;
    if (format.subsampling == YUV_422)
    {
      // The data is arranged in blocks of 4 samples. How many of these are there?
      // What are the offsets withing the 4 samples for the components?
      const int oY = (packing == Packing_YUYV || packing == Packing_YVYU) ? 0 : 1;
      const int oU = (packing == Packing_UYVY) ? 0 : (packing == Packing_YUYV) ? 1 : (packing == Packing_VYUY) ? 2 : 3;
      const int oV = (packing == Packing_VYUY) ? 0 : (packing == Packing_YVYU) ? 1 : (packing == Packing_UYVY) ? 2 : 3;

      // The offset of the pixel in bytes
      const unsigned int offsetCoordinate4Block = (w * 2 * pixelPos.y() + (pixelPos.x() / 2 * 4)) * (format.bitsPerSample > 8 ? 2 : 1);
      const unsigned char * restrict src = (unsigned char*)currentFrameRawYUVData.data() + offsetCoordinate4Block;

      Y = getValueFromSource(src, (pixelPos.x() % 2 == 0) ? oY : oY + 2,  format.bitsPerSample, format.bigEndian);
      U = getValueFromSource(src, oU, format.bitsPerSample, format.bigEndian);
      V = getValueFromSource(src, oV, format.bitsPerSample, format.bigEndian);
    }
    else if (format.subsampling == YUV_444)
    {
      // The samples are packed in 4:4:4.
      // What are the offsets withing the 3 or 4 bytes per sample?
      const int oY = (packing == Packing_AYUV) ? 1 : 0;
      const int oU = (packing == Packing_YUV || packing == Packing_YUVA) ? 1 : 2;
      const int oV = (packing == Packing_YVU) ? 1 : (packing == Packing_AYUV) ? 3 : 2;

      // How many bytes to the next sample?
      const int offsetNext = (packing == Packing_YUV || packing == Packing_YVU ? 3 : 4) * (format.bitsPerSample > 8 ? 2 : 1);
      const int offsetSrc = (w * pixelPos.y() + pixelPos.x()) * offsetNext;
      const unsigned char * restrict src = (unsigned char*)currentFrameRawYUVData.data() + offsetSrc;

      Y = getValueFromSource(src, oY, format.bitsPerSample, format.bigEndian);
      U = getValueFromSource(src, oU, format.bitsPerSample, format.bigEndian);
      V = getValueFromSource(src, oV, format.bitsPerSample, format.bigEndian);
    }
  }
}

// This is a specialized function that can convert 8-bit YUV 4:2:0 to RGB888 using NearestNeighborInterpolation.
// The chroma must be 0 in x direction and 1 in y direction. No yuvMath is supported.
// TODO: Correct the chroma subsampling offset.
#if SSE_CONVERSION
bool videoHandlerYUV::convertYUV420ToRGB(const byteArrayAligned &sourceBuffer, byteArrayAligned &targetBuffer)
#else
bool videoHandlerYUV::convertYUV420ToRGB(const QByteArray &sourceBuffer, unsigned char *targetBuffer, const QSize &size, const yuvPixelFormat format)
#endif
{
  const int frameWidth = size.width();
  const int frameHeight = size.height();

  // For 4:2:0, w and h must be dividible by 2
  assert(frameWidth % 2 == 0 && frameHeight % 2 == 0);
  
  int componentLenghtY  = frameWidth * frameHeight;
  int componentLengthUV = componentLenghtY >> 2;
  Q_ASSERT(sourceBuffer.size() >= componentLenghtY + componentLengthUV + componentLengthUV); // YUV 420 must be (at least) 1.5*Y-area

#if SSE_CONVERSION_420_ALT
  quint8 *srcYRaw = (quint8*) sourceBuffer.data();
  quint8 *srcURaw = srcYRaw + componentLenghtY;
  quint8 *srcVRaw = srcURaw + componentLengthUV;

  quint8 *dstBuffer = (quint8*)targetBuffer.data();
  quint32 dstBufferStride = frameWidth*4;

  yuv420_to_argb8888(srcYRaw,srcURaw,srcVRaw,frameWidth,frameWidth>>1,frameWidth,frameHeight,dstBuffer,dstBufferStride);
  return false;
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

    return true;
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

  unsigned char * restrict dst = targetBuffer;

  // Get/set the parameters used for YUV -> RGB conversion
  const bool fullRange = (yuvColorConversionType == BT709_FullRange || yuvColorConversionType == BT601_FullRange || yuvColorConversionType == BT2020_FullRange);
  const int yOffset = (fullRange ? 0 : 16);
  const int cZero = 128;
  const int RGBConv[5] = { 
    yuvRgbConvCoeffs[yuvColorConversionType][0],
    yuvRgbConvCoeffs[yuvColorConversionType][1],
    yuvRgbConvCoeffs[yuvColorConversionType][2],
    yuvRgbConvCoeffs[yuvColorConversionType][3],
    yuvRgbConvCoeffs[yuvColorConversionType][4]
  };

  // Get pointers to the source and the output array
  const bool uPplaneFirst = (format.planeOrder == Order_YUV || format.planeOrder == Order_YUVA); // Is the U plane the first or the second?
  const unsigned char * restrict srcY = (unsigned char*)sourceBuffer.data();
  const unsigned char * restrict srcU = uPplaneFirst ? srcY + componentLenghtY : srcY + componentLenghtY + componentLengthUV;
  const unsigned char * restrict srcV = uPplaneFirst ? srcY + componentLenghtY + componentLengthUV : srcY + componentLenghtY;

  int yh;
  for (yh=0; yh < frameHeight / 2; yh++)
  {
    // Process two lines at once, always 4 RGB values at a time (they have the same U/V components)

    int dstAddr1 = yh * 2 * frameWidth * 4;         // The RGB output address of line yh*2
    int dstAddr2 = (yh * 2 + 1) * frameWidth * 4;   // The RGB output address of line yh*2+1
    int srcAddrY1 = yh * 2 * frameWidth;            // The Y source address of line yh*2
    int srcAddrY2 = (yh * 2 + 1) * frameWidth;      // The Y source address of line yh*2+1
    int srcAddrUV = yh * frameWidth / 2;            // The UV source address of both lines (UV are identical)

    for (int xh=0, x=0; xh < frameWidth / 2; xh++, x+=2)
    {
      // Process four pixels (the ones for which U/V are valid

      // Load UV and pre-multiply
      const int U_tmp_G = ((int)srcU[srcAddrUV + xh] - cZero) * RGBConv[2];
      const int U_tmp_B = ((int)srcU[srcAddrUV + xh] - cZero) * RGBConv[4];
      const int V_tmp_R = ((int)srcV[srcAddrUV + xh] - cZero) * RGBConv[1];
      const int V_tmp_G = ((int)srcV[srcAddrUV + xh] - cZero) * RGBConv[3];

      // Pixel top left
      {
        const int Y_tmp = ((int)srcY[srcAddrY1 + x] - yOffset) * RGBConv[0];

        const int R_tmp = (Y_tmp           + V_tmp_R) >> 16;
        const int G_tmp = (Y_tmp + U_tmp_G + V_tmp_G) >> 16;
        const int B_tmp = (Y_tmp + U_tmp_B          ) >> 16;

        dst[dstAddr1]   = clip_buf[B_tmp];
        dst[dstAddr1+1] = clip_buf[G_tmp];
        dst[dstAddr1+2] = clip_buf[R_tmp];
        dst[dstAddr1+3] = 255;
        dstAddr1 += 4;
      }
      // Pixel top right
      {
        const int Y_tmp = ((int)srcY[srcAddrY1 + x + 1] - yOffset) * RGBConv[0];

        const int R_tmp = (Y_tmp           + V_tmp_R) >> 16;
        const int G_tmp = (Y_tmp + U_tmp_G + V_tmp_G) >> 16;
        const int B_tmp = (Y_tmp + U_tmp_B          ) >> 16;

        dst[dstAddr1]   = clip_buf[B_tmp];
        dst[dstAddr1+1] = clip_buf[G_tmp];
        dst[dstAddr1+2] = clip_buf[R_tmp];
        dst[dstAddr1+3] = 255;
        dstAddr1 += 4;
      }
      // Pixel bottom left
      {
        const int Y_tmp = ((int)srcY[srcAddrY2 + x] - yOffset) * RGBConv[0];

        const int R_tmp = (Y_tmp           + V_tmp_R) >> 16;
        const int G_tmp = (Y_tmp + U_tmp_G + V_tmp_G) >> 16;
        const int B_tmp = (Y_tmp + U_tmp_B          ) >> 16;

        dst[dstAddr2]   = clip_buf[B_tmp];
        dst[dstAddr2+1] = clip_buf[G_tmp];
        dst[dstAddr2+2] = clip_buf[R_tmp];
        dst[dstAddr2+3] = 255;
        dstAddr2 += 4;
      }
      // Pixel bottom right
      {
        const int Y_tmp = ((int)srcY[srcAddrY2 + x + 1] - yOffset) * RGBConv[0];

        const int R_tmp = (Y_tmp           + V_tmp_R) >> 16;
        const int G_tmp = (Y_tmp + U_tmp_G + V_tmp_G) >> 16;
        const int B_tmp = (Y_tmp + U_tmp_B          ) >> 16;

        dst[dstAddr2]   = clip_buf[B_tmp];
        dst[dstAddr2+1] = clip_buf[G_tmp];
        dst[dstAddr2+2] = clip_buf[R_tmp];
        dst[dstAddr2+3] = 255;
        dstAddr2 += 4;
      }
    }
  }

  return true;
}

bool videoHandlerYUV::markDifferencesYUVPlanarToRGB(const QByteArray &sourceBuffer, unsigned char *targetBuffer, const QSize &curFrameSize, const yuvPixelFormat &sourceBufferFormat) const
{
  // These are constant for the runtime of this function. This way, the compiler can optimize the
  // hell out of this function.
  const yuvPixelFormat format = sourceBufferFormat;
  const int w = curFrameSize.width();
  const int h = curFrameSize.height();

  const int bps = format.bitsPerSample;
  const int cZero = 128<<(bps-8);

  // Other bit depths not (yet) supported. w and h must be divisible by the subsampling.
  assert(bps >=8 && bps <= 16 && (w % format.getSubsamplingHor()) == 0 && (h % format.getSubsamplingVer()) == 0);

  // The luma component has full resolution. The size of each chroma components depends on the subsampling.
  const int componentSizeLuma = (w * h);
  const int componentSizeChroma = (w / format.getSubsamplingHor()) * (h / format.getSubsamplingVer());

  // How many bytes are in each component?
  const int nrBytesLumaPlane = (bps > 8) ? componentSizeLuma * 2 : componentSizeLuma;
  const int nrBytesChromaPlane = (bps > 8) ? componentSizeChroma * 2 : componentSizeChroma;

  // Is this big endian (actually the difference buffer should always be big endian)
  const bool bigEndian = format.bigEndian;

  // A pointer to the output
  unsigned char * restrict dst = targetBuffer;

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
          dst[((y+yInBlock)*w+x+xInBlock)*4  ] = B;
          dst[((y+yInBlock)*w+x+xInBlock)*4+1] = G;
          dst[((y+yInBlock)*w+x+xInBlock)*4+2] = R;
          dst[((y+yInBlock)*w+x+xInBlock)*4+3] = 255;
        }
      }
    }

  return true;
}

YUV_Internals::yuvPixelFormat videoHandlerYUV::getDiffYUVFormat() const
{
    return diffYUVFormat;
}

QByteArray videoHandlerYUV::getDiffYUV() const
{
    return diffYUV;
}

QImage videoHandlerYUV::calculateDifference(frameHandler *item2, const int frameIdxItem0, const int frameIdxItem1, QList<infoItem> &differenceInfoList, const int amplificationFactor, const bool markDifference)
{
  is_YUV_diff = false;

  videoHandlerYUV *yuvItem2 = dynamic_cast<videoHandlerYUV*>(item2);
  if (yuvItem2 == nullptr)
    // The given item is not a YUV source. We cannot compare YUV values to non YUV values.
    // Call the base class comparison function to compare the items using the RGB values.
    return videoHandler::calculateDifference(item2, frameIdxItem0, frameIdxItem1, differenceInfoList, amplificationFactor, markDifference);

  if (srcPixelFormat.subsampling != yuvItem2->srcPixelFormat.subsampling)
    // The two items have different subsampling modes. Compare RGB values instead.
    return videoHandler::calculateDifference(item2, frameIdxItem0, frameIdxItem1, differenceInfoList, amplificationFactor, markDifference);

  // Get/Set the bit depth of the input and output
  // If the bit depth if the two items is different, we will scale the item with the lower bit depth up.
  const int bps_in[2] = {srcPixelFormat.bitsPerSample, yuvItem2->srcPixelFormat.bitsPerSample};
  const int bps_out = std::max(bps_in[0], bps_in[1]);
  // Which of the two input values has to be scaled up? Only one of these (or neither) can be set.
  const bool bitDepthScaling[2] = {bps_in[0] != bps_out, bps_in[1] != bps_out};
  // Scale the input up by this many bits
  const int depthScale = bps_out - (bitDepthScaling[0] ? bps_in[0] : bps_in[1]);
  // Add a warning if the bit depths of the two inputs don't agree
  if (bitDepthScaling[0] || bitDepthScaling[1])
    differenceInfoList.append(infoItem("Warning", "The bit depth of the two items differs.", "The bit depth of the two input items is different. The lower bit depth will be scaled up and the difference is calculated."));
  // What are the maximum and middle value for the output bit depth
  const int diffZero = 128 << (bps_out-8);
  const int maxVal = (1 << bps_out) - 1;

  // Do we amplify the values?
  const bool amplification = (amplificationFactor != 1 && !markDifference);

  // Load the right raw YUV data (if not already loaded).
  // This will just update the raw YUV data. No conversion to image (RGB) is performed. This is either
  // done on request if the frame is actually shown or has already been done by the caching process.
  if (!loadRawYUVData(frameIdxItem0))
    return QImage();  // Loading failed
  if (!yuvItem2->loadRawYUVData(frameIdxItem1))
    return QImage();  // Loading failed

  // Both YUV buffers are up to date. Really calculate the difference.
  DEBUG_YUV("videoHandlerYUV::calculateDifference frame %d", frame);

  // The items can be of different size (we then calculate the difference of the top left aligned part)
  const int w_in[2] = {frameSize.width(), yuvItem2->frameSize.width()};
  const int h_in[2] = {frameSize.height(), yuvItem2->frameSize.height()};
  const int w_out = qMin(w_in[0], w_in[1]);
  const int h_out = qMin(h_in[0], h_in[1]);
  // Append a warning if the frame sizes are different
  if (frameSize != yuvItem2->frameSize)
    differenceInfoList.append(infoItem("Warning", "The size of the two items differs.", "The size of the two input items is different. The difference of the top left aligned part that overlaps will be calculated."));

  yuvPixelFormat tmpDiffYUVFormat(srcPixelFormat.subsampling, bps_out, Order_YUV, true);
  diffYUVFormat = tmpDiffYUVFormat;

  if (!canConvertToRGB(tmpDiffYUVFormat, QSize(w_out, h_out)))
    return QImage();


  // Get subsampling modes (they are identical for both inputs and the output)
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
  diffYUV.resize(componentSizeLuma_out + 2*componentSizeChroma_out);
  unsigned char * restrict dstY = (unsigned char*)diffYUV.data();
  unsigned char * restrict dstU = dstY + componentSizeLuma_out;
  unsigned char * restrict dstV = dstU + componentSizeChroma_out;

  // Also calculate the MSE while we're at it (Y,U,V)
  // TODO: Bug: MSE is not scaled correctly in all YUV format cases
  qint64 mseAdd[3] = {0, 0, 0};

  // Calculate Luma sample difference
  const int stride_in[2] = {bps_in[0] > 8 ? w_in[0]*2 : w_in[0], bps_in[1] > 8 ? w_in[1]*2 : w_in[1]};  // How many bytes to the next y line?
  for (int y = 0; y < h_out; y++)
  {
    for (int x = 0; x < w_out; x++)
    {
      int val1 = getValueFromSource(srcY1, x, bps_in[0], bigEndian[0]);
      int val2 = getValueFromSource(srcY2, x, bps_in[1], bigEndian[1]);

      // Scale (if necessary)
      if (bitDepthScaling[0])
        val1 = val1 << depthScale;
      else if (bitDepthScaling[1])
        val2 = val2 << depthScale;

      // Calculate the difference, add MSE, (amplify) and clip the difference value
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

      // Scale (if necessary)
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

      // Calculate the difference, add MSE, (amplify) and clip the difference value
      int diffU = valU1 - valU2;
      int diffV = valV1 - valV2;
      mseAdd[1] += diffU * diffU;
      mseAdd[2] += diffV * diffV;
      if (amplification)
      {
        diffU *= amplificationFactor;
        diffV *= amplificationFactor;
      }
      diffU = clip(diffU + diffZero, 0, maxVal);
      diffV = clip(diffV + diffZero, 0, maxVal);

      setValueInBuffer(dstU, diffU, 0, bps_out, true);
      setValueInBuffer(dstV, diffV, 0, bps_out, true);
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

  // Create the output image in the right format
  // In both cases, we will set the alpha channel to 255. The format of the raw buffer is: BGRA (each 8 bit).
  QImage outputImage;
  if (is_Q_OS_WIN)
    outputImage = QImage(QSize(w_out, h_out), QImage::Format_ARGB32_Premultiplied);
  else if (is_Q_OS_MAC)
    outputImage = QImage(QSize(w_out, h_out), QImage::Format_RGB32);
  else if (is_Q_OS_LINUX)
  {
    QImage::Format f = platformImageFormat();
    if (f == QImage::Format_ARGB32_Premultiplied)
      outputImage = QImage(QSize(w_out, h_out), QImage::Format_ARGB32_Premultiplied);
    if (f == QImage::Format_ARGB32)
      outputImage = QImage(QSize(w_out, h_out), QImage::Format_ARGB32);
    else
      outputImage = QImage(QSize(w_out, h_out), QImage::Format_RGB32);
  }

  if (markDifference)
    // We don't want to see the actual difference but just where differences are.
    markDifferencesYUVPlanarToRGB(diffYUV, outputImage.bits(), QSize(w_out, h_out), tmpDiffYUVFormat);
  else
    // Get the format of the tmpDiffYUV buffer and convert it to RGB
    convertYUVPlanarToRGB(diffYUV, outputImage.bits(), QSize(w_out, h_out), tmpDiffYUVFormat);

  // Append the conversion information that will be returned
  QStringList yuvSubsamplings = QStringList() << "4:4:4" << "4:2:2" << "4:2:0" << "4:4:0" << "4:1:0" << "4:1:1" << "4:0:0";
  differenceInfoList.append(infoItem("Difference Type",QString("YUV %1").arg(yuvSubsamplings[srcPixelFormat.subsampling])));
  double mse[4];
  mse[0] = double(mseAdd[0]) / (w_out * h_out);
  mse[1] = double(mseAdd[1]) / (w_out * h_out);
  mse[2] = double(mseAdd[2]) / (w_out * h_out);
  mse[3] = mse[0] + mse[1] + mse[2];
  differenceInfoList.append(infoItem("MSE Y",QString("%1").arg(mse[0])));
  differenceInfoList.append(infoItem("MSE U",QString("%1").arg(mse[1])));
  differenceInfoList.append(infoItem("MSE V",QString("%1").arg(mse[2])));
  differenceInfoList.append(infoItem("MSE All",QString("%1").arg(mse[3])));

  if (is_Q_OS_LINUX)
  {
    // On linux, we may have to convert the image to the platform image format if it is not one of the
    // RGBA formats.
    QImage::Format f = platformImageFormat();
    if (f != QImage::Format_ARGB32_Premultiplied && f != QImage::Format_ARGB32 && f != QImage::Format_RGB32)
      return outputImage.convertToFormat(f);
  }  

  // we have a yuv differance available
  is_YUV_diff = true;
  return outputImage;
}

void videoHandlerYUV::setYUVPixelFormat(const yuvPixelFormat &newFormat, bool emitSignal)
{
  if (!newFormat.isValid())
    return;

  if (newFormat != srcPixelFormat)
  {
    if (ui.created())
    {
      // Check if the custom format is in the presets list. If not, add it.
      int idx = yuvPresetsList.indexOf(newFormat);
      if (idx == -1)
      {
        // Valid pixel format with is not in the list. Add it...
        yuvPresetsList.append(newFormat);
        int nrItems = ui.yuvFormatComboBox->count();
        const QSignalBlocker blocker(ui.yuvFormatComboBox);
        ui.yuvFormatComboBox->insertItem(nrItems-1, newFormat.getName());
        // Select the added format
        idx = yuvPresetsList.indexOf(newFormat);
        ui.yuvFormatComboBox->setCurrentIndex(idx);
      }
      else
      {
        // Just select the format in the combo box
        const QSignalBlocker blocker(ui.yuvFormatComboBox);
        ui.yuvFormatComboBox->setCurrentIndex(idx);
      }
    }

    setSrcPixelFormat(newFormat, emitSignal);
  }
}

bool videoHandlerYUV::getIs_YUV_diff() const
{
    return is_YUV_diff;
}


void videoHandlerYUV::setYUVColorConversion(ColorConversion conversion)
{
  if (conversion != yuvColorConversionType)
  {
    yuvColorConversionType = conversion;

    if (ui.created())
      ui.colorConversionComboBox->setCurrentIndex(int(yuvColorConversionType));
  }
}

void videoHandlerYUV::setFrameSize(const QSize &size)
{
  if (size != frameSize)
  {
    currentFrameRawYUVData_frameIdx = -1;
    currentImageIdx = -1;
  }

  videoHandler::setFrameSize(size);
}

void videoHandlerYUV::invalidateAllBuffers()
{
  currentFrameRawYUVData_frameIdx = -1;
  rawYUVData_frameIdx = -1;
  videoHandler::invalidateAllBuffers();
}

bool videoHandlerYUV::canConvertToRGB(yuvPixelFormat format, QSize imageSize, QString *whyNot) const
{
  if (!format.isValid())
    return false;

  // Check the bit depth
  const int bps = format.bitsPerSample;
  bool canConvert = true;
  if (bps < 8 || bps > 16)
  {
    if (whyNot)
      whyNot->append(QString("The currently set bit depth (%1) is not supported.\n").arg(bps));
    canConvert = false;
  }
  if (imageSize.width() % format.getSubsamplingHor() != 0)
  {
    if (whyNot)
      whyNot->append(QString("The item width (%1) must be divisible by the horizontal subsampling factor (%2).\n").arg(imageSize.width()).arg(format.getSubsamplingHor()));
    canConvert = false;
  }
  if (imageSize.height() % format.getSubsamplingVer() != 0)
  {
    if (whyNot)
      whyNot->append(QString("The item height (%1) must be divisible by the vertical subsampling factor (%2).\n").arg(imageSize.height()).arg(format.getSubsamplingVer()));
    canConvert = false;
  }
  if (format.subsampling < 0 || format.subsampling >= YUV_NUM_SUBSAMPLINGS)
  {
    if (whyNot)
      whyNot->append(QString("The current yuv subsampling (%1) is invalid.\n").arg(format.subsampling));
    canConvert = false;
  }
  if (!format.planar && format.subsampling != YUV_422 && format.subsampling != YUV_444)
  { 
    if (whyNot)
      whyNot->append(QString("Packed YUV formats are onyl supported for 4:2:2 and 4:4:4 subsampling.\n"));
    canConvert = false;
  }
  return canConvert;
}
