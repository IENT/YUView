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

#include "hrd_parameters.h"
#include "parser/common/SubByteReaderLogging.h"
#include "profile_tier_level.h"
#include "NalUnitHEVC.h"

namespace parser::hevc
{

// The video parameter set. 7.3.2.1
class video_parameter_set_rbsp : public NalRBSP
{
public:
  video_parameter_set_rbsp() {}

  void parse(reader::SubByteReaderLogging &reader);

  unsigned vps_video_parameter_set_id{};
  bool     vps_base_layer_internal_flag{};
  bool     vps_base_layer_available_flag{};
  unsigned vps_max_layers_minus1{};
  unsigned vps_max_sub_layers_minus1{};
  bool     vps_temporal_id_nesting_flag{};
  unsigned vps_reserved_0xffff_16bits{};

  profile_tier_level profileTierLevel;

  bool         vps_sub_layer_ordering_info_present_flag{};
  unsigned     vps_max_dec_pic_buffering_minus1[7]{};
  unsigned     vps_max_num_reorder_pics[7]{};
  unsigned     vps_max_latency_increase_plus1[7]{};
  unsigned     vps_max_layer_id{};
  unsigned     vps_num_layer_sets_minus1{};
  vector<bool> layer_id_included_flag[7]{};

  bool             vps_timing_info_present_flag{};
  unsigned         vps_num_units_in_tick{};
  unsigned         vps_time_scale{};
  bool             vps_poc_proportional_to_timing_flag{};
  unsigned         vps_num_ticks_poc_diff_one_minus1{};
  unsigned         vps_num_hrd_parameters{};
  vector<unsigned> hrd_layer_set_idx;
  vector<bool>     cprms_present_flag;

  vector<hrd_parameters> vps_hrd_parameters;
  bool                   vps_extension_flag{};

  // Calculated values
  double frameRate{};
};

} // namespace parser::hevc