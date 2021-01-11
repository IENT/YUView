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

namespace parser::av1
{

enum class FrameType
{
  KEY_FRAME,
  INTER_FRAME,
  INTRA_ONLY_FRAME,
  SWITCH_FRAME
};

constexpr unsigned SELECT_SCREEN_CONTENT_TOOLS = 2;
constexpr unsigned SELECT_INTEGER_MV           = 2;
constexpr unsigned NUM_REF_FRAMES              = 8;
constexpr unsigned REFS_PER_FRAME              = 7;
constexpr unsigned PRIMARY_REF_NONE            = 7;
constexpr unsigned MAX_SEGMENTS                = 8;
constexpr unsigned SEG_LVL_MAX                 = 8;
constexpr unsigned SEG_LVL_REF_FRAME           = 5;
constexpr unsigned MAX_LOOP_FILTER             = 63;
constexpr unsigned SEG_LVL_ALT_Q               = 0;
constexpr unsigned TOTAL_REFS_PER_FRAME        = 8;

constexpr unsigned SUPERRES_DENOM_BITS = 3;
constexpr unsigned SUPERRES_DENOM_MIN  = 9;
constexpr unsigned SUPERRES_NUM        = 8;

// The indices into the RefFrame list
constexpr unsigned INTRA_FRAME   = 0;
constexpr unsigned LAST_FRAME    = 1;
constexpr unsigned LAST2_FRAME   = 2;
constexpr unsigned LAST3_FRAME   = 3;
constexpr unsigned GOLDEN_FRAME  = 4;
constexpr unsigned BWDREF_FRAME  = 5;
constexpr unsigned ALTREF2_FRAME = 6;
constexpr unsigned ALTREF_FRAME  = 7;

constexpr unsigned MAX_TILE_WIDTH = 4096;
constexpr unsigned MAX_TILE_AREA  = 4096 * 2304;
constexpr unsigned MAX_TILE_COLS  = 64;
constexpr unsigned MAX_TILE_ROWS  = 64;

} // namespace parser::av1