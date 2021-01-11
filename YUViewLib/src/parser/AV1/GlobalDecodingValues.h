/*  This file is part of YUView - The YUV player with advanced analytics toolset
 *   <https://github.com/IENT/YUView>
 *   Copyright (C) 2015  Institut f�r Nachrichtentechnik, RWTH Aachen University, GERMANY
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

#include "OpenBitstreamUnit.h"
#include "typedef.h"

namespace parser::av1
{

struct GlobalDecodingValues
{
  FrameType RefFrameType[8]{};
  bool      RefValid[8]{};
  unsigned  RefOrderHint[8]{};
  unsigned  OrderHints[8]{};
  int       RefFrameId[8]{};

  bool SeenFrameHeader{};
  int  PrevFrameID{};
  int  current_frame_id{};

  // TODO: These need to be set at the end of the "decoding" process
  int RefUpscaledWidth[8]{};
  int RefFrameHeight[8]{};
  int RefRenderWidth[8]{};
  int RefRenderHeight[8]{};
};

} // namespace parser::av1