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

#include <iostream>

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
std::vector<PackingOrder> getSupportedPackingFormats(const Subsampling subsampling)
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

bool isDefaultChromaOffsetInXDirection(const Offset &offset)
{
  return offset.x == 0;
}

bool isDefaultChromaOffsetInYDirection(const Offset &offset, const Subsampling subsampling)
{
  if (subsampling == Subsampling::YUV_420)
    return offset.y == 1;
  return offset.y == 0;
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

std::optional<int> parseBitDepthText(const std::string &bitdepthStr)
{
  const auto bitDepth = functions::toInt(bitdepthStr);
  if (bitDepth && *bitDepth >= 8 && *bitDepth <= 16)
    return *bitDepth;
  return {};
}

Offset parseChromaOffsetText(const std::string &offsetXStr, const std::string &offsetYStr)
{
  Offset offset{};

  if (offsetXStr.substr(0, 2) == "Cx")
    offset.x = functions::toInt(offsetXStr.substr(2)).value_or(0);
  if (offsetYStr.substr(0, 2) == "Cy")
    offset.y = functions::toInt(offsetYStr.substr(2)).value_or(0);

  return offset;
}

std::optional<std::pair<PlaneOrder, bool>>
parsePlaneOrderAndUVInterleavedText(std::string_view text)
{
  const auto uvInterleaved = text.size() >= 4 && text.substr(text.size() - 4, 4) == "(IL)";

  if (uvInterleaved)
    text.remove_suffix(4);

  if (const auto planeOder = PlaneOrderMapper.getValue(text))
    return std::make_pair(*planeOder, uvInterleaved);

  return {};
}

std::string formatSubsamplingWithColons(const Subsampling &subsampling)
{
  const auto name = SubsamplingMapper.getName(subsampling);

  std::stringstream s;
  s << name.at(0) << ":" << name.at(1) << ":" << name.at(2);
  return s.str();
}

PixelFormatYUV::PixelFormatYUV(const std::string_view name)
{
  if (const auto predefinedFormat = PredefinedPixelFormatMapper.getValue(name))
    this->format = *predefinedFormat;

  std::regex strExpr(
    "([YUVA]{3,6}(?:\\(IL\\))?) (4:[4210]{1}:[4210]{1}) ([0-9]{1,2})-bit[ ]?([BL]{1}E)?[ "
    "]?(packed-B|packed)?[ ]?(Cx[0-9]+)?[ ]?(Cy[0-9]+)?");

  std::smatch sm;
  const auto  nameAsString = std::string(name);
  if (!std::regex_match(nameAsString, sm, strExpr))
    return;

  const auto subsampling = parseSubsamplingText(sm.str(2));
  if (!subsampling)
    return;

  const auto bitsPerSample = parseBitDepthText(sm.str(3));
  if (!bitsPerSample)
    return;

  const auto endianess =
    (bitsPerSample > 8 && sm.str(4) == "BE") ? Endianness::Big : Endianness::Little;

  const auto chromaOffset = parseChromaOffsetText(sm.str(6), sm.str(7));

  const auto packingIndicator = sm.str(5);
  const auto isPlanar         = packingIndicator.empty();

  const auto bytePacking = !isPlanar && packingIndicator == "packed-B";

  if (isPlanar)
  {
    const auto planeOrderAndUVInterleaved = parsePlaneOrderAndUVInterleavedText(sm.str(1));
    if (!planeOrderAndUVInterleaved)
      return;

    this->format = PlanarPixelFormat{*subsampling,
                                     *bitsPerSample,
                                     planeOrderAndUVInterleaved->first,
                                     endianess,
                                     chromaOffset,
                                     planeOrderAndUVInterleaved->second,
                                     bytePacking};
  }
  else
  {
    const auto packingOrder = PackingOrderMapper.getValue(sm.str(1));
    if (!packingOrder)
      return;

    this->format = PackedPixelFormat{
      *subsampling, *bitsPerSample, *packingOrder, endianess, chromaOffset, bytePacking};
  }
}

PixelFormatYUV::PixelFormatYUV(Subsampling subsampling,
                               int         bitsPerSample,
                               PlaneOrder  planeOrder,
                               Endianness  endianness,
                               Offset      chromaOffset,
                               bool        uvInterleaved,
                               bool        bytePacking)
{
  if (subsampling == Subsampling::YUV_420 && chromaOffset == Offset{0, 0})
    chromaOffset = Offset{0, 1};

  this->format = PlanarPixelFormat{
    subsampling, bitsPerSample, planeOrder, endianness, chromaOffset, uvInterleaved, bytePacking};
}

PixelFormatYUV::PixelFormatYUV(Subsampling  subsampling,
                               int          bitsPerSample,
                               PackingOrder packingOrder,
                               Endianness   endianness,
                               Offset       chromaOffset,
                               bool         bytePacking)
{
  if (subsampling == Subsampling::YUV_420 && chromaOffset == Offset{0, 0})
    chromaOffset = Offset{0, 1};

  this->format = PackedPixelFormat{
    subsampling, bitsPerSample, packingOrder, endianness, chromaOffset, bytePacking};
}

PixelFormatYUV::PixelFormatYUV(PredefinedPixelFormat predefinedPixelFormat)
    : format(predefinedPixelFormat)
{
}

std::optional<PredefinedPixelFormat> PixelFormatYUV::getPredefinedFormat() const
{
  if (std::holds_alternative<PredefinedPixelFormat>(this->format))
    return std::get<PredefinedPixelFormat>(this->format);
  return {};
}

bool PixelFormatYUV::isValid() const
{
  if (std::holds_alternative<PredefinedPixelFormat>(this->format))
    return true;

  if (std::holds_alternative<PackedPixelFormat>(this->format))
  {
    const auto &packedPixelFormat = std::get<PackedPixelFormat>(this->format);
    // Check the packing mode
    if ((packedPixelFormat.packingOrder == PackingOrder::YUV ||
         packedPixelFormat.packingOrder == PackingOrder::YVU ||
         packedPixelFormat.packingOrder == PackingOrder::AYUV ||
         packedPixelFormat.packingOrder == PackingOrder::YUVA ||
         packedPixelFormat.packingOrder == PackingOrder::VUYA) &&
        packedPixelFormat.subsampling != Subsampling::YUV_444)
      return false;
    if ((packedPixelFormat.packingOrder == PackingOrder::UYVY ||
         packedPixelFormat.packingOrder == PackingOrder::VYUY ||
         packedPixelFormat.packingOrder == PackingOrder::YUYV ||
         packedPixelFormat.packingOrder == PackingOrder::YVYU) &&
        packedPixelFormat.subsampling != Subsampling::YUV_422)
      return false;
    if (packedPixelFormat.packingOrder == PackingOrder::UNKNOWN)
      return false;
    /*if ((packedPixelFormat.packingOrder == Packing_YYYYUV || packedPixelFormat.packingOrder ==
      Packing_YYUYYV || packedPixelFormat.packingOrder == Packing_UYYVYY ||
      packedPixelFormat.packingOrder == Packing_VYYUYY) && packedPixelFormat.subsampling ==
      Subsampling::YUV_420) return false;*/
    if (packedPixelFormat.subsampling == Subsampling::YUV_420 ||
        packedPixelFormat.subsampling == Subsampling::YUV_440 ||
        packedPixelFormat.subsampling == Subsampling::YUV_410 ||
        packedPixelFormat.subsampling == Subsampling::YUV_411 ||
        packedPixelFormat.subsampling == Subsampling::YUV_400)
      // No support for packed formats with this subsampling (yet)
      return false;
  }

  const auto hasChromaComponents = this->getSubsampling() != Subsampling::YUV_400;
  if (hasChromaComponents)
  {
    const auto chromaOffset = this->getChromaOffset();
    if (chromaOffset.x < 0 ||
        chromaOffset.x > getMaxPossibleChromaOffsetValues(true, this->getSubsampling()))
      return false;
    if (chromaOffset.y < 0 ||
        chromaOffset.y > getMaxPossibleChromaOffsetValues(false, this->getSubsampling()))
      return false;
  }

  if (this->getBitsPerSample() < 7)
    return false;

  return true;
}

int64_t PixelFormatYUV::bytesPerFrame(const Size &frameSize) const
{
  if (!frameSize.isValid())
    return 0;

  if (const auto predefinedFormat = std::get_if<PredefinedPixelFormat>(&this->format))
  {
    if (*predefinedFormat == PredefinedPixelFormat::V210)
    {
      // 422 10 bit with 6 Y values per 16 bytes. Width is rounded up to a multiple of 48.
      // Although there is a weird expception to this in the standard.
      auto roundedUpWidth = (((frameSize.width + 48 - 1) / 48) * 48);
      return frameSize.height * roundedUpWidth * 16 / 6;
    }
    return 0;
  }

  if (!this->isBytePacking())
  {
    // Add the bytes of the 3 (or 4) planes.
    // The numbers are the same for planar and packed formats as long as no byte packing is used.
    // The only difference is the order of the bytes in memory.

    const auto    bytesPerSample = (this->getBitsPerSample() + 7) / 8;
    const int64_t bytesLuma      = frameSize.width * frameSize.height * bytesPerSample;

    int64_t bytesChroma = 0;
    switch (this->getSubsampling())
    {
    case Subsampling::YUV_444:
      bytesChroma = 2 * bytesLuma;
      break;
    case Subsampling::YUV_422:
    case Subsampling::YUV_440:
      bytesChroma = bytesLuma;
      break;
    case Subsampling::YUV_420:
    case Subsampling::YUV_411:
      bytesChroma = bytesLuma / 2;
      break;
    case Subsampling::YUV_410:
      bytesChroma = bytesLuma / 8;
      break;
    case Subsampling::YUV_400:
      bytesChroma = 0;
      break;
    default:
      return 0;
    }

    const auto bytesAlpha = (this->hasAlpha()) ? bytesLuma : 0;

    return bytesLuma + bytesChroma + bytesAlpha;
  }

  if (const auto packedFormat = std::get_if<PackedPixelFormat>(&this->format))
  {
    if (packedFormat->subsampling == Subsampling::YUV_422)
    {
      // All packing orders have 4 values per packed value (which has 2 Y samples)
      const auto bitsPerPixel = packedFormat->bitsPerSample * 4;
      return ((bitsPerPixel + 7) / 8) * (frameSize.width / 2) * frameSize.height;
    }
    // This is a packed format. The added number of bytes might be lower because of the packing.
    if (packedFormat->subsampling == Subsampling::YUV_444)
    {
      auto bitsPerPixel = packedFormat->bitsPerSample * 3;
      if (packedFormat->packingOrder == PackingOrder::AYUV ||
          packedFormat->packingOrder == PackingOrder::YUVA ||
          packedFormat->packingOrder == PackingOrder::VUYA)
        bitsPerPixel += packedFormat->bitsPerSample;
      return ((bitsPerPixel + 7) / 8) * frameSize.width * frameSize.height;
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

  if (const auto planarFormat = std::get_if<PlanarPixelFormat>(&this->format))
  {
    // PYUV ... to be implemented
  }

  return 0;
}

// Generate a unique name for the YUV format
std::string PixelFormatYUV::getName() const
{
  if (!this->isValid())
    return "Invalid";

  std::stringstream ss;

  std::visit(
    [](const auto &format)
    {
      using T = std::decay_t<decltype(format)>;

      if constexpr (std::is_same_v<T, PlanarPixelFormat>)
      {
        // ss << PlaneOrderMapper.getName(format.planeOrder);
        // if (format.uvPlanesInterleaved)
        //   ss << "(IL)";
        std::cout << "A";
      }
      else if constexpr (std::is_same_v<T, PackedPixelFormat>)
        // ss << PackingOrderMapper.getName(format.packingOrder);
        std::cout << "B";
      else if constexpr (std::is_same_v<T, PredefinedPixelFormat>)
      {
        // ss << PredefinedPixelFormatMapper.getName(format);
        std::cout << "C";
        //return;
      }
      // else
      //   static_assert(false, "non-exhaustive visitor!");

      // ss << " " << formatSubsamplingWithColons(format.subsampling);
      // ss << " " << format.bitsPerSample << "-bit";

      // if (format.bitsPerSample > 8)
      //   ss << ((format.endianness == Endianness::Big) ? " BE" : " LE");

      // if constexpr (std::is_same_v<T, PackedPixelFormat>)
      //   if (format.subsampling != Subsampling::YUV_400)
      //     ss << (format.bytePacking ? " packed-B" : " packed");

      // // Add the Chroma offsets (if it is not the default offset)
      // if (!isDefaultChromaOffsetInXDirection(format.chromaOffset))
      //   ss << " Cx" << format.chromaOffset.x;
      // if (!isDefaultChromaOffsetInYDirection(format.chromaOffset, format.subsampling))
      //   ss << " Cy" << format.chromaOffset.y;
    },
    this->format);

  return ss.str();
}

unsigned PixelFormatYUV::getNrPlanes() const
{
  if (const auto predefinedFormat = std::get_if<PredefinedPixelFormat>(&this->format))
  {
    if (*predefinedFormat == PredefinedPixelFormat::V210)
      return 3;
    return 0;
  }

  if (this->getSubsampling() == Subsampling::YUV_400)
    return 1;

  return this->hasAlpha() ? 4 : 3;
}

Subsampling PixelFormatYUV::getSubsampling() const
{
  return std::visit(
    [](const auto &&format) -> Subsampling
    {
      using T = std::decay_t<decltype(format)>;
      if constexpr (std::is_same_v<T, PredefinedPixelFormat>)
      {
        if (format == PredefinedPixelFormat::V210)
          return Subsampling::YUV_422;
        return Subsampling::UNKNOWN;
      }
      else
        return format.subsampling;
    },
    this->format);
}

int PixelFormatYUV::getSubsamplingHor(Component component) const
{
  auto subsampling = this->getSubsampling();

  if (component == Component::Luma)
    return 1;
  if (subsampling == Subsampling::YUV_410 || subsampling == Subsampling::YUV_411)
    return 4;
  if (subsampling == Subsampling::YUV_422 || subsampling == Subsampling::YUV_420)
    return 2;
  return 1;
}

int PixelFormatYUV::getSubsamplingVer(Component component) const
{
  auto subsampling = this->getSubsampling();

  if (component == Component::Luma)
    return 1;
  if (subsampling == Subsampling::YUV_410)
    return 4;
  if (subsampling == Subsampling::YUV_420 || subsampling == Subsampling::YUV_440)
    return 2;
  return 1;
}

bool PixelFormatYUV::isChromaSubsampled() const
{
  auto subsampling = this->getSubsampling();
  return subsampling != Subsampling::YUV_444;
}

unsigned PixelFormatYUV::getBitsPerSample() const
{
  return std::visit(
    [](const auto &&format) -> unsigned
    {
      using T = std::decay_t<decltype(format)>;
      if constexpr (std::is_same_v<T, PredefinedPixelFormat>)
      {
        if (format == PredefinedPixelFormat::V210)
          return 10;
        return 0;
      }
      else
        return format.bitsPerSample;
    },
    this->format);
}

Endianness PixelFormatYUV::getEndianness() const
{
  return std::visit(
    [](const auto &&format) -> Endianness
    {
      using T = std::decay_t<decltype(format)>;
      if constexpr (std::is_same_v<T, PredefinedPixelFormat>)
      {
        if (format == PredefinedPixelFormat::V210)
          return Endianness::Little;
        return Endianness::Little;
      }
      else
        return format.endianess;
    },
    this->format);
}

bool PixelFormatYUV::isPlanar() const
{
  return std::holds_alternative<PlanarPixelFormat>(this->format);
}

std::optional<PlaneOrder> PixelFormatYUV::getPlaneOrder() const
{
  if (const auto planarFormat = std::get_if<PlanarPixelFormat>(&this->format))
    return planarFormat->planeOrder;
  return {};
}

std::optional<PackingOrder> PixelFormatYUV::getPackingOrder() const
{
  if (const auto packedFormat = std::get_if<PackedPixelFormat>(&this->format))
    return packedFormat->packingOrder;
  return {};
}

bool PixelFormatYUV::hasAlpha() const
{
  return std::visit(
    [](const auto &&format) -> bool
    {
      using T = std::decay_t<decltype(format)>;
      if constexpr (std::is_same_v<T, PredefinedPixelFormat>)
      {
        if (format == PredefinedPixelFormat::V210)
          return false;
        return false;
      }
      else if constexpr (std::is_same_v<T, PlanarPixelFormat>)
        return format.planeOrder == PlaneOrder::YUVA || format.planeOrder == PlaneOrder::YVUA;
      else if constexpr (std::is_same_v<T, PackedPixelFormat>)
        return format.packingOrder == PackingOrder::AYUV ||
               format.packingOrder == PackingOrder::YUVA ||
               format.packingOrder == PackingOrder::VUYA;
      else
        return false;
    },
    this->format);
}

Offset PixelFormatYUV::getChromaOffset() const
{
  return std::visit(
    [](const auto &&format) -> Offset
    {
      using T = std::decay_t<decltype(format)>;
      if constexpr (std::is_same_v<T, PredefinedPixelFormat>)
      {
        if (format == PredefinedPixelFormat::V210)
          return Offset({0, 0});
        return Offset({0, 0});
      }
      else
        return format.chromaOffset;
    },
    this->format);
}

bool PixelFormatYUV::isBytePacking() const
{
  return std::visit(
    [](const auto &&format) -> bool
    {
      using T = std::decay_t<decltype(format)>;
      if constexpr (std::is_same_v<T, PredefinedPixelFormat>)
      {
        if (format == PredefinedPixelFormat::V210)
          return true;
        return false;
      }
      else
        return format.bytePacking;
    },
    this->format);
}

} // namespace video::yuv
