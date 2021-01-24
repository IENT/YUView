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

namespace parser::hevc
{

class vui_parameters
{
public:
  vui_parameters() {}

  void parse(reader::SubByteReaderLogging &reader, unsigned sps_max_sub_layers_minus1);

  bool     aspect_ratio_info_present_flag{};
  unsigned aspect_ratio_idc{};
  unsigned sar_width{};
  unsigned sar_height;
  bool     overscan_info_present_flag{};
  bool     overscan_appropriate_flag{};
  bool     video_signal_type_present_flag{};
  unsigned video_format{5};
  bool     video_full_range_flag{};
  bool     colour_description_present_flag{};
  unsigned colour_primaries{};
  unsigned transfer_characteristics{};
  unsigned matrix_coeffs{};
  bool     chroma_loc_info_present_flag{};
  unsigned chroma_sample_loc_type_top_field{};
  unsigned chroma_sample_loc_type_bottom_field{};
  bool     neutral_chroma_indication_flag{};
  bool     field_seq_flag{};
  bool     frame_field_info_present_flag{};
  bool     default_display_window_flag{};
  unsigned def_disp_win_left_offset{};
  unsigned def_disp_win_right_offset{};
  unsigned def_disp_win_top_offset{};
  unsigned def_disp_win_bottom_offset{};

  bool           vui_timing_info_present_flag{};
  unsigned       vui_num_units_in_tick{};
  unsigned       vui_time_scale{};
  bool           vui_poc_proportional_to_timing_flag{};
  unsigned       vui_num_ticks_poc_diff_one_minus1{};
  bool           vui_hrd_parameters_present_flag{};
  hrd_parameters hrdParameters;

  bool     bitstream_restriction_flag{};
  bool     tiles_fixed_structure_flag{};
  bool     motion_vectors_over_pic_boundaries_flag{};
  bool     restricted_ref_pic_lists_flag{};
  unsigned min_spatial_segmentation_idc{};
  unsigned max_bytes_per_pic_denom{};
  unsigned max_bits_per_min_cu_denom{};
  unsigned log2_max_mv_length_horizontal{};
  unsigned log2_max_mv_length_vertical{};

  // Calculated values
  double frameRate{};
};

} // namespace parser::hevc