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

#include "PixelFormatYUV.h"

#include <regex>

namespace video::yuv
{

void getColorConversionCoefficients(ColorConversion colorConversion, int RGBConv[5])
{
  // The conversion parameters for the components of the different supported YUV->RGB conversions
  // The first index is the index of the ColorConversion enum. The second index is [Y, cRV, cGU,
  // cGV, cBU].
  const int yuvRgbConvCoeffs[6][5] = {
      {76309, 117489, -13975, -34925, 138438}, // BT709_LimitedRange
      {65536, 103206, -12276, -30679, 121609}, // BT709_FullRange
      {76309, 104597, -25675, -53279, 132201}, // BT601_LimitedRange
      {65536, 91881, -22553, -46802, 116130},  // BT601_FullRange
      {76309, 110014, -12277, -42626, 140363}, // BT2020_LimitedRange
      {65536, 96639, -10784, -37444, 123299}   // BT2020_FullRange
  };
  const auto index = ColorConversionMapper.indexOf(colorConversion);
  for (unsigned i = 0; i < 5; i++)
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
std::vector<PackingOrder> getSupportedPackingFormats(Subsampling subsampling)
{
  if (subsampling == Subsampling::YUV_422)
    return std::vector<PackingOrder>(
        {PackingOrder::UYVY, PackingOrder::VYUY, PackingOrder::YUYV, PackingOrder::YVYU});
  if (subsampling == Subsampling::YUV_444)
    return std::vector<PackingOrder>({PackingOrder::YUV,
                                      PackingOrder::YVU,
                                      PackingOrder::AYUV,
                                      PackingOrder::YUVA,
                                      PackingOrder::VUYA});

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

std::optional<Subsampling> parseSubsamplingText(const std::string_view text)
{
  if (text.size() != 5)
    return {};
  if (text.at(1) != ':' || text.at(3) != ':')
    return {};

  const std::string subsamplingName = {text.at(0), text.at(2), text.at(4)};
  return SubsamplingMapper.getValue(subsamplingName);
}

std::string formatSubsamplingWithColons(const Subsampling &subsampling)
{
  const auto name = SubsamplingMapper.getName(subsampling);

  std::stringstream s;
  s << name.at(0) << ":" << name.at(1) << ":" << name.at(2);
  return s.str();
}

PixelFormatYUV::PixelFormatYUV(const std::string &name)
{
  if (auto predefinedFormat = PredefinedPixelFormatMapper.getValue(name))
  {
    if (*predefinedFormat == PredefinedPixelFormat::V210)
      this->predefinedPixelFormat = predefinedFormat;
  }

  std::regex strExpr(
      "([YUVA]{3,6}(?:\\(IL\\))?) (4:[4210]{1}:[4210]{1}) ([0-9]{1,2})-bit[ ]?([BL]{1}E)?[ "
      "]?(packed-B|packed)?[ ]?(Cx[0-9]+)?[ ]?(Cy[0-9]+)?");

  std::smatch sm;
  if (!std::regex_match(name, sm, strExpr))
    return;

  try
  {
    PixelFormatYUV newFormat;

    // Is this a packed format or not?
    auto packed      = sm.str(5);
    newFormat.planar = packed.empty();
    if (!newFormat.planar)
      newFormat.bytePacking = (packed == "packed-B");

    // Parse the YUV order (planar or packed)
    if (newFormat.planar)
    {
      auto yuvName = sm.str(1);
      if (yuvName.size() >= 4 && yuvName.substr(yuvName.size() - 4, 4) == "(IL)")
      {
        newFormat.uvInterleaved = true;
        yuvName.erase(yuvName.size() - 4, 4);
      }

      if (auto po = PlaneOrderMapper.getValue(yuvName))
        newFormat.planeOrder = *po;
    }
    else
    {
      auto packingName = sm.str(1);
      if (auto po = PackingOrderMapper.getValue(packingName))
        newFormat.packingOrder = *po;
    }

    if (auto subsampling = parseSubsamplingText(sm.str(2)))
      newFormat.subsampling = *subsampling;

    // Get the bit depth
    {
      auto   bitdepthStr = sm.str(3);
      size_t sz;
      int    bitDepth = std::stoi(bitdepthStr, &sz);
      if (sz > 0 && bitDepth >= 8 && bitDepth <= 32)
        newFormat.bitsPerSample = bitDepth;
    }

    // Get the endianness. If not in the name, assume LE
    newFormat.bigEndian = (sm.str(4) == "BE");

    // Get the chroma offsets
    newFormat.setDefaultChromaOffset();
    auto chromaOffsetXStr = sm.str(6);
    if (chromaOffsetXStr.substr(0, 2) == "Cx")
    {
      size_t sz;
      auto   offsetX = std::stoi(chromaOffsetXStr.substr(2), &sz);
      if (sz > 0 && offsetX >= 0)
        newFormat.chromaOffset.x = offsetX;
    }

    auto chromaOffsetYStr = sm.str(7);
    if (chromaOffsetYStr.substr(0, 2) == "Cy")
    {
      size_t sz;
      auto   offsetY = std::stoi(chromaOffsetYStr.substr(2), &sz);
      if (sz > 0 && offsetY >= 0)
        newFormat.chromaOffset.y = offsetY;
    }

    // Check if the format is valid.
    if (newFormat.isValid())
    {
      // Set all the values from the new format
      this->subsampling   = newFormat.subsampling;
      this->bitsPerSample = newFormat.bitsPerSample;
      this->bigEndian     = newFormat.bigEndian;
      this->planar        = newFormat.planar;
      this->planeOrder    = newFormat.planeOrder;
      this->uvInterleaved = newFormat.uvInterleaved;
      this->packingOrder  = newFormat.packingOrder;
      this->bytePacking   = newFormat.bytePacking;
      this->chromaOffset  = newFormat.chromaOffset;
    }
  }
  catch (const std::exception &)
  {
  }
}

PixelFormatYUV::PixelFormatYUV(Subsampling subsampling,
                               unsigned    bitsPerSample,
                               PlaneOrder  planeOrder,
                               bool        bigEndian,
                               Offset      chromaOffset,
                               bool        uvInterleaved)
    : subsampling(subsampling), bitsPerSample(bitsPerSample), bigEndian(bigEndian), planar(true),
      chromaOffset(chromaOffset), planeOrder(planeOrder), uvInterleaved(uvInterleaved)
{
  this->setDefaultChromaOffset();
}

PixelFormatYUV::PixelFormatYUV(Subsampling  subsampling,
                               unsigned     bitsPerSample,
                               PackingOrder packingOrder,
                               bool         bytePacking,
                               bool         bigEndian,
                               Offset       chromaOffset)
    : subsampling(subsampling), bitsPerSample(bitsPerSample), bigEndian(bigEndian), planar(false),
      chromaOffset(chromaOffset), uvInterleaved(false), packingOrder(packingOrder),
      bytePacking(bytePacking)
{
  this->setDefaultChromaOffset();
}

PixelFormatYUV::PixelFormatYUV(PredefinedPixelFormat predefinedPixelFormat)
    : predefinedPixelFormat(predefinedPixelFormat)
{
}

std::optional<PredefinedPixelFormat> PixelFormatYUV::getPredefinedFormat() const
{
  return this->predefinedPixelFormat;
}

bool PixelFormatYUV::isValid() const
{
  if (this->predefinedPixelFormat.has_value())
    return true;

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
    if (this->chromaOffset.x < 0 ||
        this->chromaOffset.x > getMaxPossibleChromaOffsetValues(true, this->subsampling))
      return false;
    if (this->chromaOffset.y < 0 ||
        this->chromaOffset.y > getMaxPossibleChromaOffsetValues(false, this->subsampling))
      return false;
  }
  // Check the bit depth
  if (this->bitsPerSample < 7)
    return false;
  return true;
}

bool PixelFormatYUV::canConvertToRGB(Size imageSize, std::string *whyNot) const
{
  if (this->predefinedPixelFormat.has_value())
    return true;
  if (!this->isValid())
  {
    if (whyNot)
      whyNot->append("Invalid format");
    return false;
  }

  // Check the bit depth
  const int bps        = this->bitsPerSample;
  bool      canConvert = true;
  if (bps < 8 || bps > 32)
  {
    if (whyNot)
    {
      std::stringstream ss;
      ss << "The currently set bit depth " << bps << " is not supported.\n";
      whyNot->append(ss.str());
    }
    canConvert = false;
  }
  if (imageSize.width % this->getSubsamplingHor() != 0)
  {
    if (whyNot)
    {
      std::stringstream ss;
      ss << "The item width " << imageSize.width
         << " must be divisible by the horizontal subsampling factor " << this->getSubsamplingHor()
         << ".\n";
      whyNot->append(ss.str());
    }
    canConvert = false;
  }
  if (imageSize.height % this->getSubsamplingVer() != 0)
  {
    if (whyNot)
    {
      std::stringstream ss;
      ss << "The item height " << imageSize.height
         << " must be divisible by the vertical subsampling factor " << this->getSubsamplingVer()
         << ".\n";
      whyNot->append(ss.str());
    }
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

int64_t PixelFormatYUV::bytesPerFrame(const Size &frameSize) const
{
  if (this->predefinedPixelFormat)
  {
    if (*this->predefinedPixelFormat == PredefinedPixelFormat::V210)
    {
      // 422 10 bit with 6 Y values per 16 bytes. Width is rounded up to a multiple of 48.
      // Although there is a weird expception to this in the standard.
      auto roundedUpWidth = (((frameSize.width + 48 - 1) / 48) * 48);
      return frameSize.height * roundedUpWidth * 16 / 6;
    }
    return -1;
  }

  int64_t bytes = 0;
  const auto bytesPerSample = get_min_standard_bytes(this->bitsPerSample); // Round to bytes

  if (this->planar || !this->bytePacking)
  {
    // Add the bytes of the 3 (or 4) planes.
    // This also works for packed formats without byte packing. For these formats the number of
    // bytes are identical to the not packed formats, the bytes are just sorted in another way.

    bytes += frameSize.width * frameSize.height * bytesPerSample; // Luma plane
    if (this->subsampling == Subsampling::YUV_444)
      bytes += frameSize.width * frameSize.height * bytesPerSample * 2; // U/V planes
    else if (this->subsampling == Subsampling::YUV_422 || this->subsampling == Subsampling::YUV_440)
      bytes += (frameSize.width / 2) * frameSize.height * bytesPerSample *
               2; // U/V planes, half the width
    else if (this->subsampling == Subsampling::YUV_420)
      bytes += (frameSize.width / 2) * (frameSize.height / 2) * bytesPerSample *
               2; // U/V planes, half the width and height
    else if (this->subsampling == Subsampling::YUV_410)
      bytes += (frameSize.width / 4) * (frameSize.height / 4) * bytesPerSample *
               2; // U/V planes, half the width and height
    else if (this->subsampling == Subsampling::YUV_411)
      bytes += (frameSize.width / 4) * frameSize.height * bytesPerSample *
               2; // U/V planes, quarter the width
    else if (this->subsampling == Subsampling::YUV_400)
      bytes += 0; // No chroma components
    else
      return -1; // Unknown subsampling

    if (this->planar &&
        (this->planeOrder == PlaneOrder::YUVA || this->planeOrder == PlaneOrder::YVUA))
      // There is an additional alpha plane. The alpha plane is not subsampled
      bytes += frameSize.width * frameSize.height * bytesPerSample; // Alpha plane
    if (!this->planar && this->subsampling == Subsampling::YUV_444 &&
        (this->packingOrder == PackingOrder::AYUV || this->packingOrder == PackingOrder::YUVA ||
         this->packingOrder == PackingOrder::VUYA))
      // There is an additional alpha plane. The alpha plane is not subsampled
      bytes += frameSize.width * frameSize.height * bytesPerSample; // Alpha plane
  }
  else
  {
    // This is a packed format with byte packing
    if (this->subsampling == Subsampling::YUV_422)
    {
      // All packing orders have 4 values per packed value (which has 2 Y samples)
      return 4 * bytesPerSample * (frameSize.width / 2) * frameSize.height;
    }
    // This is a packed format. The added number of bytes might be lower because of the packing.
    if (this->subsampling == Subsampling::YUV_444)
    {
      auto channels = 3;
      if (this->packingOrder == PackingOrder::AYUV || this->packingOrder == PackingOrder::YUVA ||
          this->packingOrder == PackingOrder::VUYA)
        channels += 1;
      return channels * bytesPerSample * frameSize.width * frameSize.height;
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

// Generate a unique name for the YUV format
std::string PixelFormatYUV::getName() const
{
  if (!this->isValid())
    return "Invalid";
  if (this->predefinedPixelFormat)
  {
    if (*this->predefinedPixelFormat == PredefinedPixelFormat::V210)
      return "V210";
    return "Invalid";
  }

  std::stringstream ss;

  if (this->planar)
  {
    ss << PlaneOrderMapper.getName(this->planeOrder);

    if (this->uvInterleaved)
      ss << "(IL)";
  }
  else
    ss << PackingOrderMapper.getName(this->packingOrder);

  ss << " " << formatSubsamplingWithColons(this->subsampling) << " " << this->bitsPerSample
     << "-bit";

  // Add the endianness (if the bit depth is greater 8)
  if (this->bitsPerSample > 8)
    ss << ((this->bigEndian) ? " BE" : " LE");

  if (!this->planar && this->subsampling != Subsampling::YUV_400)
    ss << (this->bytePacking ? " packed-B" : " packed");

  // Add the Chroma offsets (if it is not the default offset)
  if (!isDefaultChromaFormat(this->chromaOffset.x, true, this->subsampling))
    ss << " Cx" << this->chromaOffset.x;
  if (!isDefaultChromaFormat(this->chromaOffset.y, false, this->subsampling))
    ss << " Cy" << this->chromaOffset.y;

  return ss.str();
}

unsigned PixelFormatYUV::getNrPlanes() const
{
  if (this->predefinedPixelFormat)
  {
    if (*this->predefinedPixelFormat == PredefinedPixelFormat::V210)
      return 3;
    return 0;
  }

  if (this->subsampling == Subsampling::YUV_400)
    return 1;
  if (this->packingOrder == PackingOrder::AYUV || this->packingOrder == PackingOrder::YUVA ||
      this->packingOrder == PackingOrder::VUYA)
    return 4;
  return 3;
}

Subsampling PixelFormatYUV::getSubsampling() const
{
  if (this->predefinedPixelFormat)
  {
    if (*this->predefinedPixelFormat == PredefinedPixelFormat::V210)
      return Subsampling::YUV_422;
    return Subsampling::UNKNOWN;
  }

  return this->subsampling;
}

int PixelFormatYUV::getSubsamplingHor(Component component) const
{
  auto sub = this->getSubsampling();

  if (component == Component::Luma)
    return 1;
  if (sub == Subsampling::YUV_410 || sub == Subsampling::YUV_411)
    return 4;
  if (sub == Subsampling::YUV_422 || sub == Subsampling::YUV_420)
    return 2;
  return 1;
}

int PixelFormatYUV::getSubsamplingVer(Component component) const
{
  auto sub = this->getSubsampling();

  if (component == Component::Luma)
    return 1;
  if (sub == Subsampling::YUV_410)
    return 4;
  if (sub == Subsampling::YUV_420 || sub == Subsampling::YUV_440)
    return 2;
  return 1;
}

void PixelFormatYUV::setDefaultChromaOffset()
{
  this->chromaOffset = Offset({0, 0});
  if (this->getSubsampling() == Subsampling::YUV_420)
    this->chromaOffset.y = 1;
}

bool PixelFormatYUV::isChromaSubsampled() const
{
  auto sub = this->getSubsampling();
  return sub != Subsampling::YUV_444;
}

unsigned PixelFormatYUV::getBitsPerSample() const
{
  if (this->predefinedPixelFormat)
  {
    if (*this->predefinedPixelFormat == PredefinedPixelFormat::V210)
      return 10;
    return 0;
  }

  return this->bitsPerSample;
}

bool PixelFormatYUV::isBigEndian() const
{
  if (this->predefinedPixelFormat)
  {
    if (*this->predefinedPixelFormat == PredefinedPixelFormat::V210)
      return false;
    return false;
  }

  return this->bigEndian;
}

bool PixelFormatYUV::isPlanar() const
{
  if (this->predefinedPixelFormat)
  {
    if (*this->predefinedPixelFormat == PredefinedPixelFormat::V210)
      return false;
    return false;
  }

  return this->planar;
}

bool PixelFormatYUV::hasAlpha() const
{
  if (this->predefinedPixelFormat)
  {
    if (*this->predefinedPixelFormat == PredefinedPixelFormat::V210)
      return false;
    return false;
  }

  if (this->planar)
    return this->planeOrder == PlaneOrder::YUVA || this->planeOrder == PlaneOrder::YVUA;
  else
    return this->packingOrder == PackingOrder::AYUV || this->packingOrder == PackingOrder::YUVA ||
           this->packingOrder == PackingOrder::VUYA;
}

Offset PixelFormatYUV::getChromaOffset() const
{
  if (this->predefinedPixelFormat)
  {
    if (*this->predefinedPixelFormat == PredefinedPixelFormat::V210)
      return Offset({0, 0});
    return Offset({0, 0});
  }

  return this->chromaOffset;
}

bool PixelFormatYUV::isBytePacking() const
{
  if (this->predefinedPixelFormat)
  {
    if (*this->predefinedPixelFormat == PredefinedPixelFormat::V210)
      return true;
    return false;
  }

  return this->bytePacking;
}

} // namespace video::yuv
