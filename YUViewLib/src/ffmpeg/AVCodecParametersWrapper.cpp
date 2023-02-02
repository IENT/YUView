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

#include "AVCodecParametersWrapper.h"

#include <common/Functions.h>
namespace FFmpeg
{

namespace
{

// AVCodecParameters is part of avcodec.
typedef struct AVCodecParameters_57_58_59
{
  AVMediaType                   codec_type;
  AVCodecID                     codec_id;
  uint32_t                      codec_tag;
  uint8_t *                     extradata;
  int                           extradata_size;
  int                           format;
  int64_t                       bit_rate;
  int                           bits_per_coded_sample;
  int                           bits_per_raw_sample;
  int                           profile;
  int                           level;
  int                           width;
  int                           height;
  Rational                      sample_aspect_ratio;
  AVFieldOrder                  field_order;
  AVColorRange                  color_range;
  AVColorPrimaries              color_primaries;
  AVColorTransferCharacteristic color_trc;
  AVColorSpace                  color_space;
  AVChromaLocation              chroma_location;
  int                           video_delay;

  // Actually, there is more here, but the variables above are the only we need.
} AVCodecParameters_57_58_59;

} // namespace

AVCodecParametersWrapper::AVCodecParametersWrapper(AVCodecParameters *p, LibraryVersion v)
{
  this->param  = p;
  this->libVer = v;
  this->update();
}

AVMediaType AVCodecParametersWrapper::getCodecType()
{
  this->update();
  return this->codec_type;
}

AVCodecID AVCodecParametersWrapper::getCodecID()
{
  this->update();
  return this->codec_id;
}

QByteArray AVCodecParametersWrapper::getExtradata()
{
  this->update();
  return this->extradata;
}

Size AVCodecParametersWrapper::getSize()
{
  this->update();
  return Size(this->width, this->height);
}

AVColorSpace AVCodecParametersWrapper::getColorspace()
{
  this->update();
  return this->color_space;
}

AVPixelFormat AVCodecParametersWrapper::getPixelFormat()
{
  this->update();
  return AVPixelFormat(this->format);
}

Ratio AVCodecParametersWrapper::getSampleAspectRatio()
{
  this->update();
  return {this->sample_aspect_ratio.num, this->sample_aspect_ratio.den};
}

StringPairVec AVCodecParametersWrapper::getInfoText() const
{
  if (this->param == nullptr)
    return {{"Codec parameters are nullptr", ""}};

  AVCodecParametersWrapper wrapper(this->param, this->libVer);

  StringPairVec info;
  info.push_back({"Codec Tag", std::to_string(wrapper.codec_tag)});
  info.push_back({"Format", std::to_string(wrapper.format)});
  info.push_back({"Bitrate", std::to_string(wrapper.bit_rate)});
  info.push_back({"Bits per coded sample", std::to_string(wrapper.bits_per_coded_sample)});
  info.push_back({"Bits per Raw sample", std::to_string(wrapper.bits_per_raw_sample)});
  info.push_back({"Profile", std::to_string(wrapper.profile)});
  info.push_back({"Level", std::to_string(wrapper.level)});
  info.push_back(
      {"Width/Height", std::to_string(this->width) + "/" + std::to_string(this->height)});
  info.push_back({"Sample aspect ratio",
                  std::to_string(wrapper.sample_aspect_ratio.num) + ":" +
                      std::to_string(wrapper.sample_aspect_ratio.den)});
  std::array fieldOrders = {"Unknown",
                            "Progressive",
                            "Top coded_first, top displayed first",
                            "Bottom coded first, bottom displayed first",
                            "Top coded first, bottom displayed first",
                            "Bottom coded first, top displayed first"};
  info.push_back({"Field Order",
                  fieldOrders.at(functions::clip(
                      static_cast<size_t>(this->codec_type), size_t(0), fieldOrders.size()))});
  std::array colorRanges = {"Unspecified",
                            "The normal 219*2^(n-8) MPEG YUV ranges",
                            "The normal 2^n-1 JPEG YUV ranges",
                            "Not part of ABI"};
  info.push_back({"Color Range",
                  colorRanges.at(functions::clip(
                      static_cast<size_t>(this->color_range), size_t(0), colorRanges.size()))});
  std::array colorPrimaries = {
      "Reserved",
      "BT709 / ITU-R BT1361 / IEC 61966-2-4 / SMPTE RP177 Annex B",
      "Unspecified",
      "Reserved",
      "BT470M / FCC Title 47 Code of Federal Regulations 73.682 (a)(20)",
      "BT470BG / ITU-R BT601-6 625 / ITU-R BT1358 625 / ITU-R BT1700 625 PAL & SECAM",
      "SMPTE170M / also ITU-R BT601-6 525 / ITU-R BT1358 525 / ITU-R BT1700 NTSC",
      "SMPTE240M",
      "FILM - colour filters using Illuminant C",
      "ITU-R BT2020",
      "SMPTE ST 428-1 (CIE 1931 XYZ)",
      "SMPTE ST 431-2 (2011)",
      "SMPTE ST 432-1 D65 (2010)",
      "Not part of ABI"};
  info.push_back(
      {"Color Primaries",
       colorPrimaries.at(functions::clip(
           static_cast<size_t>(wrapper.color_primaries), size_t(0), colorPrimaries.size()))});
  std::array colorTransfers = {
      "Reserved",
      "BT709 / ITU-R BT1361",
      "Unspecified",
      "Reserved",
      "Gamma22 / ITU-R BT470M / ITU-R BT1700 625 PAL & SECAM",
      "Gamma28 / ITU-R BT470BG",
      "SMPTE170M / ITU-R BT601-6 525 or 625 / ITU-R BT1358 525 or 625 / ITU-R BT1700 NTSC",
      "SMPTE240M",
      "Linear transfer characteristics",
      "Logarithmic transfer characteristic (100:1 range)",
      "Logarithmic transfer characteristic (100 * Sqrt(10) : 1 range)",
      "IEC 61966-2-4",
      "ITU-R BT1361 Extended Colour Gamut",
      "IEC 61966-2-1 (sRGB or sYCC)",
      "ITU-R BT2020 for 10-bit system",
      "ITU-R BT2020 for 12-bit system",
      "SMPTE ST 2084 for 10-, 12-, 14- and 16-bit systems",
      "SMPTE ST 428-1",
      "ARIB STD-B67, known as Hybrid log-gamma",
      "Not part of ABI"};
  info.push_back({"Color Transfer",
                  colorTransfers.at(functions::clip(
                      static_cast<size_t>(wrapper.color_trc), size_t(0), colorTransfers.size()))});
  std::array colorSpaces = {
      "RGB - order of coefficients is actually GBR, also IEC 61966-2-1 (sRGB)",
      "BT709 / ITU-R BT1361 / IEC 61966-2-4 xvYCC709 / SMPTE RP177 Annex B",
      "Unspecified",
      "Reserved",
      "FCC Title 47 Code of Federal Regulations 73.682 (a)(20)",
      "BT470BG / ITU-R BT601-6 625 / ITU-R BT1358 625 / ITU-R BT1700 625 PAL & "
      "SECAM / IEC 61966-2-4 xvYCC601",
      "SMPTE170M / ITU-R BT601-6 525 / ITU-R BT1358 525 / ITU-R BT1700 NTSC",
      "SMPTE240M",
      "YCOCG - Used by Dirac / VC-2 and H.264 FRext, see ITU-T SG16",
      "ITU-R BT2020 non-constant luminance system",
      "ITU-R BT2020 constant luminance system",
      "SMPTE 2085, Y'D'zD'x",
      "Not part of ABI"};
  info.push_back({"Color Space",
                  colorSpaces.at(functions::clip(
                      static_cast<size_t>(wrapper.color_space), size_t(0), colorSpaces.size()))});
  std::array chromaLocations = {
      "Unspecified",
      "Left / MPEG-2/4 4:2:0, H.264 default for 4:2:0",
      "Center / MPEG-1 4:2:0, JPEG 4:2:0, H.263 4:2:0",
      "Top Left / ITU-R 601, SMPTE 274M 296M S314M(DV 4:1:1), mpeg2 4:2:2",
      "Top",
      "Bottom Left",
      "Bottom",
      "Not part of ABI"};
  info.push_back(
      {"Chroma Location",
       chromaLocations.at(functions::clip(
           static_cast<size_t>(wrapper.chroma_location), size_t(0), chromaLocations.size()))});
  info.push_back({"Video Delay", std::to_string(wrapper.video_delay)});

  return info;
}

void AVCodecParametersWrapper::setClearValues()
{
  if (this->libVer.avformat.major == 57 || this->libVer.avformat.major == 58 ||
      this->libVer.avformat.major == 59)
  {
    auto p                   = reinterpret_cast<AVCodecParameters_57_58_59 *>(this->param);
    p->codec_type            = AVMEDIA_TYPE_UNKNOWN;
    p->codec_id              = AV_CODEC_ID_NONE;
    p->codec_tag             = 0;
    p->extradata             = nullptr;
    p->extradata_size        = 0;
    p->format                = 0;
    p->bit_rate              = 0;
    p->bits_per_coded_sample = 0;
    p->bits_per_raw_sample   = 0;
    p->profile               = 0;
    p->level                 = 0;
    p->width                 = 0;
    p->height                = 0;
    {
      Rational ratio;
      ratio.num              = 1;
      ratio.den              = 1;
      p->sample_aspect_ratio = ratio;
    }
    p->field_order     = AV_FIELD_UNKNOWN;
    p->color_range     = AVCOL_RANGE_UNSPECIFIED;
    p->color_primaries = AVCOL_PRI_UNSPECIFIED;
    p->color_trc       = AVCOL_TRC_UNSPECIFIED;
    p->color_space     = AVCOL_SPC_UNSPECIFIED;
    p->chroma_location = AVCHROMA_LOC_UNSPECIFIED;
    p->video_delay     = 0;
  }
  this->update();
}

void AVCodecParametersWrapper::setAVMediaType(AVMediaType type)
{
  if (this->libVer.avformat.major == 57 || this->libVer.avformat.major == 58 ||
      this->libVer.avformat.major == 59)
  {
    auto p           = reinterpret_cast<AVCodecParameters_57_58_59 *>(this->param);
    p->codec_type    = type;
    this->codec_type = type;
  }
}

void AVCodecParametersWrapper::setAVCodecID(AVCodecID id)
{
  if (this->libVer.avformat.major == 57 || this->libVer.avformat.major == 58 ||
      this->libVer.avformat.major == 59)
  {
    auto p         = reinterpret_cast<AVCodecParameters_57_58_59 *>(this->param);
    p->codec_id    = id;
    this->codec_id = id;
  }
}

void AVCodecParametersWrapper::setExtradata(QByteArray data)
{
  if (this->libVer.avformat.major == 57 || this->libVer.avformat.major == 58 ||
      this->libVer.avformat.major == 59)
  {
    this->extradata   = data;
    auto p            = reinterpret_cast<AVCodecParameters_57_58_59 *>(this->param);
    p->extradata      = reinterpret_cast<uint8_t *>(this->extradata.data());
    p->extradata_size = this->extradata.length();
  }
}

void AVCodecParametersWrapper::setSize(Size size)
{
  if (this->libVer.avformat.major == 57 || this->libVer.avformat.major == 58 ||
      this->libVer.avformat.major == 59)
  {
    auto p       = reinterpret_cast<AVCodecParameters_57_58_59 *>(this->param);
    p->width     = size.width;
    p->height    = size.height;
    this->width  = size.width;
    this->height = size.height;
  }
}

void AVCodecParametersWrapper::setAVPixelFormat(AVPixelFormat format)
{
  if (this->libVer.avformat.major == 57 || this->libVer.avformat.major == 58 ||
      this->libVer.avformat.major == 59)
  {
    auto p       = reinterpret_cast<AVCodecParameters_57_58_59 *>(this->param);
    p->format    = format;
    this->format = format;
  }
}

void AVCodecParametersWrapper::setProfileLevel(int profile, int level)
{
  if (this->libVer.avformat.major == 57 || this->libVer.avformat.major == 58 ||
      this->libVer.avformat.major == 59)
  {
    auto p        = reinterpret_cast<AVCodecParameters_57_58_59 *>(this->param);
    p->profile    = profile;
    p->level      = level;
    this->profile = profile;
    this->level   = level;
  }
}

void AVCodecParametersWrapper::setSampleAspectRatio(int num, int den)
{
  if (this->libVer.avformat.major == 57 || this->libVer.avformat.major == 58 ||
      this->libVer.avformat.major == 59)
  {
    auto     p = reinterpret_cast<AVCodecParameters_57_58_59 *>(param);
    Rational ratio;
    ratio.num                 = num;
    ratio.den                 = den;
    p->sample_aspect_ratio    = ratio;
    this->sample_aspect_ratio = ratio;
  }
}

void AVCodecParametersWrapper::update()
{
  if (this->param == nullptr)
    return;

  if (this->libVer.avformat.major == 56)
  {
    // This data structure does not exist in avformat major version 56.
    this->param = nullptr;
  }
  else if (this->libVer.avformat.major == 57 || this->libVer.avformat.major == 58 ||
           this->libVer.avformat.major == 59)
  {
    auto p = reinterpret_cast<AVCodecParameters_57_58_59 *>(this->param);

    this->codec_type            = p->codec_type;
    this->codec_id              = p->codec_id;
    this->codec_tag             = p->codec_tag;
    this->extradata             = QByteArray((const char *)p->extradata, p->extradata_size);
    this->format                = p->format;
    this->bit_rate              = p->bit_rate;
    this->bits_per_coded_sample = p->bits_per_coded_sample;
    this->bits_per_raw_sample   = p->bits_per_raw_sample;
    this->profile               = p->profile;
    this->level                 = p->level;
    this->width                 = p->width;
    this->height                = p->height;
    this->sample_aspect_ratio   = p->sample_aspect_ratio;
    this->field_order           = p->field_order;
    this->color_range           = p->color_range;
    this->color_primaries       = p->color_primaries;
    this->color_trc             = p->color_trc;
    this->color_space           = p->color_space;
    this->chroma_location       = p->chroma_location;
    this->video_delay           = p->video_delay;
  }
  else
    throw std::runtime_error("Invalid library version");
}

} // namespace FFmpeg