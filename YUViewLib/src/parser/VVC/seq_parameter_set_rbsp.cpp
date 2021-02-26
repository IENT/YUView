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

#include <cmath>

namespace parser::vvc
{

using namespace parser::reader;

void seq_parameter_set_rbsp::parse(SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "seq_parameter_set_rbsp");

  this->sps_seq_parameter_set_id   = reader.readBits("sps_seq_parameter_set_id", 4);
  this->sps_video_parameter_set_id = reader.readBits("sps_video_parameter_set_id", 4);
  this->sps_max_sublayers_minus1   = reader.readBits("sps_max_sublayers_minus1", 3);
  this->sps_chroma_format_idc      = reader.readBits(
      "sps_chroma_format_idc",
      2,
      Options().withMeaningMap({{0, "Monochrome"}, {1, "4:2:0"}, {2, "4:2:2"}, {3, "4:4:4"}}));

  // Table 2
  {
    auto SubWidthCVec  = std::vector({1, 2, 2, 1});
    this->SubWidthC    = SubWidthCVec[this->sps_chroma_format_idc];
    auto SubHeightCVec = std::vector({1, 2, 1, 1});
    this->SubHeightC   = SubHeightCVec[this->sps_chroma_format_idc];
  }

  this->sps_log2_ctu_size_minus5 =
      reader.readBits("sps_log2_ctu_size_minus5", 2, Options().withCheckRange({0, 2}));
  this->sps_ptl_dpb_hrd_params_present_flag =
      reader.readFlag("sps_ptl_dpb_hrd_params_present_flag");

  // The variables CtbLog2SizeY and CtbSizeY are derived as follows:
  this->CtbLog2SizeY = this->sps_log2_ctu_size_minus5 + 5; // (35)
  this->CtbSizeY     = 1 << this->CtbLog2SizeY;            // (36)
  if (this->sps_ptl_dpb_hrd_params_present_flag)
  {
    this->profile_tier_level_instance.parse(reader, true, sps_max_sublayers_minus1);
  }
  this->sps_gdr_enabled_flag = reader.readFlag("sps_gdr_enabled_flag");
  this->sps_ref_pic_resampling_enabled_flag =
      reader.readFlag("sps_ref_pic_resampling_enabled_flag");
  if (this->sps_ref_pic_resampling_enabled_flag)
  {
    this->sps_res_change_in_clvs_allowed_flag =
        reader.readFlag("sps_res_change_in_clvs_allowed_flag");
  }
  this->sps_pic_width_max_in_luma_samples = reader.readUEV(
      "sps_pic_width_max_in_luma_samples"); // SubByteReaderLogging::Options(vps_ols_dpb_pic_width[i],
                                            // SubByteReaderLogging::Options::CheckType::SmallerEqual)
  this->sps_pic_height_max_in_luma_samples = reader.readUEV(
      "sps_pic_height_max_in_luma_samples"); // SubByteReaderLogging::Options(vps_ols_dpb_pic_height[i],
                                             // SubByteReaderLogging::Options::CheckType::SmallerEqual)
  this->sps_conformance_window_flag = reader.readFlag("sps_conformance_window_flag");
  if (this->sps_conformance_window_flag)
  {
    this->sps_conf_win_left_offset   = reader.readUEV("sps_conf_win_left_offset");
    this->sps_conf_win_right_offset  = reader.readUEV("sps_conf_win_right_offset");
    this->sps_conf_win_top_offset    = reader.readUEV("sps_conf_win_top_offset");
    this->sps_conf_win_bottom_offset = reader.readUEV("sps_conf_win_bottom_offset");
  }
  this->sps_subpic_info_present_flag = reader.readFlag(
      "sps_subpic_info_present_flag",
      this->sps_res_change_in_clvs_allowed_flag ? Options().withCheckEqualTo(0) : Options());
  if (this->sps_subpic_info_present_flag)
  {
    this->sps_num_subpics_minus1 = reader.readUEV(
        "sps_num_subpics_minus1"); // SubByteReaderLogging::Options(0, MaxSlicesPerAu)
    if (this->sps_num_subpics_minus1 > 0)
    {
      this->sps_independent_subpics_flag = reader.readFlag("sps_independent_subpics_flag");
      this->sps_subpic_same_size_flag    = reader.readFlag("sps_subpic_same_size_flag");
    }

    this->setDefaultSubpicValues(this->sps_num_subpics_minus1);
    for (unsigned i = 0; this->sps_num_subpics_minus1 > 0 && i <= this->sps_num_subpics_minus1; i++)
    {
      if (!this->sps_subpic_same_size_flag || i == 0)
      {
        auto tmpWidthVal =
            (this->sps_pic_width_max_in_luma_samples + CtbSizeY - 1) / this->CtbSizeY;
        auto tmpHeightVal =
            (this->sps_pic_height_max_in_luma_samples + CtbSizeY - 1) / this->CtbSizeY;
        if (i > 0 && this->sps_pic_width_max_in_luma_samples > CtbSizeY)
        {
          auto numBits = std::ceil(std::log2(tmpWidthVal));
          this->sps_subpic_ctu_top_left_x[i] =
              reader.readBits("sps_subpic_ctu_top_left_x", numBits);
        }
        if (i > 0 && this->sps_pic_height_max_in_luma_samples > CtbSizeY)
        {
          auto numBits = std::ceil(std::log2(tmpHeightVal));
          this->sps_subpic_ctu_top_left_y[i] =
              reader.readBits("sps_subpic_ctu_top_left_y", numBits);
        }
        if (i<this->sps_num_subpics_minus1 &&this->sps_pic_width_max_in_luma_samples> CtbSizeY)
        {
          auto numBits                     = std::ceil(std::log2(tmpWidthVal));
          this->sps_subpic_width_minus1[i] = reader.readBits("sps_subpic_width_minus1", numBits);
        }
        if (i<this->sps_num_subpics_minus1 &&this->sps_pic_height_max_in_luma_samples> CtbSizeY)
        {
          auto numBits                      = std::ceil(std::log2(tmpHeightVal));
          this->sps_subpic_height_minus1[i] = reader.readBits("sps_subpic_height_minus1", numBits);
        }
      }
      if (!this->sps_independent_subpics_flag)
      {
        this->sps_subpic_treated_as_pic_flag.push_back(
            reader.readFlag("sps_subpic_treated_as_pic_flag"));
        this->sps_loop_filter_across_subpic_enabled_flag.push_back(
            reader.readFlag("sps_loop_filter_across_subpic_enabled_flag"));
      }
      else
      {
        this->sps_subpic_treated_as_pic_flag.push_back(true);
        this->sps_loop_filter_across_subpic_enabled_flag.push_back(false);
      }
    }

    this->sps_subpic_id_len_minus1 =
        reader.readUEV("sps_subpic_id_len_minus1", Options().withCheckRange({0, 15}));
    this->sps_subpic_id_mapping_explicitly_signalled_flag =
        reader.readFlag("sps_subpic_id_mapping_explicitly_signalled_flag");
    if (this->sps_subpic_id_mapping_explicitly_signalled_flag)
    {
      this->sps_subpic_id_mapping_present_flag =
          reader.readFlag("sps_subpic_id_mapping_present_flag");
      if (this->sps_subpic_id_mapping_present_flag)
      {
        for (unsigned i = 0; i <= this->sps_num_subpics_minus1; i++)
        {
          auto numBits = sps_subpic_id_len_minus1 + 1;
          this->sps_subpic_id.push_back(reader.readBits("sps_subpic_id", numBits));
        }
      }
    }
  }
  else
  {
    this->setDefaultSubpicValues(this->sps_num_subpics_minus1);
  }

  this->sps_bitdepth_minus8 =
      reader.readUEV("sps_bitdepth_minus8", Options().withCheckRange({0, 2}));

  // (38) (39)
  this->BitDepth   = 8 + this->sps_bitdepth_minus8;
  this->QpBdOffset = 6 * this->sps_bitdepth_minus8;

  this->sps_entropy_coding_sync_enabled_flag =
      reader.readFlag("sps_entropy_coding_sync_enabled_flag");
  this->sps_entry_point_offsets_present_flag =
      reader.readFlag("sps_entry_point_offsets_present_flag");
  this->sps_log2_max_pic_order_cnt_lsb_minus4 = reader.readBits(
      "sps_log2_max_pic_order_cnt_lsb_minus4", 4, Options().withCheckRange({0, 12}));
  this->MaxPicOrderCntLsb      = (1u << (this->sps_log2_max_pic_order_cnt_lsb_minus4 + 4)); // (40)
  this->sps_poc_msb_cycle_flag = reader.readFlag("sps_poc_msb_cycle_flag");
  if (this->sps_poc_msb_cycle_flag)
  {
    this->sps_poc_msb_cycle_len_minus1 =
        reader.readUEV("sps_poc_msb_cycle_len_minus1", Options().withCheckRange({0, 32}));
  }
  this->sps_num_extra_ph_bytes =
      reader.readBits("sps_num_extra_ph_bytes", 2, Options().withCheckEqualTo(0));
  for (unsigned i = 0; i < (this->sps_num_extra_ph_bytes * 8); i++)
  {
    auto flag = reader.readFlag("sps_extra_ph_bit_present_flag");
    this->sps_extra_ph_bit_present_flag.push_back(flag);
    if (flag)
      this->NumExtraPhBits++; // (41)
  }

  this->sps_num_extra_sh_bytes =
      reader.readBits("sps_num_extra_sh_bytes", 2, Options().withCheckEqualTo(0));
  for (unsigned i = 0; i < (sps_num_extra_sh_bytes * 8); i++)
  {
    auto flag = reader.readFlag("sps_extra_sh_bit_present_flag");
    this->sps_extra_sh_bit_present_flag.push_back(flag);
    if (flag)
      this->NumExtraShBits++;
  }
  if (this->sps_ptl_dpb_hrd_params_present_flag)
  {
    if (this->sps_max_sublayers_minus1 > 0)
    {
      this->sps_sublayer_dpb_params_flag = reader.readFlag("sps_sublayer_dpb_params_flag");
    }
    this->dpb_parameters_instance.parse(
        reader, this->sps_max_sublayers_minus1, this->sps_sublayer_dpb_params_flag);
  }
  this->sps_log2_min_luma_coding_block_size_minus2 = reader.readUEV(
      "sps_log2_min_luma_coding_block_size_minus2",
      Options().withCheckRange({0, std::min(4u, this->sps_log2_ctu_size_minus5 + 3)}));

  // (43) -> (47)
  this->MinCbLog2SizeY = this->sps_log2_min_luma_coding_block_size_minus2 + 2;
  this->MinCbSizeY     = 1 << this->MinCbLog2SizeY;
  this->IbcBufWidthY   = 256 * 128 / this->CtbSizeY;
  this->IbcBufWidthC   = this->IbcBufWidthY / this->SubWidthC;
  this->VSize          = std::min(64u, this->CtbSizeY);

  this->sps_partition_constraints_override_enabled_flag =
      reader.readFlag("sps_partition_constraints_override_enabled_flag");
  this->sps_log2_diff_min_qt_min_cb_intra_slice_luma = reader.readUEV(
      "sps_log2_diff_min_qt_min_cb_intra_slice_luma",
      Options().withCheckRange({0, std::min(6u, this->CtbLog2SizeY) - this->MinCbLog2SizeY}));
  this->MinQtLog2SizeIntraY =
      this->sps_log2_diff_min_qt_min_cb_intra_slice_luma + this->MinCbLog2SizeY;
  this->sps_max_mtt_hierarchy_depth_intra_slice_luma = reader.readUEV(
      "sps_max_mtt_hierarchy_depth_intra_slice_luma",
      Options().withCheckRange({0, 2 * (this->CtbLog2SizeY - this->MinCbLog2SizeY)}));
  if (this->sps_max_mtt_hierarchy_depth_intra_slice_luma != 0)
  {
    this->sps_log2_diff_max_bt_min_qt_intra_slice_luma =
        reader.readUEV("sps_log2_diff_max_bt_min_qt_intra_slice_luma",
                       Options().withCheckRange({0, CtbLog2SizeY}));
    this->sps_log2_diff_max_tt_min_qt_intra_slice_luma =
        reader.readUEV("sps_log2_diff_max_tt_min_qt_intra_slice_luma",
                       Options().withCheckRange(
                           {0, std::min(6u, this->CtbLog2SizeY) - this->MinQtLog2SizeIntraY}));
  }
  if (this->sps_chroma_format_idc != 0)
  {
    Options opt;
    if (this->sps_log2_diff_max_bt_min_qt_intra_slice_luma >
        std::min(6u, this->CtbLog2SizeY) - this->MinQtLog2SizeIntraY)
      opt = Options().withCheckEqualTo(0);
    this->sps_qtbtt_dual_tree_intra_flag = reader.readFlag("sps_qtbtt_dual_tree_intra_flag", opt);
  }
  if (this->sps_qtbtt_dual_tree_intra_flag)
  {
    this->sps_log2_diff_min_qt_min_cb_intra_slice_chroma =
        reader.readUEV("sps_log2_diff_min_qt_min_cb_intra_slice_chroma",
                       Options().withCheckRange({0, std::min(6u, CtbLog2SizeY) - MinCbLog2SizeY}));
    this->MinQtLog2SizeIntraC =
        this->sps_log2_diff_min_qt_min_cb_intra_slice_chroma + this->MinCbLog2SizeY; // (51)
    this->sps_max_mtt_hierarchy_depth_intra_slice_chroma =
        reader.readUEV("sps_max_mtt_hierarchy_depth_intra_slice_chroma",
                       Options().withCheckRange({0, 2 * (CtbLog2SizeY - MinCbLog2SizeY)}));
    if (this->sps_max_mtt_hierarchy_depth_intra_slice_chroma != 0)
    {
      this->sps_log2_diff_max_bt_min_qt_intra_slice_chroma = reader.readUEV(
          "sps_log2_diff_max_bt_min_qt_intra_slice_chroma",
          Options().withCheckRange({0, std::min(6u, CtbLog2SizeY) - MinQtLog2SizeIntraC}));
      this->sps_log2_diff_max_tt_min_qt_intra_slice_chroma = reader.readUEV(
          "sps_log2_diff_max_tt_min_qt_intra_slice_chroma",
          Options().withCheckRange({0, std::min(6u, CtbLog2SizeY) - MinQtLog2SizeIntraC}));
    }
  }
  this->sps_log2_diff_min_qt_min_cb_inter_slice =
      reader.readUEV("sps_log2_diff_min_qt_min_cb_inter_slice",
                     Options().withCheckRange({0, std::min(6u, CtbLog2SizeY) - MinCbLog2SizeY}));
  this->MinQtLog2SizeInterY =
      this->sps_log2_diff_min_qt_min_cb_inter_slice + this->MinCbLog2SizeY; // (52)
  this->sps_max_mtt_hierarchy_depth_inter_slice =
      reader.readUEV("sps_max_mtt_hierarchy_depth_inter_slice",
                     Options().withCheckRange({0, 2 * (CtbLog2SizeY - MinCbLog2SizeY)}));
  if (this->sps_max_mtt_hierarchy_depth_inter_slice != 0)
  {
    this->sps_log2_diff_max_bt_min_qt_inter_slice =
        reader.readUEV("sps_log2_diff_max_bt_min_qt_inter_slice",
                       Options().withCheckRange({0, CtbLog2SizeY - MinQtLog2SizeInterY}));
    this->sps_log2_diff_max_tt_min_qt_inter_slice = reader.readUEV(
        "sps_log2_diff_max_tt_min_qt_inter_slice",
        Options().withCheckRange({0, std::min(6u, CtbLog2SizeY) - MinQtLog2SizeInterY}));
  }
  if (this->CtbSizeY > 32)
  {
    this->sps_max_luma_transform_size_64_flag =
        reader.readFlag("sps_max_luma_transform_size_64_flag");
  }
  this->sps_transform_skip_enabled_flag = reader.readFlag("sps_transform_skip_enabled_flag");
  if (this->sps_transform_skip_enabled_flag)
  {
    this->sps_log2_transform_skip_max_size_minus2 =
        reader.readUEV("sps_log2_transform_skip_max_size_minus2");
    this->sps_bdpcm_enabled_flag = reader.readFlag("sps_bdpcm_enabled_flag");
  }
  this->sps_mts_enabled_flag = reader.readFlag("sps_mts_enabled_flag");
  if (this->sps_mts_enabled_flag)
  {
    this->sps_explicit_mts_intra_enabled_flag =
        reader.readFlag("sps_explicit_mts_intra_enabled_flag");
    this->sps_explicit_mts_inter_enabled_flag =
        reader.readFlag("sps_explicit_mts_inter_enabled_flag");
  }
  this->sps_lfnst_enabled_flag = reader.readFlag("sps_lfnst_enabled_flag");
  if (this->sps_chroma_format_idc != 0)
  {
    this->sps_joint_cbcr_enabled_flag       = reader.readFlag("sps_joint_cbcr_enabled_flag");
    this->sps_same_qp_table_for_chroma_flag = reader.readFlag("sps_same_qp_table_for_chroma_flag");
    auto numQpTables                        = this->sps_same_qp_table_for_chroma_flag
                                                  ? 1u
                                                  : (this->sps_joint_cbcr_enabled_flag ? 3u : 2u);
    for (unsigned i = 0; i < numQpTables; i++)
    {
      this->sps_qp_table_start_minus26.push_back(reader.readSEV("sps_qp_table_start_minus26"));
      this->sps_num_points_in_qp_table_minus1.push_back(
          reader.readUEV("sps_num_points_in_qp_table_minus1"));
      this->sps_delta_qp_in_val_minus1.push_back({});
      this->sps_delta_qp_diff_val.push_back({});
      for (unsigned j = 0; j <= this->sps_num_points_in_qp_table_minus1[i]; j++)
      {
        this->sps_delta_qp_in_val_minus1[i].push_back(reader.readUEV("sps_delta_qp_in_val_minus1"));
        this->sps_delta_qp_diff_val[i].push_back(reader.readUEV("sps_delta_qp_diff_val"));
      }
    }
  }
  this->sps_sao_enabled_flag = reader.readFlag("sps_sao_enabled_flag");
  this->sps_alf_enabled_flag = reader.readFlag("sps_alf_enabled_flag");
  if (this->sps_alf_enabled_flag && this->sps_chroma_format_idc != 0)
  {
    this->sps_ccalf_enabled_flag = reader.readFlag("sps_ccalf_enabled_flag");
  }
  this->sps_lmcs_enabled_flag       = reader.readFlag("sps_lmcs_enabled_flag");
  this->sps_weighted_pred_flag      = reader.readFlag("sps_weighted_pred_flag");
  this->sps_weighted_bipred_flag    = reader.readFlag("sps_weighted_bipred_flag");
  this->sps_long_term_ref_pics_flag = reader.readFlag("sps_long_term_ref_pics_flag");
  if (this->sps_video_parameter_set_id > 0)
  {
    this->sps_inter_layer_prediction_enabled_flag = reader.readFlag(
        "sps_inter_layer_prediction_enabled_flag",
        this->sps_video_parameter_set_id == 0 ? Options().withCheckEqualTo(0) : Options());
  }
  this->sps_idr_rpl_present_flag   = reader.readFlag("sps_idr_rpl_present_flag");
  this->sps_rpl1_same_as_rpl0_flag = reader.readFlag("sps_rpl1_same_as_rpl0_flag");
  {
    auto refPicSubLevel            = SubByteReaderLoggingSubLevel(reader, "Ref Pic List 0");
    this->sps_num_ref_pic_lists[0] = reader.readUEV("sps_num_ref_pic_lists");
    for (unsigned j = 0; j < this->sps_num_ref_pic_lists[0]; j++)
    {
      ref_pic_list_struct rpl;
      rpl.parse(reader, 0, j, shared_from_this());
      this->ref_pic_list_structs[0].push_back(rpl);
    }
  }
  {
    auto refPicSubLevel = SubByteReaderLoggingSubLevel(reader, "Ref Pic List 1");
    if (this->sps_rpl1_same_as_rpl0_flag)
    {
      this->sps_num_ref_pic_lists[1] = this->sps_num_ref_pic_lists[0];
      for (unsigned j = 0; j < this->sps_num_ref_pic_lists[1]; j++)
      {
        this->ref_pic_list_structs[1].push_back(this->ref_pic_list_structs[0][j]);
      }
    }
    else
    {
      this->sps_num_ref_pic_lists[1] = reader.readUEV("sps_num_ref_pic_lists");
      for (unsigned j = 0; j < this->sps_num_ref_pic_lists[1]; j++)
      {
        ref_pic_list_struct rpl;
        rpl.parse(reader, 1, j, shared_from_this());
        this->ref_pic_list_structs[1].push_back(rpl);
      }
    }
  }
  this->sps_ref_wraparound_enabled_flag = reader.readFlag("sps_ref_wraparound_enabled_flag");
  this->sps_temporal_mvp_enabled_flag   = reader.readFlag("sps_temporal_mvp_enabled_flag");
  if (this->sps_temporal_mvp_enabled_flag)
  {
    this->sps_sbtmvp_enabled_flag = reader.readFlag("sps_sbtmvp_enabled_flag");
  }
  this->sps_amvr_enabled_flag = reader.readFlag("sps_amvr_enabled_flag");
  this->sps_bdof_enabled_flag = reader.readFlag("sps_bdof_enabled_flag");
  if (this->sps_bdof_enabled_flag)
  {
    this->sps_bdof_control_present_in_ph_flag =
        reader.readFlag("sps_bdof_control_present_in_ph_flag");
  }
  this->sps_smvd_enabled_flag = reader.readFlag("sps_smvd_enabled_flag");
  this->sps_dmvr_enabled_flag = reader.readFlag("sps_dmvr_enabled_flag");
  if (this->sps_dmvr_enabled_flag)
  {
    this->sps_dmvr_control_present_in_ph_flag =
        reader.readFlag("sps_dmvr_control_present_in_ph_flag");
  }
  this->sps_mmvd_enabled_flag = reader.readFlag("sps_mmvd_enabled_flag");
  if (this->sps_mmvd_enabled_flag)
  {
    this->sps_mmvd_fullpel_only_enabled_flag =
        reader.readFlag("sps_mmvd_fullpel_only_enabled_flag");
  }
  this->sps_six_minus_max_num_merge_cand =
      reader.readUEV("sps_six_minus_max_num_merge_cand", Options().withCheckRange({0, 5}));
  this->MaxNumMergeCand         = 6 - this->sps_six_minus_max_num_merge_cand;
  this->sps_sbt_enabled_flag    = reader.readFlag("sps_sbt_enabled_flag");
  this->sps_affine_enabled_flag = reader.readFlag("sps_affine_enabled_flag");
  if (this->sps_affine_enabled_flag)
  {
    this->sps_five_minus_max_num_subblock_merge_cand = reader.readUEV(
        "sps_five_minus_max_num_subblock_merge_cand", Options().withCheckRange({0, 5}));
    this->sps_6param_affine_enabled_flag = reader.readFlag("sps_6param_affine_enabled_flag");
    if (this->sps_amvr_enabled_flag)
    {
      this->sps_affine_amvr_enabled_flag = reader.readFlag("sps_affine_amvr_enabled_flag");
    }
    this->sps_affine_prof_enabled_flag = reader.readFlag("sps_affine_prof_enabled_flag");
    if (this->sps_affine_prof_enabled_flag)
    {
      this->sps_prof_control_present_in_ph_flag =
          reader.readFlag("sps_prof_control_present_in_ph_flag");
    }
  }
  this->sps_bcw_enabled_flag  = reader.readFlag("sps_bcw_enabled_flag");
  this->sps_ciip_enabled_flag = reader.readFlag("sps_ciip_enabled_flag");
  if (this->MaxNumMergeCand >= 2)
  {
    this->sps_gpm_enabled_flag = reader.readFlag("sps_gpm_enabled_flag");
    if (this->sps_gpm_enabled_flag && this->MaxNumMergeCand >= 3)
    {
      this->sps_max_num_merge_cand_minus_max_num_gpm_cand =
          reader.readUEV("sps_max_num_merge_cand_minus_max_num_gpm_cand",
                         Options().withCheckRange({0, this->MaxNumMergeCand}));
    }
  }
  this->sps_log2_parallel_merge_level_minus2 = reader.readUEV(
      "sps_log2_parallel_merge_level_minus2", Options().withCheckRange({0, this->CtbLog2SizeY}));
  this->sps_isp_enabled_flag = reader.readFlag("sps_isp_enabled_flag");
  this->sps_mrl_enabled_flag = reader.readFlag("sps_mrl_enabled_flag");
  this->sps_mip_enabled_flag = reader.readFlag("sps_mip_enabled_flag");
  if (this->sps_chroma_format_idc != 0)
  {
    this->sps_cclm_enabled_flag = reader.readFlag("sps_cclm_enabled_flag");
  }
  if (this->sps_chroma_format_idc == 1)
  {
    this->sps_chroma_horizontal_collocated_flag =
        reader.readFlag("sps_chroma_horizontal_collocated_flag");
    this->sps_chroma_vertical_collocated_flag =
        reader.readFlag("sps_chroma_vertical_collocated_flag");
  }
  this->sps_palette_enabled_flag = reader.readFlag("sps_palette_enabled_flag");
  if (this->sps_chroma_format_idc == 3 && !this->sps_max_luma_transform_size_64_flag)
  {
    this->sps_act_enabled_flag = reader.readFlag("sps_act_enabled_flag");
  }
  if (this->sps_transform_skip_enabled_flag || this->sps_palette_enabled_flag)
  {
    this->sps_min_qp_prime_ts =
        reader.readUEV("sps_min_qp_prime_ts", Options().withCheckRange({0, 8}));
  }
  this->sps_ibc_enabled_flag = reader.readFlag("sps_ibc_enabled_flag");
  if (this->sps_ibc_enabled_flag)
  {
    this->sps_six_minus_max_num_ibc_merge_cand =
        reader.readUEV("sps_six_minus_max_num_ibc_merge_cand", Options().withCheckRange({0, 5}));
  }
  this->sps_ladf_enabled_flag = reader.readFlag("sps_ladf_enabled_flag");
  if (this->sps_ladf_enabled_flag)
  {
    this->sps_num_ladf_intervals_minus2 =
        reader.readBits("sps_num_ladf_intervals_minus2", 2, Options().withCheckRange({0, 3}));
    this->sps_ladf_lowest_interval_qp_offset =
        reader.readSEV("sps_ladf_lowest_interval_qp_offset", Options().withCheckRange({-63, 63}));
    for (unsigned i = 0; i < this->sps_num_ladf_intervals_minus2 + 1; i++)
    {
      this->sps_ladf_qp_offset.push_back(reader.readSEV("sps_ladf_qp_offset"));
      this->sps_ladf_delta_threshold_minus1.push_back(
          reader.readUEV("sps_ladf_delta_threshold_minus1"));
    }
  }
  this->sps_explicit_scaling_list_enabled_flag =
      reader.readFlag("sps_explicit_scaling_list_enabled_flag");
  if (this->sps_lfnst_enabled_flag && this->sps_explicit_scaling_list_enabled_flag)
  {
    this->sps_scaling_matrix_for_lfnst_disabled_flag =
        reader.readFlag("sps_scaling_matrix_for_lfnst_disabled_flag");
  }
  if (this->sps_act_enabled_flag && this->sps_explicit_scaling_list_enabled_flag)
  {
    this->sps_scaling_matrix_for_alternative_colour_space_disabled_flag =
        reader.readFlag("sps_scaling_matrix_for_alternative_colour_space_disabled_flag");
  }
  if (this->sps_scaling_matrix_for_alternative_colour_space_disabled_flag)
  {
    this->sps_scaling_matrix_designated_colour_space_flag =
        reader.readFlag("sps_scaling_matrix_designated_colour_space_flag");
  }
  this->sps_dep_quant_enabled_flag        = reader.readFlag("sps_dep_quant_enabled_flag");
  this->sps_sign_data_hiding_enabled_flag = reader.readFlag("sps_sign_data_hiding_enabled_flag");
  this->sps_virtual_boundaries_enabled_flag =
      reader.readFlag("sps_virtual_boundaries_enabled_flag");
  if (this->sps_virtual_boundaries_enabled_flag)
  {
    {
      Options opt;
      if (this->sps_res_change_in_clvs_allowed_flag)
        opt = Options().withCheckEqualTo(0);
      else if (this->sps_subpic_info_present_flag && this->sps_virtual_boundaries_enabled_flag)
        opt = Options().withCheckEqualTo(1);
      this->sps_virtual_boundaries_present_flag =
          reader.readFlag("sps_virtual_boundaries_present_flag", opt);
    }
    if (this->sps_virtual_boundaries_present_flag)
    {
      this->sps_num_ver_virtual_boundaries = reader.readUEV(
          "sps_num_ver_virtual_boundaries",
          Options().withCheckRange({0, (this->sps_pic_width_max_in_luma_samples <= 8 ? 0 : 3)}));
      for (unsigned i = 0; i < sps_num_ver_virtual_boundaries; i++)
      {
        this->sps_virtual_boundary_pos_x_minus1.push_back(
            reader.readUEV("sps_virtual_boundary_pos_x_minus1"));
      }
      this->sps_num_hor_virtual_boundaries = reader.readUEV(
          "sps_num_hor_virtual_boundaries",
          Options().withCheckRange({0, (this->sps_pic_height_max_in_luma_samples <= 8 ? 0 : 3)}));
      for (unsigned i = 0; i < this->sps_num_hor_virtual_boundaries; i++)
      {
        this->sps_virtual_boundary_pos_y_minus1.push_back(
            reader.readUEV("sps_virtual_boundary_pos_y_minus1"));
      }
    }
  }
  if (this->sps_ptl_dpb_hrd_params_present_flag)
  {
    this->sps_timing_hrd_params_present_flag =
        reader.readFlag("sps_timing_hrd_params_present_flag");
    if (this->sps_timing_hrd_params_present_flag)
    {
      this->general_timing_hrd_parameters_instance.parse(reader);
      if (this->sps_max_sublayers_minus1 > 0)
      {
        this->sps_sublayer_cpb_params_present_flag =
            reader.readFlag("sps_sublayer_cpb_params_present_flag");
      }
      auto firstSubLayer =
          this->sps_sublayer_cpb_params_present_flag ? 0u : this->sps_max_sublayers_minus1;
      this->ols_timing_hrd_parameters_instance.parse(reader,
                                                     firstSubLayer,
                                                     this->sps_max_sublayers_minus1,
                                                     &this->general_timing_hrd_parameters_instance);
    }
  }
  this->sps_field_seq_flag              = reader.readFlag("sps_field_seq_flag");
  this->sps_vui_parameters_present_flag = reader.readFlag("sps_vui_parameters_present_flag");
  if (this->sps_vui_parameters_present_flag)
  {
    this->sps_vui_payload_size_minus1 =
        reader.readUEV("sps_vui_payload_size_minus1", Options().withCheckRange({0, 1023}));
    while (!reader.byte_aligned())
    {
      this->sps_vui_alignment_zero_bit =
          reader.readFlag("sps_vui_alignment_zero_bit", Options().withCheckEqualTo(0));
    }
    this->vui_payload_instance.parse(reader, this->sps_vui_payload_size_minus1 + 1);
  }
  this->sps_extension_flag = reader.readFlag("sps_extension_flag", Options().withCheckEqualTo(0));
  if (this->sps_extension_flag)
  {
    while (reader.more_rbsp_data())
    {
      this->sps_extension_data_flag = reader.readFlag("sps_extension_data_flag");
    }
  }
  this->rbsp_trailing_bits_instance.parse(reader);
}

void seq_parameter_set_rbsp::setDefaultSubpicValues(unsigned numSubPic)
{
  for (unsigned i = 0; i <= numSubPic; i++)
  {
    // If not present, create default values
    auto tmpWidthVal =
        (this->sps_pic_width_max_in_luma_samples + this->CtbSizeY - 1) / this->CtbSizeY;
    auto tmpHeightVal =
        (this->sps_pic_height_max_in_luma_samples + this->CtbSizeY - 1) / this->CtbSizeY;
    if (!this->sps_subpic_same_size_flag || i == 0)
    {
      this->sps_subpic_ctu_top_left_x.push_back(0);
      this->sps_subpic_ctu_top_left_y.push_back(0);
      this->sps_subpic_width_minus1.push_back(tmpWidthVal - this->sps_subpic_ctu_top_left_x[i] - 1);
      this->sps_subpic_height_minus1.push_back(tmpHeightVal - sps_subpic_ctu_top_left_y[i] - 1);
    }
    else
    {
      auto numSubpicCols = tmpWidthVal / (this->sps_subpic_width_minus1[0] + 1); // (37)
      this->sps_subpic_ctu_top_left_x.push_back((i % numSubpicCols) *
                                                (this->sps_subpic_width_minus1[0] + 1));
      this->sps_subpic_ctu_top_left_y.push_back((i / numSubpicCols) *
                                                (this->sps_subpic_height_minus1[0] + 1));
      this->sps_subpic_width_minus1.push_back(this->sps_subpic_width_minus1[0]);
      this->sps_subpic_height_minus1.push_back(sps_subpic_height_minus1[0]);
    }
  }
}

} // namespace parser::vvc
