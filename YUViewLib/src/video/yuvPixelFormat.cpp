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

#include "yuvPixelFormat.h"

#include <QRegularExpression>
#include <QSize>

namespace YUV_Internals
{

void getColorConversionCoefficients(ColorConversion colorConversion, int RGBConv[5])
{
  // The conversion parameters for the components of the different supported YUV->RGB conversions
  // The first index is the index of the ColorConversion enum. The second index is [Y, cRV, cGU,
  // cGV, cBU].
  const int yuvRgbConvCoeffs[6][5] = {
      {76309, 117489, -13975, -34925, 138438}, // BT709_LimitedRange
      {65536, 103206, -12276, -30679, 121608}, // BT709_FullRange
      {76309, 104597, -25675, -53279, 132201}, // BT601_LimitedRange
      {65536, 91881, -22553, -46802, 116129},  // BT601_FullRange
      {76309, 110013, -12276, -42626, 140363}, // BT2020_LimitedRange
      {65536, 96638, -10783, -37444, 123299}   // BT2020_FullRange
  };
  const auto index = colorConversionList.indexOf(colorConversion);
  for (unsigned int i = 0; i < 5; i++)
    RGBConv[i] = yuvRgbConvCoeffs[index][i];
}

// All values between 0 and this value are possible for the subsampling.
int getMaxPossibleChromaOffsetValues(bool horizontal, Subsampling subsampling)
{
  if (subsampling == Subsampling::YUV_444)
    return 1;
  else if (subsampling == Subsampling::YUV_422)
    return (horizontal) ? 3 : 1;
  else if (subsampling == Subsampling::YUV_420)
    return 3;
  else if (subsampling == Subsampling::YUV_440)
    return (horizontal) ? 1 : 3;
  else if (subsampling == Subsampling::YUV_410)
    return 7;
  else if (subsampling == Subsampling::YUV_411)
    return (horizontal) ? 7 : 1;
  return 0;
}

// Return a list with all the packing formats that are supported with this subsampling
QList<PackingOrder> getSupportedPackingFormats(Subsampling subsampling)
{
  if (subsampling == Subsampling::YUV_422)
    return QList<PackingOrder>() << PackingOrder::UYVY << PackingOrder::VYUY << PackingOrder::YUYV
                                 << PackingOrder::YVYU;
  if (subsampling == Subsampling::YUV_444)
    return QList<PackingOrder>() << PackingOrder::YUV << PackingOrder::YVU << PackingOrder::AYUV
                                 << PackingOrder::YUVA << PackingOrder::VUYA;

  return QList<PackingOrder>();
}

QString subsamplingToString(Subsampling type)
{
  if (subsamplingList.contains(type))
  {
    const auto index = subsamplingList.indexOf(type);
    return subsamplingNameList[index];
  }
  return {};
}

Subsampling stringToSubsampling(QString typeString)
{
  if (subsamplingNameList.contains(typeString))
  {
    const auto index = subsamplingNameList.indexOf(typeString);
    return Subsampling(index);
  }
  return Subsampling::UNKNOWN;
}

QString getPackingFormatString(PackingOrder packing)
{
  if (packingOrderList.contains(packing))
  {
    const auto index = packingOrderList.indexOf(packing);
    return packingOrderNameList[index];
  }
  return {};
}

// Is this the default chroma offset for the subsampling type?
bool isDefaultChromaFormat(int chromaOffset, bool offsetX, Subsampling subsampling)
{
  if (subsampling == Subsampling::YUV_420 && !offsetX)
    // The default subsampling for YUV 420 has a Y offset of 1/2
    return chromaOffset == 1;
  else if (subsampling == Subsampling::YUV_400)
    return true;
  return chromaOffset == 0;
}

yuvPixelFormat::yuvPixelFormat(const QString &name)
{

  QRegularExpression rxYUVFormat(
      "([YUVA]{3,6}(?:\\(IL\\))?) (4:[4210]{1}:[4210]{1}) ([0-9]{1,2})-bit[ ]?([BL]{1}E)?[ "
      "]?(packed-B|packed)?[ ]?(Cx[0-9]+)?[ ]?(Cy[0-9]+)?");

  auto match = rxYUVFormat.match(name);
  if (match.hasMatch())
  {
    yuvPixelFormat newFormat;

    // Is this a packed format or not?
    QString packed   = match.captured(5);
    newFormat.planar = packed.isEmpty();
    if (!newFormat.planar)
      newFormat.bytePacking = (packed == "packed-B");

    // Parse the YUV order (planar or packed)
    if (newFormat.planar)
    {
      QString yuvName = match.captured(1);
      if (yuvName.endsWith("(IL)"))
      {
        newFormat.uvInterleaved = true;
        yuvName.chop(4);
      }

      int idx = planeOrderNameList.indexOf(yuvName);
      if (idx == -1)
        return;
      newFormat.planeOrder = planeOrderList[idx];
    }
    else
    {
      int idx = packingOrderNameList.indexOf(match.captured(1));
      if (idx == -1)
        return;
      newFormat.packingOrder = packingOrderList[idx];
    }

    // Parse the subsampling
    int idx = subsamplingTextList.indexOf(match.captured(2));
    if (idx == -1)
      return;
    newFormat.subsampling = subsamplingList[idx];

    // Get the bit depth
    bool ok;
    int  bitDepth = match.captured(3).toInt(&ok);
    if (!ok || bitDepth <= 0)
      return;
    newFormat.bitsPerSample = bitDepth;

    // Get the endianness. If not in the name, assume LE
    newFormat.bigEndian = (match.captured(4) == "BE");

    // Get the chroma offsets
    newFormat.setDefaultChromaOffset();
    if (match.captured(6).startsWith("Cx"))
    {
      QString test              = match.captured(6);
      QString test2             = match.captured(6).mid(2);
      newFormat.chromaOffset[0] = match.captured(6).mid(2).toInt();
    }
    if (match.captured(7).startsWith("Cy"))
      newFormat.chromaOffset[1] = match.captured(7).mid(2).toInt();

    // Check if the format is valid.
    if (newFormat.isValid())
    {
      // Set all the values from the new format
      this->subsampling     = newFormat.subsampling;
      this->bitsPerSample   = newFormat.bitsPerSample;
      this->bigEndian       = newFormat.bigEndian;
      this->planar          = newFormat.planar;
      this->planeOrder      = newFormat.planeOrder;
      this->uvInterleaved   = newFormat.uvInterleaved;
      this->packingOrder    = newFormat.packingOrder;
      this->bytePacking     = newFormat.bytePacking;
      this->chromaOffset[0] = newFormat.chromaOffset[0];
      this->chromaOffset[1] = newFormat.chromaOffset[1];
    }
  }
}

yuvPixelFormat::yuvPixelFormat(Subsampling subsampling,
                               int         bitsPerSample,
                               PlaneOrder  planeOrder,
                               bool        bigEndian)
    : subsampling(subsampling), bitsPerSample(bitsPerSample), bigEndian(bigEndian), planar(true),
      planeOrder(planeOrder), uvInterleaved(false)
{
  this->setDefaultChromaOffset();
}

yuvPixelFormat::yuvPixelFormat(Subsampling  subsampling,
                               int          bitsPerSample,
                               PackingOrder packingOrder,
                               bool         bytePacking,
                               bool         bigEndian)
    : subsampling(subsampling), bitsPerSample(bitsPerSample), bigEndian(bigEndian), planar(false),
      uvInterleaved(false), packingOrder(packingOrder), bytePacking(bytePacking)
{
  this->setDefaultChromaOffset();
}

bool yuvPixelFormat::isValid() const
{
  if (!planar)
  {
    // Check the packing mode
    if ((this->packingOrder == PackingOrder::YUV || this->packingOrder == PackingOrder::YVU ||
         this->packingOrder == PackingOrder::AYUV || this->packingOrder == PackingOrder::YUVA ||
         this->packingOrder == PackingOrder::VUYA) &&
        subsampling != Subsampling::YUV_444)
      return false;
    if ((this->packingOrder == PackingOrder::UYVY || this->packingOrder == PackingOrder::VYUY ||
         this->packingOrder == PackingOrder::YUYV || this->packingOrder == PackingOrder::YVYU) &&
        subsampling != Subsampling::YUV_422)
      return false;
    if (this->packingOrder == PackingOrder::UNKNOWN)
      return false;
    /*if ((packingOrder == Packing_YYYYUV || packingOrder == Packing_YYUYYV || packingOrder ==
      Packing_UYYVYY || packingOrder == Packing_VYYUYY) && subsampling == Subsampling::YUV_420)
      return false;*/
    if (this->subsampling == Subsampling::YUV_420 || this->subsampling == Subsampling::YUV_440 ||
        this->subsampling == Subsampling::YUV_410 || this->subsampling == Subsampling::YUV_411 ||
        this->subsampling == Subsampling::YUV_400)
      // No support for packed formats with this subsampling (yet)
      return false;
    if (this->uvInterleaved)
      // This can only be set for planar formats
      return false;
  }
  if (this->subsampling != Subsampling::YUV_400)
  {
    // There are chroma components. Check the chroma offsets.
    if (this->chromaOffset[0] < 0 ||
        this->chromaOffset[0] > getMaxPossibleChromaOffsetValues(true, this->subsampling))
      return false;
    if (this->chromaOffset[1] < 0 ||
        this->chromaOffset[1] > getMaxPossibleChromaOffsetValues(false, this->subsampling))
      return false;
  }
  // Check the bit depth
  if (this->bitsPerSample < 7)
    return false;
  return true;
}

bool yuvPixelFormat::canConvertToRGB(QSize imageSize, QString *whyNot) const
{
  if (!this->isValid())
    return false;

  // Check the bit depth
  const int bps        = this->bitsPerSample;
  bool      canConvert = true;
  if (bps < 8 || bps > 16)
  {
    if (whyNot)
      whyNot->append(QString("The currently set bit depth (%1) is not supported.\n").arg(bps));
    canConvert = false;
  }
  if (imageSize.width() % this->getSubsamplingHor() != 0)
  {
    if (whyNot)
      whyNot->append(
          QString(
              "The item width (%1) must be divisible by the horizontal subsampling factor (%2).\n")
              .arg(imageSize.width())
              .arg(this->getSubsamplingHor()));
    canConvert = false;
  }
  if (imageSize.height() % this->getSubsamplingVer() != 0)
  {
    if (whyNot)
      whyNot->append(
          QString(
              "The item height (%1) must be divisible by the vertical subsampling factor (%2).\n")
              .arg(imageSize.height())
              .arg(this->getSubsamplingVer()));
    canConvert = false;
  }
  if (this->subsampling == Subsampling::UNKNOWN)
  {
    if (whyNot)
      whyNot->append("The current yuv subsampling is unknown.\n");
    canConvert = false;
  }
  if (!this->planar && this->subsampling != Subsampling::YUV_422 &&
      this->subsampling != Subsampling::YUV_444)
  {
    if (whyNot)
      whyNot->append("Packed YUV formats are onyl supported for 4:2:2 and 4:4:4 subsampling.\n");
    canConvert = false;
  }
  return canConvert;
}

// Generate a unique name for the YUV format
QString yuvPixelFormat::getName() const
{
  if (!this->isValid())
    return "Invalid";

  QString name;

  // Start with the YUV order
  if (this->planar)
  {
    const auto           idx          = int(this->planeOrder);
    static const QString orderNames[] = {"YUV", "YVU", "YUVA", "YVUA"};
    Q_ASSERT(idx >= 0 && idx < 4);
    name += orderNames[idx];

    if (this->uvInterleaved)
      name += "(IL)";
  }
  else
    name += getPackingFormatString(this->packingOrder);

  // Next add the subsampling
  Q_ASSERT(this->subsampling != Subsampling::UNKNOWN);
  name += " " + subsamplingTextList[subsamplingList.indexOf(this->subsampling)];

  // Add the bits
  name += QString(" %1-bit").arg(this->bitsPerSample);

  // Add the endianness (if the bit depth is greater 8)
  if (this->bitsPerSample > 8)
    name += (this->bigEndian) ? " BE" : " LE";

  if (!this->planar && this->subsampling != Subsampling::YUV_400)
    name += this->bytePacking ? " packed-B" : " packed";

  // Add the Chroma offsets (if it is not the default offset)
  if (!isDefaultChromaFormat(this->chromaOffset[0], true, this->subsampling))
    name += QString(" Cx%1").arg(this->chromaOffset[0]);
  if (!isDefaultChromaFormat(this->chromaOffset[1], false, this->subsampling))
    name += QString(" Cy%1").arg(this->chromaOffset[1]);

  return name;
}

int64_t yuvPixelFormat::bytesPerFrame(const QSize &frameSize) const
{
  int64_t bytes = 0;
  if (this->planar || !this->bytePacking)
  {
    // Add the bytes of the 3 (or 4) planes.
    // This also works for packed formats without byte packing. For these formats the number of
    // bytes are identical to the not packed formats, the bytes are just sorted in another way.

    const auto bytesPerSample = (this->bitsPerSample + 7) / 8;        // Round to bytes
    bytes += frameSize.width() * frameSize.height() * bytesPerSample; // Luma plane
    if (this->subsampling == Subsampling::YUV_444)
      bytes += frameSize.width() * frameSize.height() * bytesPerSample * 2; // U/V planes
    else if (this->subsampling == Subsampling::YUV_422 || this->subsampling == Subsampling::YUV_440)
      bytes += (frameSize.width() / 2) * frameSize.height() * bytesPerSample *
               2; // U/V planes, half the width
    else if (this->subsampling == Subsampling::YUV_420)
      bytes += (frameSize.width() / 2) * (frameSize.height() / 2) * bytesPerSample *
               2; // U/V planes, half the width and height
    else if (this->subsampling == Subsampling::YUV_410)
      bytes += (frameSize.width() / 4) * (frameSize.height() / 4) * bytesPerSample *
               2; // U/V planes, half the width and height
    else if (this->subsampling == Subsampling::YUV_411)
      bytes += (frameSize.width() / 4) * frameSize.height() * bytesPerSample *
               2; // U/V planes, quarter the width
    else if (this->subsampling == Subsampling::YUV_400)
      bytes += 0; // No chroma components
    else
      return -1; // Unknown subsampling

    if (this->planar &&
        (this->planeOrder == PlaneOrder::YUVA || this->planeOrder == PlaneOrder::YVUA))
      // There is an additional alpha plane. The alpha plane is not subsampled
      bytes += frameSize.width() * frameSize.height() * bytesPerSample; // Alpha plane
    if (!this->planar && this->subsampling == Subsampling::YUV_444 &&
        (this->packingOrder == PackingOrder::AYUV || this->packingOrder == PackingOrder::YUVA ||
         this->packingOrder == PackingOrder::VUYA))
      // There is an additional alpha plane. The alpha plane is not subsampled
      bytes += frameSize.width() * frameSize.height() * bytesPerSample; // Alpha plane
  }
  else
  {
    // This is a packed format with byte packing
    if (this->subsampling == Subsampling::YUV_422)
    {
      // All packing orders have 4 values per packed value (which has 2 Y samples)
      const auto bitsPerPixel = this->bitsPerSample * 4;
      return ((bitsPerPixel + 7) / 8) * (frameSize.width() / 2) * frameSize.height();
    }
    // This is a packed format. The added number of bytes might be lower because of the packing.
    if (this->subsampling == Subsampling::YUV_444)
    {
      auto bitsPerPixel = this->bitsPerSample * 3;
      if (this->packingOrder == PackingOrder::AYUV || this->packingOrder == PackingOrder::YUVA ||
          this->packingOrder == PackingOrder::VUYA)
        bitsPerPixel += this->bitsPerSample;
      return ((bitsPerPixel + 7) / 8) * frameSize.width() * frameSize.height();
    }
    // else if (subsampling == Subsampling::YUV_422 || subsampling == Subsampling::YUV_440)
    //{
    //  // All packing orders have 4 values per packed value (which has 2 Y samples)
    //  int bitsPerPixel = bitsPerSample * 4;
    //  return ((bitsPerPixel + 7) / 8) * (frameSize.width() / 2) * frameSize.height();
    //}
    // else if (subsampling == Subsampling::YUV_420)
    //{
    //  // All packing orders have 6 values per packed sample (which has 4 Y samples)
    //  int bitsPerPixel = bitsPerSample * 6;
    //  return ((bitsPerPixel + 7) / 8) * (frameSize.width() / 2) * (frameSize.height() / 2);
    //}
    // else
    //  return -1;  // Unknown subsampling
  }
  return bytes;
}

unsigned yuvPixelFormat::getNrPlanes() const
{
  if (this->subsampling == Subsampling::YUV_400)
    return 1;
  if (this->packingOrder == PackingOrder::AYUV || this->packingOrder == PackingOrder::YUVA ||
      this->packingOrder == PackingOrder::VUYA)
    return 4;
  return 3;
}

int yuvPixelFormat::getSubsamplingHor(Component component) const
{
  if (component == Component::Luma)
    return 1;
  if (this->subsampling == Subsampling::YUV_410 || this->subsampling == Subsampling::YUV_411)
    return 4;
  if (this->subsampling == Subsampling::YUV_422 || this->subsampling == Subsampling::YUV_420)
    return 2;
  return 1;
}
int yuvPixelFormat::getSubsamplingVer(Component component) const
{
  if (component == Component::Luma)
    return 1;
  if (this->subsampling == Subsampling::YUV_410)
    return 4;
  if (this->subsampling == Subsampling::YUV_420 || this->subsampling == Subsampling::YUV_440)
    return 2;
  return 1;
}
void yuvPixelFormat::setDefaultChromaOffset()
{
  this->chromaOffset[0] = 0;
  this->chromaOffset[1] = 0;
  if (this->subsampling == Subsampling::YUV_420)
    this->chromaOffset[1] = 1;
}

} // namespace YUV_Internals
