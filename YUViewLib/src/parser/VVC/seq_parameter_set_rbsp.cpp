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

#include "seq_parameter_set_rbsp.h"

namespace parser::vvc
{

void seq_parameter_set_rbsp::parse(ReaderHelperNew &reader)
{
  this->sps_seq_parameter_set_id   = reader.readBits("sps_seq_parameter_set_id", 4);
  this->sps_video_parameter_set_id = reader.readBits("sps_video_parameter_set_id", 4);
  this->sps_max_sublayers_minus1   = reader.readBits("sps_max_sublayers_minus1", 3);
  this->sps_chroma_format_idc      = reader.readBits("sps_chroma_format_idc", 2);
  this->sps_log2_ctu_size_minus5   = reader.readBits("sps_log2_ctu_size_minus5", 2);

  // The variables CtbLog2SizeY and CtbSizeY are derived as follows:
  this->CtbLog2SizeY = sps_log2_ctu_size_minus5 + 5; // (35)
  this->CtbSizeY     = 1 << CtbLog2SizeY;            // (36)

  this->sps_ptl_dpb_hrd_params_present_flag =
      reader.readFlag("sps_ptl_dpb_hrd_params_present_flag");
  if (sps_ptl_dpb_hrd_params_present_flag)
  {
    // TODO
    // this->profile_tier_level_instance.parse(reader, 1, sps_max_sublayers_minus1);
  }
  this->sps_gdr_enabled_flag = reader.readFlag("sps_gdr_enabled_flag");
  this->sps_ref_pic_resampling_enabled_flag =
      reader.readFlag("sps_ref_pic_resampling_enabled_flag");
  if (sps_ref_pic_resampling_enabled_flag)
  {
    this->sps_res_change_in_clvs_allowed_flag =
        reader.readFlag("sps_res_change_in_clvs_allowed_flag");
  }
  this->sps_pic_width_max_in_luma_samples  = reader.readUEV("sps_pic_width_max_in_luma_samples");
  this->sps_pic_height_max_in_luma_samples = reader.readUEV("sps_pic_height_max_in_luma_samples");
  this->sps_conformance_window_flag        = reader.readFlag("sps_conformance_window_flag");
  if (sps_conformance_window_flag)
  {
    this->sps_conf_win_left_offset   = reader.readUEV("sps_conf_win_left_offset");
    this->sps_conf_win_right_offset  = reader.readUEV("sps_conf_win_right_offset");
    this->sps_conf_win_top_offset    = reader.readUEV("sps_conf_win_top_offset");
    this->sps_conf_win_bottom_offset = reader.readUEV("sps_conf_win_bottom_offset");
  }
  this->sps_subpic_info_present_flag = reader.readFlag("sps_subpic_info_present_flag");

  if (sps_subpic_info_present_flag)
  {
    this->sps_num_subpics_minus1 = reader.readUEV("sps_num_subpics_minus1");
    if (sps_num_subpics_minus1 > 0)
    {
      this->sps_independent_subpics_flag = reader.readFlag("sps_independent_subpics_flag");
      this->sps_subpic_same_size_flag    = reader.readFlag("sps_subpic_same_size_flag");
    }
    for (unsigned i = 0; sps_num_subpics_minus1 > 0 && i <= sps_num_subpics_minus1; i += i++)
    {
      if (!sps_subpic_same_size_flag || i == 0)
      {
        auto tmpWidthVal  = (sps_pic_width_max_in_luma_samples + CtbSizeY - 1) / CtbSizeY;
        auto tmpHeightVal = (sps_pic_height_max_in_luma_samples + CtbSizeY - 1) / CtbSizeY;
        if (i > 0 && sps_pic_width_max_in_luma_samples > CtbSizeY)
        {
          auto numBits                    = std::ceil(std::log2(tmpWidthVal));
          this->sps_subpic_ctu_top_left_x = reader.readBits("sps_subpic_ctu_top_left_x", numBits);
        }
        if (i > 0 && sps_pic_height_max_in_luma_samples > CtbSizeY)
        {
          auto numBits                    = std::ceil(std::log2(tmpHeightVal));
          this->sps_subpic_ctu_top_left_y = reader.readBits("sps_subpic_ctu_top_left_y", numBits);
        }
        if (i < sps_num_subpics_minus1 && sps_pic_width_max_in_luma_samples > CtbSizeY)
        {
          auto numBits                  = std::ceil(std::log2(tmpWidthVal));
          this->sps_subpic_width_minus1 = reader.readBits("sps_subpic_width_minus1", numBits);
        }
        if (i < sps_num_subpics_minus1 && sps_pic_height_max_in_luma_samples > CtbSizeY)
        {
          auto numBits                   = std::ceil(std::log2(tmpHeightVal));
          this->sps_subpic_height_minus1 = reader.readBits("sps_subpic_height_minus1", numBits);
        }
      }
      if (!sps_independent_subpics_flag)
      {
        this->sps_subpic_treated_as_pic_flag[i] = reader.readFlag("sps_subpic_treated_as_pic_flag");
        this->sps_loop_filter_across_subpic_enabled_flag[i] =
            reader.readFlag("sps_loop_filter_across_subpic_enabled_flag");
      }
    }
    this->sps_subpic_id_len_minus1 = reader.readUEV("sps_subpic_id_len_minus1");
    this->sps_subpic_id_mapping_explicitly_signalled_flag =
        reader.readFlag("sps_subpic_id_mapping_explicitly_signalled_flag");
    if (sps_subpic_id_mapping_explicitly_signalled_flag)
    {
      this->sps_subpic_id_mapping_present_flag =
          reader.readFlag("sps_subpic_id_mapping_present_flag");
      if (sps_subpic_id_mapping_present_flag)
      {
        for (unsigned i = 0; i <= sps_num_subpics_minus1; i += i++)
        {
          auto numBits        = sps_subpic_id_len_minus1 + 1;
          this->sps_subpic_id = reader.readBits("sps_subpic_id", numBits);
        }
      }
    }
  }
  this->sps_bitdepth_minus8 = reader.readUEV("sps_bitdepth_minus8");
  this->sps_entropy_coding_sync_enabled_flag =
      reader.readFlag("sps_entropy_coding_sync_enabled_flag");
  this->sps_entry_point_offsets_present_flag =
      reader.readFlag("sps_entry_point_offsets_present_flag");
  this->sps_log2_max_pic_order_cnt_lsb_minus4 =
      reader.readBits("sps_log2_max_pic_order_cnt_lsb_minus4", 4);
  this->sps_poc_msb_cycle_flag = reader.readFlag("sps_poc_msb_cycle_flag");
  if (sps_poc_msb_cycle_flag)
  {
    this->sps_poc_msb_cycle_len_minus1 = reader.readUEV("sps_poc_msb_cycle_len_minus1");
  }
  this->sps_num_extra_ph_bytes = reader.readBits("sps_num_extra_ph_bytes", 2);
  for (unsigned i = 0; i < (sps_num_extra_ph_bytes * 8); i += i++)
  {
    this->sps_extra_ph_bit_present_flag[i] = reader.readFlag("sps_extra_ph_bit_present_flag");
  }
  this->sps_num_extra_sh_bytes = reader.readBits("sps_num_extra_sh_bytes", 2);
  for (unsigned i = 0; i < (sps_num_extra_sh_bytes * 8); i += i++)
  {
    this->sps_extra_sh_bit_present_flag[i] = reader.readFlag("sps_extra_sh_bit_present_flag");
  }
  if (sps_ptl_dpb_hrd_params_present_flag)
  {
    if (sps_max_sublayers_minus1 > 0)
    {
      this->sps_sublayer_dpb_params_flag = reader.readFlag("sps_sublayer_dpb_params_flag");
    }
    // TODO
    // this->dpb_parameters_instance.parse(
    //     reader, sps_max_sublayers_minus1, sps_sublayer_dpb_params_flag);
  }
  this->sps_log2_min_luma_coding_block_size_minus2 =
      reader.readUEV("sps_log2_min_luma_coding_block_size_minus2");
  this->sps_partition_constraints_override_enabled_flag =
      reader.readFlag("sps_partition_constraints_override_enabled_flag");
  this->sps_log2_diff_min_qt_min_cb_intra_slice_luma =
      reader.readUEV("sps_log2_diff_min_qt_min_cb_intra_slice_luma");
  this->sps_max_mtt_hierarchy_depth_intra_slice_luma =
      reader.readUEV("sps_max_mtt_hierarchy_depth_intra_slice_luma");
  if (sps_max_mtt_hierarchy_depth_intra_slice_luma != 0)
  {
    this->sps_log2_diff_max_bt_min_qt_intra_slice_luma =
        reader.readUEV("sps_log2_diff_max_bt_min_qt_intra_slice_luma");
    this->sps_log2_diff_max_tt_min_qt_intra_slice_luma =
        reader.readUEV("sps_log2_diff_max_tt_min_qt_intra_slice_luma");
  }
  if (sps_chroma_format_idc != 0)
  {
    this->sps_qtbtt_dual_tree_intra_flag = reader.readFlag("sps_qtbtt_dual_tree_intra_flag");
  }
  if (sps_qtbtt_dual_tree_intra_flag)
  {
    this->sps_log2_diff_min_qt_min_cb_intra_slice_chroma =
        reader.readUEV("sps_log2_diff_min_qt_min_cb_intra_slice_chroma");
    this->sps_max_mtt_hierarchy_depth_intra_slice_chroma =
        reader.readUEV("sps_max_mtt_hierarchy_depth_intra_slice_chroma");
    if (sps_max_mtt_hierarchy_depth_intra_slice_chroma != 0)
    {
      this->sps_log2_diff_max_bt_min_qt_intra_slice_chroma =
          reader.readUEV("sps_log2_diff_max_bt_min_qt_intra_slice_chroma");
      this->sps_log2_diff_max_tt_min_qt_intra_slice_chroma =
          reader.readUEV("sps_log2_diff_max_tt_min_qt_intra_slice_chroma");
    }
  }
  this->sps_log2_diff_min_qt_min_cb_inter_slice =
      reader.readUEV("sps_log2_diff_min_qt_min_cb_inter_slice");
  this->sps_max_mtt_hierarchy_depth_inter_slice =
      reader.readUEV("sps_max_mtt_hierarchy_depth_inter_slice");
  if (sps_max_mtt_hierarchy_depth_inter_slice != 0)
  {
    this->sps_log2_diff_max_bt_min_qt_inter_slice =
        reader.readUEV("sps_log2_diff_max_bt_min_qt_inter_slice");
    this->sps_log2_diff_max_tt_min_qt_inter_slice =
        reader.readUEV("sps_log2_diff_max_tt_min_qt_inter_slice");
  }
  if (CtbSizeY > 32)
  {
    this->sps_max_luma_transform_size_64_flag =
        reader.readFlag("sps_max_luma_transform_size_64_flag");
  }
  this->sps_transform_skip_enabled_flag = reader.readFlag("sps_transform_skip_enabled_flag");
  if (sps_transform_skip_enabled_flag)
  {
    this->sps_log2_transform_skip_max_size_minus2 =
        reader.readUEV("sps_log2_transform_skip_max_size_minus2");
    this->sps_bdpcm_enabled_flag = reader.readFlag("sps_bdpcm_enabled_flag");
  }
  this->sps_mts_enabled_flag = reader.readFlag("sps_mts_enabled_flag");
  if (sps_mts_enabled_flag)
  {
    this->sps_explicit_mts_intra_enabled_flag =
        reader.readFlag("sps_explicit_mts_intra_enabled_flag");
    this->sps_explicit_mts_inter_enabled_flag =
        reader.readFlag("sps_explicit_mts_inter_enabled_flag");
  }
  this->sps_lfnst_enabled_flag = reader.readFlag("sps_lfnst_enabled_flag");
  if (sps_chroma_format_idc != 0)
  {
    this->sps_joint_cbcr_enabled_flag       = reader.readFlag("sps_joint_cbcr_enabled_flag");
    this->sps_same_qp_table_for_chroma_flag = reader.readFlag("sps_same_qp_table_for_chroma_flag");
    unsigned numQpTables =
        this->sps_same_qp_table_for_chroma_flag ? 1 : (this->sps_joint_cbcr_enabled_flag ? 3 : 2);
    for (unsigned i = 0; i < numQpTables; i += i++)
    {
      this->sps_qp_table_start_minus26[i] = reader.readSEV("sps_qp_table_start_minus26");
      this->sps_num_points_in_qp_table_minus1[i] =
          reader.readUEV("sps_num_points_in_qp_table_minus1");
      for (unsigned j = 0; j <= sps_num_points_in_qp_table_minus1[i]; i += j++)
      {
        this->sps_delta_qp_in_val_minus1[i][j] = reader.readUEV("sps_delta_qp_in_val_minus1");
        this->sps_delta_qp_diff_val[i][j]      = reader.readUEV("sps_delta_qp_diff_val");
      }
    }
  }
  this->sps_sao_enabled_flag = reader.readFlag("sps_sao_enabled_flag");
  this->sps_alf_enabled_flag = reader.readFlag("sps_alf_enabled_flag");
  if (sps_alf_enabled_flag && sps_chroma_format_idc != 0)
  {
    this->sps_ccalf_enabled_flag = reader.readFlag("sps_ccalf_enabled_flag");
  }
  this->sps_lmcs_enabled_flag       = reader.readFlag("sps_lmcs_enabled_flag");
  this->sps_weighted_pred_flag      = reader.readFlag("sps_weighted_pred_flag");
  this->sps_weighted_bipred_flag    = reader.readFlag("sps_weighted_bipred_flag");
  this->sps_long_term_ref_pics_flag = reader.readFlag("sps_long_term_ref_pics_flag");
  if (sps_video_parameter_set_id > 0)
  {
    this->sps_inter_layer_prediction_enabled_flag =
        reader.readFlag("sps_inter_layer_prediction_enabled_flag");
  }
  this->sps_idr_rpl_present_flag   = reader.readFlag("sps_idr_rpl_present_flag");
  this->sps_rpl1_same_as_rpl0_flag = reader.readFlag("sps_rpl1_same_as_rpl0_flag");
  for (unsigned i = 0; i < (sps_rpl1_same_as_rpl0_flag ? 1u : 2u); i += i++)
  {
    this->sps_num_ref_pic_lists[i] = reader.readUEV("sps_num_ref_pic_lists");
    for (unsigned j = 0; j < sps_num_ref_pic_lists[i]; i += j++)
    {
      // TODO
      //this->ref_pic_list_struct_instance.parse(reader, i, j);
    }
  }
  this->sps_ref_wraparound_enabled_flag = reader.readFlag("sps_ref_wraparound_enabled_flag");
  this->sps_temporal_mvp_enabled_flag   = reader.readFlag("sps_temporal_mvp_enabled_flag");
  if (sps_temporal_mvp_enabled_flag)
  {
    this->sps_sbtmvp_enabled_flag = reader.readFlag("sps_sbtmvp_enabled_flag");
  }
  this->sps_amvr_enabled_flag = reader.readFlag("sps_amvr_enabled_flag");
  this->sps_bdof_enabled_flag = reader.readFlag("sps_bdof_enabled_flag");
  if (sps_bdof_enabled_flag)
  {
    this->sps_bdof_control_present_in_ph_flag =
        reader.readFlag("sps_bdof_control_present_in_ph_flag");
  }
  this->sps_smvd_enabled_flag = reader.readFlag("sps_smvd_enabled_flag");
  this->sps_dmvr_enabled_flag = reader.readFlag("sps_dmvr_enabled_flag");
  if (sps_dmvr_enabled_flag)
  {
    this->sps_dmvr_control_present_in_ph_flag =
        reader.readFlag("sps_dmvr_control_present_in_ph_flag");
  }
  this->sps_mmvd_enabled_flag = reader.readFlag("sps_mmvd_enabled_flag");
  if (sps_mmvd_enabled_flag)
  {
    this->sps_mmvd_fullpel_only_enabled_flag =
        reader.readFlag("sps_mmvd_fullpel_only_enabled_flag");
  }
  this->sps_six_minus_max_num_merge_cand = reader.readUEV("sps_six_minus_max_num_merge_cand");
  this->sps_sbt_enabled_flag             = reader.readFlag("sps_sbt_enabled_flag");
  this->sps_affine_enabled_flag          = reader.readFlag("sps_affine_enabled_flag");

  this->MaxNumMergeCand = 6 - sps_six_minus_max_num_merge_cand; // (58)

  if (sps_affine_enabled_flag)
  {
    this->sps_five_minus_max_num_subblock_merge_cand =
        reader.readUEV("sps_five_minus_max_num_subblock_merge_cand");
    this->sps_6param_affine_enabled_flag = reader.readFlag("sps_6param_affine_enabled_flag");
    if (sps_amvr_enabled_flag)
    {
      this->sps_affine_amvr_enabled_flag = reader.readFlag("sps_affine_amvr_enabled_flag");
    }
    this->sps_affine_prof_enabled_flag = reader.readFlag("sps_affine_prof_enabled_flag");
    if (sps_affine_prof_enabled_flag)
    {
      this->sps_prof_control_present_in_ph_flag =
          reader.readFlag("sps_prof_control_present_in_ph_flag");
    }
  }
  this->sps_bcw_enabled_flag  = reader.readFlag("sps_bcw_enabled_flag");
  this->sps_ciip_enabled_flag = reader.readFlag("sps_ciip_enabled_flag");
  if (MaxNumMergeCand >= 2)
  {
    this->sps_gpm_enabled_flag = reader.readFlag("sps_gpm_enabled_flag");
    if (sps_gpm_enabled_flag && MaxNumMergeCand >= 3)
    {
      this->sps_max_num_merge_cand_minus_max_num_gpm_cand =
          reader.readUEV("sps_max_num_merge_cand_minus_max_num_gpm_cand");
    }
  }
  this->sps_log2_parallel_merge_level_minus2 =
      reader.readUEV("sps_log2_parallel_merge_level_minus2");
  this->sps_isp_enabled_flag = reader.readFlag("sps_isp_enabled_flag");
  this->sps_mrl_enabled_flag = reader.readFlag("sps_mrl_enabled_flag");
  this->sps_mip_enabled_flag = reader.readFlag("sps_mip_enabled_flag");
  if (sps_chroma_format_idc != 0)
  {
    this->sps_cclm_enabled_flag = reader.readFlag("sps_cclm_enabled_flag");
  }
  if (sps_chroma_format_idc == 1)
  {
    this->sps_chroma_horizontal_collocated_flag =
        reader.readFlag("sps_chroma_horizontal_collocated_flag");
    this->sps_chroma_vertical_collocated_flag =
        reader.readFlag("sps_chroma_vertical_collocated_flag");
  }
  this->sps_palette_enabled_flag = reader.readFlag("sps_palette_enabled_flag");
  if (sps_chroma_format_idc == 3 && !sps_max_luma_transform_size_64_flag)
  {
    this->sps_act_enabled_flag = reader.readFlag("sps_act_enabled_flag");
  }
  if (sps_transform_skip_enabled_flag || sps_palette_enabled_flag)
  {
    this->sps_min_qp_prime_ts = reader.readUEV("sps_min_qp_prime_ts");
  }
  this->sps_ibc_enabled_flag = reader.readFlag("sps_ibc_enabled_flag");
  if (sps_ibc_enabled_flag)
  {
    this->sps_six_minus_max_num_ibc_merge_cand =
        reader.readUEV("sps_six_minus_max_num_ibc_merge_cand");
  }
  this->sps_ladf_enabled_flag = reader.readFlag("sps_ladf_enabled_flag");
  if (sps_ladf_enabled_flag)
  {
    this->sps_num_ladf_intervals_minus2      = reader.readBits("sps_num_ladf_intervals_minus2", 2);
    this->sps_ladf_lowest_interval_qp_offset = reader.readSEV("sps_ladf_lowest_interval_qp_offset");
    for (unsigned i = 0; i < sps_num_ladf_intervals_minus2 + 1; i += i++)
    {
      this->sps_ladf_qp_offset[i]              = reader.readSEV("sps_ladf_qp_offset");
      this->sps_ladf_delta_threshold_minus1[i] = reader.readUEV("sps_ladf_delta_threshold_minus1");
    }
  }
  this->sps_explicit_scaling_list_enabled_flag =
      reader.readFlag("sps_explicit_scaling_list_enabled_flag");
  if (sps_lfnst_enabled_flag && sps_explicit_scaling_list_enabled_flag)
  {
    this->sps_scaling_matrix_for_lfnst_disabled_flag =
        reader.readFlag("sps_scaling_matrix_for_lfnst_disabled_flag");
  }
  if (sps_act_enabled_flag && sps_explicit_scaling_list_enabled_flag)
  {
    this->sps_scaling_matrix_for_alternative_colour_space_disabled_flag =
        reader.readFlag("sps_scaling_matrix_for_alternative_colour_space_disabled_flag");
  }
  if (sps_scaling_matrix_for_alternative_colour_space_disabled_flag)
  {
    this->sps_scaling_matrix_designated_colour_space_flag =
        reader.readFlag("sps_scaling_matrix_designated_colour_space_flag");
  }
  this->sps_dep_quant_enabled_flag        = reader.readFlag("sps_dep_quant_enabled_flag");
  this->sps_sign_data_hiding_enabled_flag = reader.readFlag("sps_sign_data_hiding_enabled_flag");
  this->sps_virtual_boundaries_enabled_flag =
      reader.readFlag("sps_virtual_boundaries_enabled_flag");
  if (sps_virtual_boundaries_enabled_flag)
  {
    this->sps_virtual_boundaries_present_flag =
        reader.readFlag("sps_virtual_boundaries_present_flag");
    if (sps_virtual_boundaries_present_flag)
    {
      this->sps_num_ver_virtual_boundaries = reader.readUEV("sps_num_ver_virtual_boundaries");
      for (unsigned i = 0; i < sps_num_ver_virtual_boundaries; i += i++)
      {
        this->sps_virtual_boundary_pos_x_minus1[i] =
            reader.readUEV("sps_virtual_boundary_pos_x_minus1");
      }
      this->sps_num_hor_virtual_boundaries = reader.readUEV("sps_num_hor_virtual_boundaries");
      for (unsigned i = 0; i < sps_num_hor_virtual_boundaries; i += i++)
      {
        this->sps_virtual_boundary_pos_y_minus1[i] =
            reader.readUEV("sps_virtual_boundary_pos_y_minus1");
      }
    }
  }
  if (sps_ptl_dpb_hrd_params_present_flag)
  {
    this->sps_timing_hrd_params_present_flag =
        reader.readFlag("sps_timing_hrd_params_present_flag");
    if (sps_timing_hrd_params_present_flag)
    {
      // TODO
      //this->general_timing_hrd_parameters_instance.parse(reader);
      if (sps_max_sublayers_minus1 > 0)
      {
        this->sps_sublayer_cpb_params_present_flag =
            reader.readFlag("sps_sublayer_cpb_params_present_flag");
      }
      // TODO
      // this->ols_timing_hrd_parameters_instance.parse(
      //     reader, firstSubLayer, sps_max_sublayers_minus1);
    }
  }
  this->sps_field_seq_flag              = reader.readFlag("sps_field_seq_flag");
  this->sps_vui_parameters_present_flag = reader.readFlag("sps_vui_parameters_present_flag");
  if (sps_vui_parameters_present_flag)
  {
    this->sps_vui_payload_size_minus1 = reader.readUEV("sps_vui_payload_size_minus1");
    while (!reader.byte_aligned())
    {
      this->sps_vui_alignment_zero_bit = reader.readFlag("sps_vui_alignment_zero_bit");
    }
    // TODO
    //this->vui_payload_instance.parse(reader, sps_vui_payload_size_minus1 + 1);
  }
  this->sps_extension_flag = reader.readFlag("sps_extension_flag");
  if (sps_extension_flag)
  {
    while (reader.more_rbsp_data())
    {
      this->sps_extension_data_flag = reader.readFlag("sps_extension_data_flag");
    }
  }
  // TODO
  //this->rbsp_trailing_bits_instance.parse(reader);
}

} // namespace parser::vvc
