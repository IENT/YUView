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

#include "parser/common/SubByteReaderLogging.h"

namespace parser::hevc
{

class profile_tier_level
{
public:
  profile_tier_level() {}

  void parse(reader::SubByteReaderLogging &reader,
             bool                          profilePresentFlag,
             unsigned                      maxNumSubLayersMinus1);

  unsigned general_profile_space{};
  bool     general_tier_flag{};
  unsigned general_profile_idc{};
  bool     general_profile_compatibility_flag[32]{}; // TODO: Is this correct initialization?
  bool     general_progressive_source_flag{};
  bool     general_interlaced_source_flag{};
  bool     general_non_packed_constraint_flag{};
  bool     general_frame_only_constraint_flag{};

  bool general_max_12bit_constraint_flag{};
  bool general_max_10bit_constraint_flag{};
  bool general_max_8bit_constraint_flag{};
  bool general_max_422chroma_constraint_flag{};
  bool general_max_420chroma_constraint_flag{};
  bool general_max_monochrome_constraint_flag{};
  bool general_intra_constraint_flag{};
  bool general_one_picture_only_constraint_flag{};
  bool general_lower_bit_rate_constraint_flag{};
  bool general_inbld_flag{};

  unsigned int general_level_idc{};

  // A maximum of 8 sub-layer are allowed
  bool     sub_layer_profile_present_flag[8]{};
  bool     sub_layer_level_present_flag[8]{};
  unsigned sub_layer_profile_space[8]{};
  bool     sub_layer_tier_flag[8]{};
  unsigned sub_layer_profile_idc[8]{};
  bool     sub_layer_profile_compatibility_flag[8][32]{{}};
  bool     sub_layer_progressive_source_flag[8]{};
  bool     sub_layer_interlaced_source_flag[8]{};
  bool     sub_layer_non_packed_constraint_flag[8]{};
  bool     sub_layer_frame_only_constraint_flag[8]{};
  bool     sub_layer_max_12bit_constraint_flag[8]{};
  bool     sub_layer_max_10bit_constraint_flag[8]{};
  bool     sub_layer_max_8bit_constraint_flag[8]{};
  bool     sub_layer_max_422chroma_constraint_flag[8]{};
  bool     sub_layer_max_420chroma_constraint_flag[8]{};
  bool     sub_layer_max_monochrome_constraint_flag[8]{};
  bool     sub_layer_intra_constraint_flag[8]{};
  bool     sub_layer_one_picture_only_constraint_flag[8]{};
  bool     sub_layer_lower_bit_rate_constraint_flag[8]{};
  bool     sub_layer_inbld_flag[8]{};
  bool     sub_layer_reserved_zero_bit[8]{};
  unsigned sub_layer_level_idc[8]{};
};

} // namespace parser::hevc