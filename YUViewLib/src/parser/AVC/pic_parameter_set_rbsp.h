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

#include "commonMaps.h"
#include "parser/common/SubByteReaderLogging.h"
#include "rbsp_trailing_bits.h"
#include "NalUnitAVC.h"

namespace parser::avc
{

class pic_parameter_set_rbsp : public NalRBSP
{
public:
  pic_parameter_set_rbsp() = default;

  void parse(reader::SubByteReaderLogging &reader, SPSMap &spsMap);

  unsigned int     pic_parameter_set_id{};
  unsigned int     seq_parameter_set_id{};
  bool             entropy_coding_mode_flag{};
  bool             bottom_field_pic_order_in_frame_present_flag{};
  unsigned int     num_slice_groups_minus1{};
  unsigned int     slice_group_map_type{};
  unsigned int     run_length_minus1[8]{};
  unsigned int     top_left[8]{};
  unsigned int     bottom_right[8]{};
  bool             slice_group_change_direction_flag{};
  unsigned int     slice_group_change_rate_minus1{};
  unsigned int     pic_size_in_map_units_minus1{};
  vector<unsigned> slice_group_id{};
  unsigned int     num_ref_idx_l0_default_active_minus1{};
  unsigned int     num_ref_idx_l1_default_active_minus1{};
  bool             weighted_pred_flag{};
  unsigned int     weighted_bipred_idc{};
  int              pic_init_qp_minus26{};
  int              pic_init_qs_minus26{};
  int              chroma_qp_index_offset{};
  bool             deblocking_filter_control_present_flag{};
  bool             constrained_intra_pred_flag{};
  bool             redundant_pic_cnt_present_flag{};

  bool transform_8x8_mode_flag{false};
  bool pic_scaling_matrix_present_flag{false};
  int  second_chroma_qp_index_offset{};
  bool pic_scaling_list_present_flag[12]{};

  array2d<int, 6, 16> ScalingList4x4;
  array<bool, 6>      UseDefaultScalingMatrix4x4Flag{};
  array2d<int, 6, 64> ScalingList8x8{};
  array<bool, 2>      UseDefaultScalingMatrix8x8Flag{};

  rbsp_trailing_bits rbspTrailingBits;

  // The following values are not read from the bitstream but are calculated from the read values.
  int SliceGroupChangeRate{};
};

} // namespace parser::avc
