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

#pragma once

#include <common/EnumMapper.h>
#include <common/Offset.h>
#include <common/Size.h>
#include <common/Typedef.h>

#include <video/PixelFormat.h>

// The YUV_Internals namespace. We use this namespace because of the dialog. We want to be able to
// pass a PixelFormatYUV to the dialog and keep the global namespace clean but we are not able to
// use nested classes because of the Q_OBJECT macro. So the dialog and the PixelFormatYUV is inside
// of this namespace.
namespace video::yuv
{

enum class Component
{
  Luma   = 0,
  Chroma = 1
};

/*
kr/kg/kb matrix (Rec. ITU-T H.264 03/2010, p. 379):
R = Y                  + V*(1-Kr)
G = Y - U*(1-Kb)*Kb/Kg - V*(1-Kr)*Kr/Kg
B = Y + U*(1-Kb)
To respect value range of Y in [16:235] and U/V in [16:240], the matrix entries need to be scaled
by 255/219 for Y and 255/224 for U/V In this software color conversion is performed with 16bit
precision. Thus, further scaling with 2^16 is performed to get all factors as integers.
*/
enum class ColorConversion
{
  BT709_LimitedRange,
  BT709_FullRange,
  BT601_LimitedRange,
  BT601_FullRange,
  BT2020_LimitedRange,
  BT2020_FullRange,
};

constexpr EnumMapper<ColorConversion, 6> ColorConversionMapper = {
  std::make_pair(ColorConversion::BT709_LimitedRange, "ITU-R.BT709"),
  std::make_pair(ColorConversion::BT709_FullRange, "ITU-R.BT709 Full Range"),
  std::make_pair(ColorConversion::BT601_LimitedRange, "ITU-R.BT601"),
  std::make_pair(ColorConversion::BT601_FullRange, "ITU-R.BT601 Full Range"),
  std::make_pair(ColorConversion::BT2020_LimitedRange, "ITU-R.BT2020"),
  std::make_pair(ColorConversion::BT2020_FullRange, "ITU-R.BT2020 Full Range")};

void getColorConversionCoefficients(ColorConversion colorConversion, int RGBConv[5]);

// How to perform up-sampling (chroma subsampling)
enum class ChromaInterpolation
{
  NearestNeighbor,
  Bilinear,
  Interstitial
};

constexpr EnumMapper<ChromaInterpolation, 3> ChromaInterpolationMapper = {
  std::make_pair(ChromaInterpolation::NearestNeighbor, "Nearest Neighbor"),
  std::make_pair(ChromaInterpolation::Bilinear, "Bilinear"),
  std::make_pair(ChromaInterpolation::Interstitial, "Interstitial")};

class MathParameters
{
public:
  MathParameters() = default;
  MathParameters(int scale, int offset, bool invert) : scale(scale), offset(offset), invert(invert)
  {
  }
  // Do we need to apply any transform to the raw YUV data before conversion to RGB?
  bool mathRequired() const { return scale != 1 || invert; }

  int  scale{1};
  int  offset{128};
  bool invert{};
};

enum class PredefinedPixelFormat
{
  // https://developer.apple.com/library/archive/technotes/tn2162/_index.html#//apple_ref/doc/uid/DTS40013070-CH1-TNTAG8-V210__4_2_2_COMPRESSION_TYPE
  // Packed 422 format with 12 10 bit values in 16 bytes
  V210
};

constexpr EnumMapper<PredefinedPixelFormat, 1> PredefinedPixelFormatMapper = {
  std::make_pair(PredefinedPixelFormat::V210, "V210")};

enum class PackingOrder
{
  YUV,  // 444
  YVU,  // 444
  AYUV, // 444
  YUVA, // 444
  VUYA, // 444
  UYVY, // 422
  VYUY, // 422
  YUYV, // 422
  YVYU, // 422
  // YYYYUV,   // 420
  // YYUYYV,   // 420
  // UYYVYY,   // 420
  // VYYUYY    // 420
  UNKNOWN
};

constexpr EnumMapper<PackingOrder, 9> PackingOrderMapper = {
  std::make_pair(PackingOrder::YUV, "YUV"),
  std::make_pair(PackingOrder::YVU, "YVU"),
  std::make_pair(PackingOrder::AYUV, "AYUV"),
  std::make_pair(PackingOrder::YUVA, "YUVA"),
  std::make_pair(PackingOrder::VUYA, "VUYA"),
  std::make_pair(PackingOrder::UYVY, "UYVY"),
  std::make_pair(PackingOrder::VYUY, "VYUY"),
  std::make_pair(PackingOrder::YUYV, "YUYV"),
  std::make_pair(PackingOrder::YVYU, "YVYU")};

enum class Subsampling
{
  YUV_444, // No subsampling
  YUV_422, // Chroma: half horizontal resolution
  YUV_420, // Chroma: half vertical and horizontal resolution
  YUV_440, // Chroma: half vertical resolution
  YUV_410, // Chroma: quarter vertical, quarter horizontal resolution
  YUV_411, // Chroma: quarter horizontal resolution
  YUV_400, // Luma only
  UNKNOWN
};

constexpr EnumMapper<Subsampling, 7> SubsamplingMapper = {
  std::make_pair(Subsampling::YUV_444, "444"),
  std::make_pair(Subsampling::YUV_422, "422"),
  std::make_pair(Subsampling::YUV_420, "420"),
  std::make_pair(Subsampling::YUV_440, "440"),
  std::make_pair(Subsampling::YUV_410, "410"),
  std::make_pair(Subsampling::YUV_411, "411"),
  std::make_pair(Subsampling::YUV_400, "400")};

std::string formatSubsamplingWithColons(const Subsampling &subsampling);

int getMaxPossibleChromaOffsetValues(bool horizontal, Subsampling subsampling);
std::vector<PackingOrder> getSupportedPackingFormats(Subsampling subsampling);

enum class PlaneOrder
{
  YUV,
  YVU,
  YUVA,
  YVUA
};

constexpr EnumMapper<PlaneOrder, 4> PlaneOrderMapper = {std::make_pair(PlaneOrder::YUV, "YUV"),
                                                        std::make_pair(PlaneOrder::YVU, "YVU"),
                                                        std::make_pair(PlaneOrder::YUVA, "YUVA"),
                                                        std::make_pair(PlaneOrder::YVUA, "YVUA")};

const auto BitDepthList = std::vector<unsigned>({8, 9, 10, 12, 14, 16});

// This class defines a specific YUV format with all properties like pixels per sample, subsampling
// of chroma components and so on.
class PixelFormatYUV
{
public:
  PixelFormatYUV() = default;
  PixelFormatYUV(const std::string_view name);
  PixelFormatYUV(Subsampling subsampling,
                 int         bitsPerSample,
                 PlaneOrder  planeOrder          = PlaneOrder::YUV,
                 Endianness  endianness          = Endianness::Little,
                 Offset      chromaOffset        = {},
                 bool        uvPlanesInterleaved = false,
                 bool        bytePacking         = false);
  PixelFormatYUV(Subsampling  subsampling,
                 int          bitsPerSample,
                 PackingOrder packingOrder,
                 Endianness   endianness   = Endianness::Little,
                 Offset       chromaOffset = {},
                 bool         bytePacking  = false);
  PixelFormatYUV(PredefinedPixelFormat predefinedPixelFormat);

  std::optional<PredefinedPixelFormat> getPredefinedFormat() const;

  bool        isValid() const;
  int64_t     bytesPerFrame(const Size &frameSize) const;
  std::string getName() const;
  unsigned    getNrPlanes() const;

  Subsampling getSubsampling() const;
  int         getSubsamplingHor(Component component = Component::Chroma) const;
  int         getSubsamplingVer(Component component = Component::Chroma) const;
  bool        isChromaSubsampled() const;

  unsigned   getBitsPerSample() const;
  Endianness getEndianness() const;
  bool       hasAlpha() const;

  bool                        isPlanar() const;
  std::optional<PlaneOrder>   getPlaneOrder() const;
  std::optional<PackingOrder> getPackingOrder() const;

  Offset getChromaOffset() const;

  bool operator==(const PixelFormatYUV &a) const { return getName() == a.getName(); }
  bool operator!=(const PixelFormatYUV &a) const { return getName() != a.getName(); }
  bool operator==(const std::string &a) const { return getName() == a; }
  bool operator!=(const std::string &a) const { return getName() != a; }
       operator bool() const { return this->isValid(); }

private:
  bool isBytePacking() const;

  struct PlanarPixelFormat
  {
    Subsampling subsampling{Subsampling::YUV_420};
    int         bitsPerSample{};
    PlaneOrder  planeOrder{PlaneOrder::YUV};
    Endianness  endianness{Endianness::Little};
    Offset      chromaOffset{};
    bool        uvPlanesInterleaved{};
    bool        bytePacking{};
  };

  struct PackedPixelFormat
  {
    Subsampling  subsampling{Subsampling::YUV_420};
    int          bitsPerSample{};
    PackingOrder packingOrder{PackingOrder::YUV};
    Endianness   endianness{Endianness::Little};
    Offset       chromaOffset{};
    bool         bytePacking{};
  };

  std::variant<PlanarPixelFormat, PackedPixelFormat, PredefinedPixelFormat> format{};
};

} // namespace video::yuv
