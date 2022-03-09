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

#include "FFMpegLibrariesTypes.h"

namespace FFmpeg
{

class AVMotionVectorWrapper
{
public:
  AVMotionVectorWrapper() = delete;
  AVMotionVectorWrapper(LibraryVersion &libVer, uint8_t *data, unsigned idx);

  static size_t getNumberOfMotionVectors(LibraryVersion &libVer, unsigned dataSize);

  // For performance reasons, these are public here. Since update is called at construction, these
  // should be valid.
  int32_t  source{};
  uint8_t  w{};
  uint8_t  h{};
  int16_t  src_x{};
  int16_t  src_y{};
  int16_t  dst_x{};
  int16_t  dst_y{};
  uint64_t flags{};
  // The following may be invalid (-1) in older ffmpeg versions)
  int32_t  motion_x{};
  int32_t  motion_y{};
  uint16_t motion_scale{};
};

} // namespace FFmpeg
