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

#include "general_constraints_info.h"
#include "parser/common/SubByteReaderLogging.h"


namespace parser::vvc
{

class profile_tier_level
{
public:
  profile_tier_level()  = default;
  ~profile_tier_level() = default;
  void parse(reader::SubByteReaderLogging &reader,
             bool                     profileTierPresentFlag,
             unsigned                 MaxNumSubLayersMinus1);

  unsigned                 general_profile_idc{};
  bool                     general_tier_flag{};
  unsigned                 general_level_idc{};
  bool                     ptl_frame_only_constraint_flag{};
  bool                     ptl_multilayer_enabled_flag{};
  general_constraints_info general_constraints_info_instance;
  std::vector<bool>        ptl_sublayer_level_present_flag{};
  bool                     ptl_reserved_zero_bit{};
  std::vector<unsigned>    sublayer_level_idc{};
  unsigned                 ptl_num_sub_profiles{};
  std::vector<unsigned>    general_sub_profile_idc{};
};

} // namespace parser::vvc
