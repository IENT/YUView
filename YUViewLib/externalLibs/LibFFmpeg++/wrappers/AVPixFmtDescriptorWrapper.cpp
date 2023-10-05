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

using namespace std::rel_ops;

namespace LibFFmpeg
{

namespace
{

struct AVComponentDescriptor_54
{
  uint16_t plane : 2;
  uint16_t step_minus1 : 3;
  uint16_t offset_plus1 : 3;
  uint16_t shift : 3;
  uint16_t depth_minus1 : 4;
};

struct AVPixFmtDescriptor_54
{
  const char *             name;
  uint8_t                  nb_components;
  uint8_t                  log2_chroma_w;
  uint8_t                  log2_chroma_h;
  uint8_t                  flags;
  AVComponentDescriptor_54 comp[4];
  const char *             alias;
};

struct AVComponentDescriptor_55_56
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
};

struct AVPixFmtDescriptor_55
{
  const char *                name;
  uint8_t                     nb_components;
  uint8_t                     log2_chroma_w;
  uint8_t                     log2_chroma_h;
  uint64_t                    flags;
  AVComponentDescriptor_55_56 comp[4];
  const char *                alias;
};

struct AVPixFmtDescriptor_56
{
  const char *                name;
  uint8_t                     nb_components;
  uint8_t                     log2_chroma_w;
  uint8_t                     log2_chroma_h;
  uint64_t                    flags;
  AVComponentDescriptor_55_56 comp[4];
  const char *                alias;
};

struct AVComponentDescriptor_57
{
  int plane;
  int step;
  int offset;
  int shift;
  int depth;
};

struct AVPixFmtDescriptor_57_58
{
  const char *             name;
  uint8_t                  nb_components;
  uint8_t                  log2_chroma_w;
  uint8_t                  log2_chroma_h;
  uint64_t                 flags;
  AVComponentDescriptor_57 comp[4];
  const char *             alias;
};

AVPixFmtDescriptorWrapper::Flags parseFlags(const uint8_t flagsValue)
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
  return flags;
}

AVPixFmtDescriptorWrapper::Flags parseFlags(const uint64_t flagsValue)
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

} // namespace

AVPixFmtDescriptorWrapper::AVPixFmtDescriptorWrapper(AVPixFmtDescriptor *   descriptor,
                                                     const LibraryVersions &libraryVersions)
{
  if (libraryVersions.avutil.major == 54)
  {
    auto p              = reinterpret_cast<AVPixFmtDescriptor_54 *>(descriptor);
    this->name          = std::string(p->name);
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

    aliases = std::string(p->alias);
  }
  else if (libraryVersions.avutil.major == 55)
  {
    auto p              = reinterpret_cast<AVPixFmtDescriptor_55 *>(descriptor);
    this->name          = std::string(p->name);
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

    aliases = std::string(p->alias);
  }
  else if (libraryVersions.avutil.major == 56)
  {
    auto p              = reinterpret_cast<AVPixFmtDescriptor_56 *>(descriptor);
    this->name          = std::string(p->name);
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

    aliases = std::string(p->alias);
  }
  else if (libraryVersions.avutil.major == 57 || //
           libraryVersions.avutil.major == 58)
  {
    auto p              = reinterpret_cast<AVPixFmtDescriptor_57_58 *>(descriptor);
    this->name          = std::string(p->name);
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

    aliases = std::string(p->alias);
  }
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

} // namespace LibFFmpeg