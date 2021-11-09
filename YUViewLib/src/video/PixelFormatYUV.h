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
#include <common/Typedef.h>

#include "PixelFormat.h"

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
by 255/219 for Y and 255/112 for U/V In this software color conversion is performed with 16bit
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

const auto ColorConversionMapper =
    EnumMapper<ColorConversion>({{ColorConversion::BT709_LimitedRange, "ITU-R.BT709"},
                                 {ColorConversion::BT709_FullRange, "ITU-R.BT709 Full Range"},
                                 {ColorConversion::BT601_LimitedRange, "ITU-R.BT601"},
                                 {ColorConversion::BT601_FullRange, "ITU-R.BT601 Full Range"},
                                 {ColorConversion::BT2020_LimitedRange, "ITU-R.BT2020"},
                                 {ColorConversion::BT2020_FullRange, "ITU-R.BT2020 Full Range"}});

void getColorConversionCoefficients(ColorConversion colorConversion, int RGBConv[5]);

// How to perform up-sampling (chroma subsampling)
enum class ChromaInterpolation
{
  NearestNeighbor,
  Bilinear,
  Interstitial
};

const auto ChromaInterpolationMapper =
    EnumMapper<ChromaInterpolation>({{ChromaInterpolation::NearestNeighbor, "Nearest Neighbor"},
                                     {ChromaInterpolation::Bilinear, "Bilinear"},
                                     {ChromaInterpolation::Interstitial, "Interstitial"}});

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

const auto PredefinedPixelFormatMapper =
    EnumMapper<PredefinedPixelFormat>({{PredefinedPixelFormat::V210, "V210"}});

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

const auto PackingOrderMapper = EnumMapper<PackingOrder>({{PackingOrder::YUV, "YUV"},
                                                          {PackingOrder::YVU, "YVU"},
                                                          {PackingOrder::AYUV, "AYUV"},
                                                          {PackingOrder::YUVA, "YUVA"},
                                                          {PackingOrder::VUYA, "VUYA"},
                                                          {PackingOrder::UYVY, "UYVY"},
                                                          {PackingOrder::VYUY, "VYUY"},
                                                          {PackingOrder::YUYV, "YUYV"},
                                                          {PackingOrder::YVYU, "YVYU"}});

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

const auto SubsamplingMapper = EnumMapper<Subsampling>({{Subsampling::YUV_444, "444", "4:4:4"},
                                                        {Subsampling::YUV_422, "422", "4:2:2"},
                                                        {Subsampling::YUV_420, "420", "4:2:0"},
                                                        {Subsampling::YUV_440, "440", "4:4:0"},
                                                        {Subsampling::YUV_410, "410", "4:1:0"},
                                                        {Subsampling::YUV_411, "411", "4:1:1"},
                                                        {Subsampling::YUV_400, "400", "4:0:0"}});

int getMaxPossibleChromaOffsetValues(bool horizontal, Subsampling subsampling);
std::vector<PackingOrder> getSupportedPackingFormats(Subsampling subsampling);

enum class PlaneOrder
{
  YUV,
  YVU,
  YUVA,
  YVUA
};

const auto PlaneOrderMapper = EnumMapper<PlaneOrder>({{PlaneOrder::YUV, "YUV"},
                                                      {PlaneOrder::YVU, "YVU"},
                                                      {PlaneOrder::YUVA, "YUVA"},
                                                      {PlaneOrder::YVUA, "YVUA"}});

const auto BitDepthList = std::vector<unsigned>({8, 9, 10, 12, 14, 16});

// This class defines a specific YUV format with all properties like pixels per sample, subsampling
// of chroma components and so on.
class PixelFormatYUV
{
public:
  PixelFormatYUV() = default;
  PixelFormatYUV(const std::string &name); // Set the pixel format by name. The name should have the
                                           // format that is returned by getName().
  PixelFormatYUV(Subsampling subsampling,
                 unsigned    bitsPerSample,
                 PlaneOrder  planeOrder    = PlaneOrder::YUV,
                 bool        bigEndian     = false,
                 Offset      chromaOffset  = {},
                 bool        uvInterleaved = false);
  PixelFormatYUV(Subsampling  subsampling,
                 unsigned     bitsPerSample,
                 PackingOrder packingOrder,
                 bool         bytePacking  = false,
                 bool         bigEndian    = false,
                 Offset       chromaOffset = {});
  PixelFormatYUV(PredefinedPixelFormat predefinedPixelFormat);

  std::optional<PredefinedPixelFormat> getPredefinedFormat() const;

  bool        isValid() const;
  bool        canConvertToRGB(Size frameSize, std::string *whyNot = nullptr) const;
  int64_t     bytesPerFrame(const Size &frameSize) const;
  std::string getName() const;
  unsigned    getNrPlanes() const;
  void        setDefaultChromaOffset();

  Subsampling getSubsampling() const;
  int         getSubsamplingHor(Component component = Component::Chroma) const;
  int         getSubsamplingVer(Component component = Component::Chroma) const;
  bool        isChromaSubsampled() const;

  unsigned getBitsPerSample() const;
  bool     isBigEndian() const;
  bool     isPlanar() const;
  bool     hasAlpha() const;

  Offset getChromaOffset() const;

  PlaneOrder getPlaneOrder() const { return this->planeOrder; }
  bool       isUVInterleaved() const { return this->uvInterleaved; }

  PackingOrder getPackingOrder() const { return this->packingOrder; }
  bool         isBytePacking() const;

  bool operator==(const PixelFormatYUV &a) const { return getName() == a.getName(); }
  bool operator!=(const PixelFormatYUV &a) const { return getName() != a.getName(); }
  bool operator==(const std::string &a) const { return getName() == a; }
  bool operator!=(const std::string &a) const { return getName() != a; }

private:
  // If this is set, the format is defined according to a specific standard and does not
  // conform to the definition below (using subsampling/bitDepht/planes/packed)
  // If this is set, none of the values below matter.
  std::optional<PredefinedPixelFormat> predefinedPixelFormat;

  Subsampling subsampling{Subsampling::YUV_420};
  unsigned    bitsPerSample{};
  bool        bigEndian{};
  bool        planar{};

  // The chroma offset in x and y direction. The vales (0...4) define the offsets [0, 1/2, 1, 3/2]
  // samples towards the right and bottom.
  Offset chromaOffset;

  PlaneOrder planeOrder{PlaneOrder::YUV};
  bool       uvInterleaved{}; //< If set, the UV (and A if present) planes are interleaved

  // if planar is not set
  PackingOrder packingOrder{PackingOrder::YUV};
  bool         bytePacking{};
};

} // namespace video::yuv
