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

#include "NalUnitHEVC.h"
#include "parser/common/SubByteReaderLogging.h"
#include "pps_range_extension.h"
#include "scaling_list_data.h"
#include "rbsp_trailing_bits.h"

namespace parser::hevc
{

class pic_parameter_set_rbsp : public NalRBSP
{
public:
  pic_parameter_set_rbsp() {}

  void parse(reader::SubByteReaderLogging &reader);

  unsigned          pps_pic_parameter_set_id{};
  unsigned          pps_seq_parameter_set_id{};
  bool              dependent_slice_segments_enabled_flag{};
  bool              output_flag_present_flag{};
  unsigned          num_extra_slice_header_bits{};
  bool              sign_data_hiding_enabled_flag{};
  bool              cabac_init_present_flag{};
  unsigned          num_ref_idx_l0_default_active_minus1{};
  unsigned          num_ref_idx_l1_default_active_minus1{};
  int               init_qp_minus26{};
  bool              constrained_intra_pred_flag{};
  bool              transform_skip_enabled_flag{};
  bool              cu_qp_delta_enabled_flag{};
  unsigned          diff_cu_qp_delta_depth{};
  int               pps_cb_qp_offset{};
  int               pps_cr_qp_offset{};
  bool              pps_slice_chroma_qp_offsets_present_flag{};
  bool              weighted_pred_flag{};
  bool              weighted_bipred_flag{};
  bool              transquant_bypass_enabled_flag{};
  bool              tiles_enabled_flag{};
  bool              entropy_coding_sync_enabled_flag{};
  unsigned          num_tile_columns_minus1{};
  unsigned          num_tile_rows_minus1{};
  bool              uniform_spacing_flag{};
  vector<unsigned>  column_width_minus1;
  vector<unsigned>  row_height_minus1;
  bool              loop_filter_across_tiles_enabled_flag{};
  bool              pps_loop_filter_across_slices_enabled_flag{};
  bool              deblocking_filter_control_present_flag{};
  bool              deblocking_filter_override_enabled_flag{};
  bool              pps_deblocking_filter_disabled_flag{};
  int               pps_beta_offset_div2{};
  int               pps_tc_offset_div2{};
  bool              pps_scaling_list_data_present_flag{};
  scaling_list_data scalingListData;
  bool              lists_modification_present_flag{};
  unsigned          log2_parallel_merge_level_minus2{};
  bool              slice_segment_header_extension_present_flag{};
  bool              pps_extension_present_flag{};
  bool              pps_range_extension_flag{};
  bool              pps_multilayer_extension_flag{};
  bool              pps_3d_extension_flag{};
  unsigned          pps_extension_5bits{};

  pps_range_extension ppsRangeExtension;

  rbsp_trailing_bits rbspTrailingBits;
};

} // namespace parser::hevc