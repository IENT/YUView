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

#include "NalUnitVVC.h"
#include "dpb_parameters.h"
#include "general_timing_hrd_parameters.h"
#include "ols_timing_hrd_parameters.h"
#include "parser/common/SubByteReaderLogging.h"
#include "profile_tier_level.h"
#include "rbsp_trailing_bits.h"
#include "ref_pic_list_struct.h"
#include "vui_payload.h"

namespace parser::vvc
{

class seq_parameter_set_rbsp : public NalRBSP,
                               public std::enable_shared_from_this<seq_parameter_set_rbsp>
{
public:
  seq_parameter_set_rbsp()  = default;
  ~seq_parameter_set_rbsp() = default;
  void parse(reader::SubByteReaderLogging &reader);

  unsigned                      sps_seq_parameter_set_id{};
  unsigned                      sps_video_parameter_set_id{};
  unsigned                      sps_max_sublayers_minus1{};
  unsigned                      sps_chroma_format_idc{};
  unsigned                      sps_log2_ctu_size_minus5{};
  bool                          sps_ptl_dpb_hrd_params_present_flag{};
  profile_tier_level            profile_tier_level_instance;
  bool                          sps_gdr_enabled_flag{};
  bool                          sps_ref_pic_resampling_enabled_flag{};
  bool                          sps_res_change_in_clvs_allowed_flag{};
  unsigned                      sps_pic_width_max_in_luma_samples{};
  unsigned                      sps_pic_height_max_in_luma_samples{};
  bool                          sps_conformance_window_flag{};
  unsigned                      sps_conf_win_left_offset{};
  unsigned                      sps_conf_win_right_offset{};
  unsigned                      sps_conf_win_top_offset{};
  unsigned                      sps_conf_win_bottom_offset{};
  bool                          sps_subpic_info_present_flag{};
  unsigned                      sps_num_subpics_minus1{};
  bool                          sps_independent_subpics_flag{true};
  bool                          sps_subpic_same_size_flag{};
  vector<unsigned>              sps_subpic_ctu_top_left_x{};
  vector<unsigned>              sps_subpic_ctu_top_left_y{};
  vector<unsigned>              sps_subpic_width_minus1{};
  vector<unsigned>              sps_subpic_height_minus1{};
  vector<bool>                  sps_subpic_treated_as_pic_flag{};
  vector<bool>                  sps_loop_filter_across_subpic_enabled_flag{};
  unsigned                      sps_subpic_id_len_minus1{};
  bool                          sps_subpic_id_mapping_explicitly_signalled_flag{};
  bool                          sps_subpic_id_mapping_present_flag{};
  vector<unsigned>              sps_subpic_id{};
  unsigned                      sps_bitdepth_minus8{};
  bool                          sps_entropy_coding_sync_enabled_flag{};
  bool                          sps_entry_point_offsets_present_flag{};
  unsigned                      sps_log2_max_pic_order_cnt_lsb_minus4{};
  bool                          sps_poc_msb_cycle_flag{};
  unsigned                      sps_poc_msb_cycle_len_minus1{};
  unsigned                      sps_num_extra_ph_bytes{};
  vector<bool>                  sps_extra_ph_bit_present_flag{};
  unsigned                      sps_num_extra_sh_bytes{};
  vector<bool>                  sps_extra_sh_bit_present_flag{};
  bool                          sps_sublayer_dpb_params_flag{};
  dpb_parameters                dpb_parameters_instance;
  unsigned                      sps_log2_min_luma_coding_block_size_minus2{};
  bool                          sps_partition_constraints_override_enabled_flag{};
  unsigned                      sps_log2_diff_min_qt_min_cb_intra_slice_luma{};
  unsigned                      sps_max_mtt_hierarchy_depth_intra_slice_luma{};
  unsigned                      sps_log2_diff_max_bt_min_qt_intra_slice_luma{};
  unsigned                      sps_log2_diff_max_tt_min_qt_intra_slice_luma{};
  bool                          sps_qtbtt_dual_tree_intra_flag{};
  unsigned                      sps_log2_diff_min_qt_min_cb_intra_slice_chroma{};
  unsigned                      sps_max_mtt_hierarchy_depth_intra_slice_chroma{};
  unsigned                      sps_log2_diff_max_bt_min_qt_intra_slice_chroma{};
  unsigned                      sps_log2_diff_max_tt_min_qt_intra_slice_chroma{};
  unsigned                      sps_log2_diff_min_qt_min_cb_inter_slice{};
  unsigned                      sps_max_mtt_hierarchy_depth_inter_slice{};
  unsigned                      sps_log2_diff_max_bt_min_qt_inter_slice{};
  unsigned                      sps_log2_diff_max_tt_min_qt_inter_slice{};
  bool                          sps_max_luma_transform_size_64_flag{};
  bool                          sps_transform_skip_enabled_flag{};
  unsigned                      sps_log2_transform_skip_max_size_minus2{};
  bool                          sps_bdpcm_enabled_flag{};
  bool                          sps_mts_enabled_flag{};
  bool                          sps_explicit_mts_intra_enabled_flag{};
  bool                          sps_explicit_mts_inter_enabled_flag{};
  bool                          sps_lfnst_enabled_flag{};
  bool                          sps_joint_cbcr_enabled_flag{};
  bool                          sps_same_qp_table_for_chroma_flag{true};
  vector<int>                   sps_qp_table_start_minus26{};
  vector<unsigned>              sps_num_points_in_qp_table_minus1{};
  vector2d<unsigned>            sps_delta_qp_in_val_minus1{};
  vector2d<unsigned>            sps_delta_qp_diff_val{};
  bool                          sps_sao_enabled_flag{};
  bool                          sps_alf_enabled_flag{};
  bool                          sps_ccalf_enabled_flag{};
  bool                          sps_lmcs_enabled_flag{};
  bool                          sps_weighted_pred_flag{};
  bool                          sps_weighted_bipred_flag{};
  bool                          sps_long_term_ref_pics_flag{};
  bool                          sps_inter_layer_prediction_enabled_flag{};
  bool                          sps_idr_rpl_present_flag{};
  bool                          sps_rpl1_same_as_rpl0_flag{};
  unsigned                      sps_num_ref_pic_lists[2];
  vector<ref_pic_list_struct>   ref_pic_list_structs[2];
  bool                          sps_ref_wraparound_enabled_flag{};
  bool                          sps_temporal_mvp_enabled_flag{};
  bool                          sps_sbtmvp_enabled_flag{};
  bool                          sps_amvr_enabled_flag{};
  bool                          sps_bdof_enabled_flag{};
  bool                          sps_bdof_control_present_in_ph_flag{};
  bool                          sps_smvd_enabled_flag{};
  bool                          sps_dmvr_enabled_flag{};
  bool                          sps_dmvr_control_present_in_ph_flag{};
  bool                          sps_mmvd_enabled_flag{};
  bool                          sps_mmvd_fullpel_only_enabled_flag{};
  unsigned                      sps_six_minus_max_num_merge_cand{};
  bool                          sps_sbt_enabled_flag{};
  bool                          sps_affine_enabled_flag{};
  unsigned                      sps_five_minus_max_num_subblock_merge_cand{};
  bool                          sps_6param_affine_enabled_flag{};
  bool                          sps_affine_amvr_enabled_flag{};
  bool                          sps_affine_prof_enabled_flag{};
  bool                          sps_prof_control_present_in_ph_flag{};
  bool                          sps_bcw_enabled_flag{};
  bool                          sps_ciip_enabled_flag{};
  bool                          sps_gpm_enabled_flag{};
  unsigned                      sps_max_num_merge_cand_minus_max_num_gpm_cand{};
  unsigned                      sps_log2_parallel_merge_level_minus2{};
  bool                          sps_isp_enabled_flag{};
  bool                          sps_mrl_enabled_flag{};
  bool                          sps_mip_enabled_flag{};
  bool                          sps_cclm_enabled_flag{};
  bool                          sps_chroma_horizontal_collocated_flag{true};
  bool                          sps_chroma_vertical_collocated_flag{true};
  bool                          sps_palette_enabled_flag{};
  bool                          sps_act_enabled_flag{};
  unsigned                      sps_min_qp_prime_ts{};
  bool                          sps_ibc_enabled_flag{};
  unsigned                      sps_six_minus_max_num_ibc_merge_cand{};
  bool                          sps_ladf_enabled_flag{};
  unsigned                      sps_num_ladf_intervals_minus2{};
  int                           sps_ladf_lowest_interval_qp_offset{};
  vector<int>                   sps_ladf_qp_offset{};
  vector<unsigned>              sps_ladf_delta_threshold_minus1{};
  bool                          sps_explicit_scaling_list_enabled_flag{};
  bool                          sps_scaling_matrix_for_lfnst_disabled_flag{};
  bool                          sps_scaling_matrix_for_alternative_colour_space_disabled_flag{};
  bool                          sps_scaling_matrix_designated_colour_space_flag{};
  bool                          sps_dep_quant_enabled_flag{};
  bool                          sps_sign_data_hiding_enabled_flag{};
  bool                          sps_virtual_boundaries_enabled_flag{};
  bool                          sps_virtual_boundaries_present_flag{};
  unsigned                      sps_num_ver_virtual_boundaries{};
  vector<unsigned>              sps_virtual_boundary_pos_x_minus1{};
  unsigned                      sps_num_hor_virtual_boundaries{};
  vector<unsigned>              sps_virtual_boundary_pos_y_minus1{};
  bool                          sps_timing_hrd_params_present_flag{};
  general_timing_hrd_parameters general_timing_hrd_parameters_instance;
  bool                          sps_sublayer_cpb_params_present_flag{};
  ols_timing_hrd_parameters     ols_timing_hrd_parameters_instance;
  bool                          sps_field_seq_flag{};
  bool                          sps_vui_parameters_present_flag{};
  unsigned                      sps_vui_payload_size_minus1{};
  bool                          sps_vui_alignment_zero_bit{};
  vui_payload                   vui_payload_instance;
  bool                          sps_extension_flag{};
  bool                          sps_extension_data_flag{};
  rbsp_trailing_bits            rbsp_trailing_bits_instance;

  unsigned BitDepth{};
  unsigned QpBdOffset{};
  unsigned MaxPicOrderCntLsb{};
  unsigned NumExtraPhBits{0};
  unsigned SubWidthC{};
  unsigned SubHeightC{};
  unsigned CtbLog2SizeY{};
  unsigned CtbSizeY{};
  unsigned NumExtraShBits{};
  unsigned MinCbLog2SizeY{};
  unsigned MinCbSizeY{};
  unsigned IbcBufWidthY{};
  unsigned IbcBufWidthC{};
  unsigned VSize{};
  unsigned MinQtLog2SizeIntraY{};
  unsigned MaxNumMergeCand{};
  unsigned MinQtLog2SizeIntraC{};
  unsigned MinQtLog2SizeInterY{};

  // Get the actual size of the image that will be returned. Internally the image might be bigger.
  unsigned get_max_width_cropping() const
  {
    return (this->sps_pic_width_max_in_luma_samples -
            (this->SubWidthC * this->sps_conf_win_right_offset) -
            this->SubWidthC * this->sps_conf_win_left_offset);
  }
  unsigned get_max_height_cropping() const
  {
    return (this->sps_pic_height_max_in_luma_samples -
            (this->SubHeightC * this->sps_conf_win_bottom_offset) -
            this->SubHeightC * this->sps_conf_win_top_offset);
  }

private:
  void setDefaultSubpicValues(unsigned numSubPic);
};

} // namespace parser::vvc
