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

#include <QObject>
#include <QString>

// The YUV_Internals namespace. We use this namespace because of the dialog. We want to be able to pass a yuvPixelFormat to the dialog and keep the
// global namespace clean but we are not able to use nested classes because of the Q_OBJECT macro. So the dialog and the yuvPixelFormat is inside
// of this namespace.
namespace YUV_Internals
{
  
enum class Component
{
  Luma = 0,
  Chroma = 1
};

enum class ColorConversion
{
  BT709_LimitedRange,
  BT709_FullRange,
  BT601_LimitedRange,
  BT601_FullRange,
  BT2020_LimitedRange,
  BT2020_FullRange,
};
const auto colorConversionList = QList<ColorConversion>() << ColorConversion::BT709_LimitedRange << ColorConversion::BT709_FullRange << ColorConversion::BT601_LimitedRange << ColorConversion::BT601_FullRange << ColorConversion::BT2020_LimitedRange << ColorConversion::BT2020_FullRange;
const auto colorConversionNameList = QStringList() << "ITU-R.BT709" << "ITU-R.BT709 Full Range" << "ITU-R.BT601" << "ITU-R.BT601 Full Range" << "ITU-R.BT2020" << "ITU-R.BT2020 Full Range";
void getColorConversionCoefficients(ColorConversion colorConversion, int RGBConv[5]);

// How to perform up-sampling (chroma subsampling)
enum class ChromaInterpolation
{
  NearestNeighbor,
  Bilinear,
  Interstitial
};
const auto chromaInterpolationList = QList<ChromaInterpolation>() << ChromaInterpolation::NearestNeighbor << ChromaInterpolation::Bilinear << ChromaInterpolation::Interstitial;
const auto chromaInterpolationNameList = QStringList() << "Nearest Neighbor" << "Bilinear" << "Interstitial";

class MathParameters
{
public:
  MathParameters() : scale(1), offset(128), invert(false) {}
  MathParameters(int scale, int offset, bool invert) : scale(scale), offset(offset), invert(invert) {}
  // Do we need to apply any transform to the raw YUV data before conversion to RGB?
  bool mathRequired() const { return scale != 1 || invert; }

  int scale, offset;
  bool invert;
};

enum class PackingOrder
{
  YUV,      // 444
  YVU,      // 444
  AYUV,     // 444
  YUVA,     // 444
  VUYA,     // 444
  UYVY,     // 422
  VYUY,     // 422
  YUYV,     // 422
  YVYU,     // 422
  //YYYYUV,   // 420
  //YYUYYV,   // 420
  //UYYVYY,   // 420
  //VYYUYY    // 420
  UNKNOWN
};
const auto packingOrderList = QList<PackingOrder>() << PackingOrder::YUV << PackingOrder::YVU << PackingOrder::AYUV << PackingOrder::YUVA << PackingOrder::VUYA << PackingOrder::UYVY << PackingOrder::VYUY << PackingOrder::YUYV << PackingOrder::YVYU;
const auto packingOrderNameList = QStringList() << "YUV" << "YVU" << "AYUV" << "YUVA" << "VUYA" << "UYVY" << "VYUY" << "YUYV" << "YVYU";
QString getPackingFormatString(PackingOrder packing);

enum class Subsampling
{
  YUV_444,  // No subsampling
  YUV_422,  // Chroma: half horizontal resolution
  YUV_420,  // Chroma: half vertical and horizontal resolution
  YUV_440,  // Chroma: half vertical resolution
  YUV_410,  // Chroma: quarter vertical, quarter horizontal resolution
  YUV_411,  // Chroma: quarter horizontal resolution
  YUV_400,  // Luma only
  UNKNOWN
};
const auto subsamplingList = QList<Subsampling>() << Subsampling::YUV_444 << Subsampling::YUV_422 << Subsampling::YUV_420 << Subsampling::YUV_440 << Subsampling::YUV_410 << Subsampling::YUV_411 << Subsampling::YUV_400;
const auto subsamplingNameList = QStringList() << "444" << "422" << "420" << "440" << "410" << "411" << "400";
const auto subsamplingTextList = QStringList() << "4:4:4" << "4:2:2" << "4:2:0" << "4:4:0" << "4:1:0" << "4:1:1" << "4:0:0";
int getMaxPossibleChromaOffsetValues(bool horizontal, Subsampling subsampling);
QList<PackingOrder> getSupportedPackingFormats(Subsampling subsampling);
QString subsamplingToString(Subsampling type);
Subsampling stringToSubsampling(QString typeString);

enum class PlaneOrder
{
  YUV,
  YVU,
  YUVA,
  YVUA
};
const auto planeOrderList = QList<PlaneOrder>() << PlaneOrder::YUV << PlaneOrder::YVU << PlaneOrder::YUVA << PlaneOrder::YVUA;
const auto planeOrderNameList = QStringList() << "YUV" << "YVU" << "YUVA" << "YVUA";

const QList<int> bitDepthList = QList<int>() << 8 << 9 << 10 << 12 << 14 << 16;

// This class defines a specific YUV format with all properties like pixels per sample, subsampling of chroma
// components and so on.
class yuvPixelFormat
{
public:
  yuvPixelFormat() = default;
  yuvPixelFormat(const QString &name);  // Set the pixel format by name. The name should have the format that is returned by getName().
  yuvPixelFormat(Subsampling subsampling, int bitsPerSample, PlaneOrder planeOrder=PlaneOrder::YUV, bool bigEndian=false);
  yuvPixelFormat(Subsampling subsampling, int bitsPerSample, PackingOrder packingOrder, bool bytePacking=false, bool bigEndian=false);

  bool isValid() const;
  bool canConvertToRGB(QSize frameSize, QString *whyNot = nullptr) const;
  int64_t bytesPerFrame(const QSize &frameSize) const;
  QString getName() const;
  int getSubsamplingHor() const;
  int getSubsamplingVer() const;
  void setDefaultChromaOffset();
  bool isChromaSubsampled() const { return subsampling != Subsampling::YUV_444; }

  bool operator==(const yuvPixelFormat& a) const { return getName() == a.getName(); }
  bool operator!=(const yuvPixelFormat& a) const { return getName() != a.getName(); }
  bool operator==(const QString& a) const { return getName() == a; }
  bool operator!=(const QString& a) const { return getName() != a; }

  Subsampling subsampling {Subsampling::YUV_444};
  int bitsPerSample {-1};
  bool bigEndian {false};
  bool planar {false};
  
  // The chroma offset in x and y direction. The vales (0...4) define the offsets [0, 1/2, 1, 3/2] samples towards the right and bottom.
  int chromaOffset[2] {0, 0};

  PlaneOrder planeOrder {PlaneOrder::YUV};
  bool uvInterleaved {false};       //< If set, the UV (and A if present) planes are interleaved

  // if planar is not set
  PackingOrder packingOrder {PackingOrder::YUV};
  bool bytePacking {false};
};

} // namespace YUV_Internals
