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

#include "AVMotionVectorWrapper.h"
#include <stdexcept>

namespace FFmpeg
{

namespace
{

typedef struct AVMotionVector_54
{
  int32_t  source;
  uint8_t  w, h;
  int16_t  src_x, src_y;
  int16_t  dst_x, dst_y;
  uint64_t flags;
} AVMotionVector_54;

typedef struct AVMotionVector_55_56_57
{
  int32_t  source;
  uint8_t  w, h;
  int16_t  src_x, src_y;
  int16_t  dst_x, dst_y;
  uint64_t flags;
  int32_t  motion_x, motion_y;
  uint16_t motion_scale;
} AVMotionVector_55_56_57;

} // namespace

AVMotionVectorWrapper::AVMotionVectorWrapper(LibraryVersion &libVer, uint8_t *data, unsigned idx)
{
  if (libVer.avutil.major == 54)
  {
    auto p             = reinterpret_cast<AVMotionVector_54 *>(data) + idx;
    this->source       = p->source;
    this->w            = p->w;
    this->h            = p->h;
    this->src_x        = p->src_x;
    this->src_y        = p->src_y;
    this->dst_x        = p->dst_x;
    this->dst_y        = p->dst_y;
    this->flags        = p->flags;
    this->motion_x     = -1;
    this->motion_y     = -1;
    this->motion_scale = -1;
  }
  else if (libVer.avutil.major == 55 || //
           libVer.avutil.major == 56 || //
           libVer.avutil.major == 57)
  {
    auto p             = reinterpret_cast<AVMotionVector_55_56_57 *>(data) + idx;
    this->source       = p->source;
    this->w            = p->w;
    this->h            = p->h;
    this->src_x        = p->src_x;
    this->src_y        = p->src_y;
    this->dst_x        = p->dst_x;
    this->dst_y        = p->dst_y;
    this->flags        = p->flags;
    this->motion_x     = p->motion_x;
    this->motion_y     = p->motion_y;
    this->motion_scale = p->motion_scale;
  }
  else
    throw std::runtime_error("Invalid library version");
}

size_t AVMotionVectorWrapper::getNumberOfMotionVectors(LibraryVersion &libVer, unsigned dataSize)
{
  if (libVer.avutil.major == 54)
    return dataSize / sizeof(AVMotionVector_54);
  else if (libVer.avutil.major == 55 || libVer.avutil.major == 56 || libVer.avutil.major == 57)
    return dataSize / sizeof(AVMotionVector_55_56_57);
  else
    return 0;
}

} // namespace FFmpeg
