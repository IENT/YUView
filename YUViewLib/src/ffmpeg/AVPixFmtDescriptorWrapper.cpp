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

#include "AVPixFmtDescriptorWrapper.h"

using Subsampling    = video::yuv::Subsampling;
using PlaneOrder     = video::yuv::PlaneOrder;
using PixelFormatYUV = video::yuv::PixelFormatYUV;

using namespace std::rel_ops;

namespace FFmpeg
{

namespace
{

typedef struct AVComponentDescriptor_54
{
  uint16_t plane : 2;
  uint16_t step_minus1 : 3;
  uint16_t offset_plus1 : 3;
  uint16_t shift : 3;
  uint16_t depth_minus1 : 4;
} AVComponentDescriptor_54;

typedef struct AVPixFmtDescriptor_54
{
  const char *             name;
  uint8_t                  nb_components;
  uint8_t                  log2_chroma_w;
  uint8_t                  log2_chroma_h;
  uint8_t                  flags;
  AVComponentDescriptor_54 comp[4];
  const char *             alias;
} AVPixFmtDescriptor_54;

typedef struct AVComponentDescriptor_55_56
{
  int plane;
  int step;
  int offset;
  int shift;
  int depth;

  // deprectaed
  int step_minus1;
  int depth_minus1;
  int offset_plus1;
} AVComponentDescriptor_55_56;

typedef struct AVPixFmtDescriptor_55
{
  const char *                name;
  uint8_t                     nb_components;
  uint8_t                     log2_chroma_w;
  uint8_t                     log2_chroma_h;
  uint64_t                    flags;
  AVComponentDescriptor_55_56 comp[4];
  const char *                alias;
} AVPixFmtDescriptor_55;

typedef struct AVPixFmtDescriptor_56
{
  const char *                name;
  uint8_t                     nb_components;
  uint8_t                     log2_chroma_w;
  uint8_t                     log2_chroma_h;
  uint64_t                    flags;
  AVComponentDescriptor_55_56 comp[4];
  const char *                alias;
} AVPixFmtDescriptor_56;

typedef struct AVComponentDescriptor_57
{
  int plane;
  int step;
  int offset;
  int shift;
  int depth;
} AVComponentDescriptor_57;

typedef struct AVPixFmtDescriptor_57
{
  const char *             name;
  uint8_t                  nb_components;
  uint8_t                  log2_chroma_w;
  uint8_t                  log2_chroma_h;
  uint64_t                 flags;
  AVComponentDescriptor_57 comp[4];
  const char *             alias;
} AVPixFmtDescriptor_57;

AVPixFmtDescriptorWrapper::Flags parseFlags(uint8_t flagsValue)
{
  AVPixFmtDescriptorWrapper::Flags flags;
  flags.bigEndian      = flagsValue & (1 << 0);
  flags.pallette       = flagsValue & (1 << 1);
  flags.bitwisePacked  = flagsValue & (1 << 2);
  flags.hwAccelerated  = flagsValue & (1 << 3);
  flags.planar         = flagsValue & (1 << 4);
  flags.rgb            = flagsValue & (1 << 5);
  flags.pseudoPallette = flagsValue & (1 << 6);
  flags.hasAlphaPlane  = flagsValue & (1 << 7);
  flags.bayerPattern   = flagsValue & (1 << 8);
  flags.floatValues    = flagsValue & (1 << 9);
  return flags;
}

bool flagsSupported(const AVPixFmtDescriptorWrapper::Flags &flags)
{
  // We don't support any of these
  if (flags.pallette)
    return false;
  if (flags.hwAccelerated)
    return false;
  if (flags.pseudoPallette)
    return false;
  if (flags.bayerPattern)
    return false;
  if (flags.floatValues)
    return false;
  return true;
}

} // namespace

AVPixFmtDescriptorWrapper::AVPixFmtDescriptorWrapper(AVPixFmtDescriptor *descriptor,
                                                     LibraryVersion      libVer)
{
  if (libVer.avutil.major == 54)
  {
    auto p              = reinterpret_cast<AVPixFmtDescriptor_54 *>(descriptor);
    this->name          = QString(p->name);
    this->nb_components = p->nb_components;
    this->log2_chroma_w = p->log2_chroma_w;
    this->log2_chroma_h = p->log2_chroma_h;
    this->flags         = parseFlags(p->flags);

    for (unsigned i = 0; i < 4; i++)
    {
      this->comp[i].plane  = p->comp[i].plane;
      this->comp[i].step   = p->comp[i].step_minus1 + 1;
      this->comp[i].offset = p->comp[i].offset_plus1 - 1;
      this->comp[i].shift  = p->comp[i].shift;
      this->comp[i].depth  = p->comp[i].depth_minus1 + 1;
    }

    aliases = QString(p->alias);
  }
  else if (libVer.avutil.major == 55)
  {
    auto p              = reinterpret_cast<AVPixFmtDescriptor_55 *>(descriptor);
    this->name          = QString(p->name);
    this->nb_components = p->nb_components;
    this->log2_chroma_w = p->log2_chroma_w;
    this->log2_chroma_h = p->log2_chroma_h;
    this->flags         = parseFlags(p->flags);

    for (unsigned i = 0; i < 4; i++)
    {
      this->comp[i].plane  = p->comp[i].plane;
      this->comp[i].step   = p->comp[i].step;
      this->comp[i].offset = p->comp[i].offset;
      this->comp[i].shift  = p->comp[i].shift;
      this->comp[i].depth  = p->comp[i].depth;
    }

    aliases = QString(p->alias);
  }
  else if (libVer.avutil.major == 56)
  {
    auto p              = reinterpret_cast<AVPixFmtDescriptor_56 *>(descriptor);
    this->name          = QString(p->name);
    this->nb_components = p->nb_components;
    this->log2_chroma_w = p->log2_chroma_w;
    this->log2_chroma_h = p->log2_chroma_h;
    this->flags         = parseFlags(p->flags);

    for (unsigned i = 0; i < 4; i++)
    {
      this->comp[i].plane  = p->comp[i].plane;
      this->comp[i].step   = p->comp[i].step;
      this->comp[i].offset = p->comp[i].offset;
      this->comp[i].shift  = p->comp[i].shift;
      this->comp[i].depth  = p->comp[i].depth;
    }

    aliases = QString(p->alias);
  }
  else if (libVer.avutil.major == 57)
  {
    auto p              = reinterpret_cast<AVPixFmtDescriptor_57 *>(descriptor);
    this->name          = QString(p->name);
    this->nb_components = p->nb_components;
    this->log2_chroma_w = p->log2_chroma_w;
    this->log2_chroma_h = p->log2_chroma_h;
    this->flags         = parseFlags(p->flags);

    for (unsigned i = 0; i < 4; i++)
    {
      this->comp[i].plane  = p->comp[i].plane;
      this->comp[i].step   = p->comp[i].step;
      this->comp[i].offset = p->comp[i].offset;
      this->comp[i].shift  = p->comp[i].shift;
      this->comp[i].depth  = p->comp[i].depth;
    }

    aliases = QString(p->alias);
  }
}

video::RawFormat AVPixFmtDescriptorWrapper::getRawFormat() const
{
  return this->flags.rgb ? video::RawFormat::RGB : video::RawFormat::YUV;
}

PixelFormatYUV AVPixFmtDescriptorWrapper::getPixelFormatYUV() const
{
  if (this->getRawFormat() == video::RawFormat::RGB || !flagsSupported(this->flags))
    return {};

  Subsampling subsampling;
  if (this->nb_components == 1)
    subsampling = Subsampling::YUV_400;
  else if (this->log2_chroma_w == 0 && this->log2_chroma_h == 0)
    subsampling = Subsampling::YUV_444;
  else if (this->log2_chroma_w == 1 && this->log2_chroma_h == 0)
    subsampling = Subsampling::YUV_422;
  else if (this->log2_chroma_w == 1 && this->log2_chroma_h == 1)
    subsampling = Subsampling::YUV_420;
  else if (this->log2_chroma_w == 0 && this->log2_chroma_h == 1)
    subsampling = Subsampling::YUV_440;
  else if (this->log2_chroma_w == 2 && this->log2_chroma_h == 2)
    subsampling = Subsampling::YUV_410;
  else if (this->log2_chroma_w == 0 && this->log2_chroma_h == 2)
    subsampling = Subsampling::YUV_411;
  else
    return {};

  PlaneOrder planeOrder;
  if (this->nb_components == 1)
    planeOrder = PlaneOrder::YUV;
  else if (this->nb_components == 3 && !this->flags.hasAlphaPlane)
    planeOrder = PlaneOrder::YUV;
  else if (this->nb_components == 4 && this->flags.hasAlphaPlane)
    planeOrder = PlaneOrder::YUVA;
  else
    return {};

  int bitsPerSample = comp[0].depth;
  for (int i = 1; i < this->nb_components; i++)
    if (comp[i].depth != bitsPerSample)
      // Varying bit depths for components is not supported
      return {};

  if (this->flags.bitwisePacked || !this->flags.planar)
    // Maybe this could be supported but I don't think that any decoder actually uses this.
    // If you encounter a format that does not work because of this check please let us know.
    return {};

  return PixelFormatYUV(subsampling, bitsPerSample, planeOrder, this->flags.bigEndian);
}

video::rgb::PixelFormatRGB AVPixFmtDescriptorWrapper::getRGBPixelFormat() const
{
  if (this->getRawFormat() == video::RawFormat::YUV || !flagsSupported(this->flags))
    return {};

  auto bitsPerSample = comp[0].depth;
  for (int i = 1; i < nb_components; i++)
    if (comp[i].depth != bitsPerSample)
      // Varying bit depths for components is not supported
      return {};

  if (this->flags.bitwisePacked)
    // Maybe this could be supported but I don't think that any decoder actually uses this.
    // If you encounter a format that does not work because of this check please let us know.
    return {};

  // The only possible order of planes seems to be RGB(A)
  auto dataLayout = this->flags.planar ? video::DataLayout::Planar : video::DataLayout::Packed;
  auto alphaMode =
      this->flags.hasAlphaPlane ? video::rgb::AlphaMode::Last : video::rgb::AlphaMode::None;
  auto endianness = this->flags.bigEndian ? video::Endianness::Big : video::Endianness::Little;

  return video::rgb::PixelFormatRGB(
      bitsPerSample, dataLayout, video::rgb::ChannelOrder::RGB, alphaMode, endianness);
}

bool AVPixFmtDescriptorWrapper::setValuesFromPixelFormatYUV(PixelFormatYUV fmt)
{
  if (fmt.getPlaneOrder() == PlaneOrder::YVU || fmt.getPlaneOrder() == PlaneOrder::YVUA)
    return false;

  if (fmt.getSubsampling() == Subsampling::YUV_422)
  {
    this->log2_chroma_w = 1;
    this->log2_chroma_h = 0;
  }
  else if (fmt.getSubsampling() == Subsampling::YUV_422)
  {
    this->log2_chroma_w = 1;
    this->log2_chroma_h = 0;
  }
  else if (fmt.getSubsampling() == Subsampling::YUV_420)
  {
    this->log2_chroma_w = 1;
    this->log2_chroma_h = 1;
  }
  else if (fmt.getSubsampling() == Subsampling::YUV_440)
  {
    this->log2_chroma_w = 0;
    this->log2_chroma_h = 1;
  }
  else if (fmt.getSubsampling() == Subsampling::YUV_410)
  {
    this->log2_chroma_w = 2;
    this->log2_chroma_h = 2;
  }
  else if (fmt.getSubsampling() == Subsampling::YUV_411)
  {
    this->log2_chroma_w = 0;
    this->log2_chroma_h = 2;
  }
  else if (fmt.getSubsampling() == Subsampling::YUV_400)
    this->nb_components = 1;
  else
    return false;

  this->nb_components = fmt.getSubsampling() == Subsampling::YUV_400 ? 1 : 3;

  this->flags.bigEndian = fmt.isBigEndian();
  this->flags.planar    = fmt.isPlanar();
  if (fmt.getPlaneOrder() == PlaneOrder::YUVA)
    this->flags.hasAlphaPlane = true;

  for (int i = 0; i < this->nb_components; i++)
  {
    this->comp[i].plane  = i;
    this->comp[i].step   = (fmt.getBitsPerSample() > 8) ? 2 : 1;
    this->comp[i].offset = 0;
    this->comp[i].shift  = 0;
    this->comp[i].depth  = fmt.getBitsPerSample();
  }
  return true;
}

bool AVPixFmtDescriptorWrapper::Flags::operator==(
    const AVPixFmtDescriptorWrapper::Flags &other) const
{
  return this->bigEndian == other.bigEndian && this->pallette == other.pallette &&
         this->bitwisePacked == other.bitwisePacked && this->hwAccelerated == other.hwAccelerated &&
         this->planar == other.planar && this->rgb == other.rgb &&
         this->pseudoPallette == other.pseudoPallette &&
         this->hasAlphaPlane == other.hasAlphaPlane && this->bayerPattern == other.bayerPattern &&
         this->floatValues == other.floatValues;
}

bool AVPixFmtDescriptorWrapper::operator==(const AVPixFmtDescriptorWrapper &other)
{
  if (this->nb_components != other.nb_components)
    return false;
  if (this->log2_chroma_w != other.log2_chroma_w)
    return false;
  if (this->log2_chroma_h != other.log2_chroma_h)
    return false;
  if (this->flags != other.flags)
    return false;

  for (int i = 0; i < this->nb_components; i++)
  {
    if (this->comp[i].plane != other.comp[i].plane)
      return false;
    if (this->comp[i].step != other.comp[i].step)
      return false;
    if (this->comp[i].offset != other.comp[i].offset)
      return false;
    if (this->comp[i].shift != other.comp[i].shift)
      return false;
    if (this->comp[i].depth != other.comp[i].depth)
      return false;
  }
  return true;
}

} // namespace FFmpeg