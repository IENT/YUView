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

#include "picture_header_structure.h"

#include "pic_parameter_set_rbsp.h"
#include "seq_parameter_set_rbsp.h"
#include "slice_layer_rbsp.h"
#include "video_parameter_set_rbsp.h"

namespace parser::vvc
{

using namespace parser::reader;

void picture_header_structure::parse(SubByteReaderLogging &            reader,
                                     VPSMap &                          vpsMap,
                                     SPSMap &                          spsMap,
                                     PPSMap &                          ppsMap,
                                     std::shared_ptr<slice_layer_rbsp> sl)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "picture_header_structure");

  this->ph_gdr_or_irap_pic_flag = reader.readFlag("ph_gdr_or_irap_pic_flag");
  this->ph_non_ref_pic_flag     = reader.readFlag("ph_non_ref_pic_flag");
  if (this->ph_gdr_or_irap_pic_flag)
  {
    this->ph_gdr_pic_flag = reader.readFlag("ph_gdr_pic_flag");
  }
  {
    // if (this->ph_gdr_or_irap_pic_flag && !this->ph_gdr_pic_flag && vps_independent_layer_flag[
    // GeneralLayerIdx[ nuh_layer_id ] ] )
    // the value should be 0
    this->ph_inter_slice_allowed_flag = reader.readFlag("ph_inter_slice_allowed_flag");
  }
  if (this->ph_inter_slice_allowed_flag)
  {
    this->ph_intra_slice_allowed_flag = reader.readFlag("ph_intra_slice_allowed_flag");
  }
  this->ph_pic_parameter_set_id =
      reader.readUEV("ph_pic_parameter_set_id", Options().withCheckRange({0, 63}));

  if (ppsMap.count(this->ph_pic_parameter_set_id) == 0)
    throw std::logic_error("PPS with given ph_pic_parameter_set_id not found.");
  auto pps = ppsMap[this->ph_pic_parameter_set_id];

  if (spsMap.count(pps->pps_seq_parameter_set_id) == 0)
    throw std::logic_error("SPS with given pps_seq_parameter_set_id not found.");
  auto sps = spsMap[pps->pps_seq_parameter_set_id];

  std::shared_ptr<video_parameter_set_rbsp> vps;
  if (sps->sps_video_parameter_set_id > 0)
  {
    if (vpsMap.count(sps->sps_video_parameter_set_id) == 0)
      throw std::logic_error("VPS with given sps_video_parameter_set_id not found.");
    vps = vpsMap[sps->sps_video_parameter_set_id];
  }
  else
  {
    vps = std::make_shared<video_parameter_set_rbsp>();
  }

  if (!sps->sps_gdr_enabled_flag && this->ph_gdr_pic_flag)
  {
    throw std::logic_error("When sps_gdr_enabled_flag is equal to 0, the value of ph_gdr_pic_flag "
                           "shall be equal to 0.");
  }

  {
    auto nrBits                = sps->sps_log2_max_pic_order_cnt_lsb_minus4 + 4;
    this->ph_pic_order_cnt_lsb = reader.readBits(
        "ph_pic_order_cnt_lsb", nrBits, Options().withCheckRange({0, sps->MaxPicOrderCntLsb - 1}));
  }
  if (this->ph_gdr_pic_flag)
  {
    this->ph_recovery_poc_cnt = reader.readUEV(
        "ph_recovery_poc_cnt", Options().withCheckRange({0, sps->MaxPicOrderCntLsb}));
  }
  for (unsigned i = 0; i < sps->NumExtraPhBits; i++)
  {
    this->ph_extra_bit.push_back(reader.readFlag("ph_extra_bit"));
  }
  if (sps->sps_poc_msb_cycle_flag)
  {
    this->ph_poc_msb_cycle_present_flag = reader.readFlag("ph_poc_msb_cycle_present_flag");
    if (this->ph_poc_msb_cycle_present_flag)
    {
      auto nrBits                = sps->sps_poc_msb_cycle_len_minus1 + 1;
      this->ph_poc_msb_cycle_val = reader.readBits("ph_poc_msb_cycle_val", nrBits);
    }
  }
  if (sps->sps_alf_enabled_flag && pps->pps_alf_info_in_ph_flag)
  {
    this->ph_alf_enabled_flag = reader.readFlag("ph_alf_enabled_flag");
    if (this->ph_alf_enabled_flag)
    {
      this->ph_num_alf_aps_ids_luma = reader.readBits("ph_num_alf_aps_ids_luma", 3);
      for (unsigned i = 0; i < ph_num_alf_aps_ids_luma; i++)
      {
        this->ph_alf_aps_id_luma.push_back(reader.readBits("ph_alf_aps_id_luma", 3));
      }
      if (sps->sps_chroma_format_idc != 0)
      {
        this->ph_alf_cb_enabled_flag = reader.readFlag("ph_alf_cb_enabled_flag");
        this->ph_alf_cr_enabled_flag = reader.readFlag("ph_alf_cr_enabled_flag");
      }
      if (this->ph_alf_cb_enabled_flag || ph_alf_cr_enabled_flag)
      {
        this->ph_alf_aps_id_chroma = reader.readBits("ph_alf_aps_id_chroma", 3);
      }
      if (sps->sps_ccalf_enabled_flag)
      {
        this->ph_alf_cc_cb_enabled_flag = reader.readFlag("ph_alf_cc_cb_enabled_flag");
        if (this->ph_alf_cc_cb_enabled_flag)
        {
          this->ph_alf_cc_cb_aps_id = reader.readBits("ph_alf_cc_cb_aps_id", 3);
        }
        this->ph_alf_cc_cr_enabled_flag = reader.readFlag("ph_alf_cc_cr_enabled_flag");
        if (this->ph_alf_cc_cr_enabled_flag)
        {
          this->ph_alf_cc_cr_aps_id = reader.readBits("ph_alf_cc_cr_aps_id", 3);
        }
      }
    }
  }
  if (sps->sps_lmcs_enabled_flag)
  {
    this->ph_lmcs_enabled_flag = reader.readFlag("ph_lmcs_enabled_flag");
    if (this->ph_lmcs_enabled_flag)
    {
      this->ph_lmcs_aps_id = reader.readBits("ph_lmcs_aps_id", 2);
      if (sps->sps_chroma_format_idc != 0)
      {
        this->ph_chroma_residual_scale_flag = reader.readFlag("ph_chroma_residual_scale_flag");
      }
    }
  }
  if (sps->sps_explicit_scaling_list_enabled_flag)
  {
    this->ph_explicit_scaling_list_enabled_flag =
        reader.readFlag("ph_explicit_scaling_list_enabled_flag");
    if (this->ph_explicit_scaling_list_enabled_flag)
    {
      this->ph_scaling_list_aps_id = reader.readBits("ph_scaling_list_aps_id", 3);
    }
  }
  if (sps->sps_virtual_boundaries_enabled_flag && !sps->sps_virtual_boundaries_present_flag)
  {
    this->ph_virtual_boundaries_present_flag =
        reader.readFlag("ph_virtual_boundaries_present_flag");
    if (this->ph_virtual_boundaries_present_flag)
    {
      this->ph_num_ver_virtual_boundaries = reader.readUEV(
          "ph_num_ver_virtual_boundaries",
          Options().withCheckRange({0, (pps->pps_pic_width_in_luma_samples <= 8 ? 0 : 3)}));
      for (unsigned i = 0; i < ph_num_ver_virtual_boundaries; i++)
      {
        this->ph_virtual_boundary_pos_x_minus1.push_back(
            reader.readUEV("ph_virtual_boundary_pos_x_minus1"));
      }
      this->ph_num_hor_virtual_boundaries = reader.readUEV(
          "ph_num_hor_virtual_boundaries",
          Options().withCheckRange({0, (pps->pps_pic_height_in_luma_samples <= 8 ? 0 : 3)}));
      for (unsigned i = 0; i < ph_num_hor_virtual_boundaries; i++)
      {
        this->ph_virtual_boundary_pos_y_minus1.push_back(
            reader.readUEV("ph_virtual_boundary_pos_y_minus1"));
      }
    }
  }
  if (pps->pps_output_flag_present_flag && !this->ph_non_ref_pic_flag)
  {
    this->ph_pic_output_flag = reader.readFlag("ph_pic_output_flag");
  }
  if (pps->pps_rpl_info_in_ph_flag)
  {
    this->ref_pic_lists_instance = std::make_shared<ref_pic_lists>();
    this->ref_pic_lists_instance->parse(reader, sps, pps);
  }
  if (sps->sps_partition_constraints_override_enabled_flag)
  {
    this->ph_partition_constraints_override_flag =
        reader.readFlag("ph_partition_constraints_override_flag");
  }

  // Inferred defaults
  this->ph_log2_diff_min_qt_min_cb_intra_slice_luma =
      sps->sps_log2_diff_min_qt_min_cb_intra_slice_luma;
  this->ph_max_mtt_hierarchy_depth_intra_slice_luma =
      sps->sps_max_mtt_hierarchy_depth_intra_slice_luma;
  this->ph_log2_diff_max_bt_min_qt_intra_slice_luma =
      sps->sps_log2_diff_max_bt_min_qt_intra_slice_luma;
  this->ph_log2_diff_max_tt_min_qt_intra_slice_luma =
      sps->sps_log2_diff_max_tt_min_qt_intra_slice_luma;
  this->ph_log2_diff_min_qt_min_cb_intra_slice_chroma =
      sps->sps_log2_diff_min_qt_min_cb_intra_slice_chroma;
  this->ph_max_mtt_hierarchy_depth_intra_slice_chroma =
      sps->sps_max_mtt_hierarchy_depth_intra_slice_chroma;
  this->ph_log2_diff_max_bt_min_qt_intra_slice_chroma =
      sps->sps_log2_diff_max_bt_min_qt_intra_slice_chroma;
  this->ph_log2_diff_max_tt_min_qt_intra_slice_chroma =
      sps->sps_log2_diff_max_tt_min_qt_intra_slice_chroma;

  if (this->ph_intra_slice_allowed_flag)
  {
    if (this->ph_partition_constraints_override_flag)
    {
      this->ph_log2_diff_min_qt_min_cb_intra_slice_luma = reader.readUEV(
          "ph_log2_diff_min_qt_min_cb_intra_slice_luma",
          Options().withCheckRange({0, std::min(6u, sps->CtbLog2SizeY) - sps->MinCbLog2SizeY}));
      this->ph_max_mtt_hierarchy_depth_intra_slice_luma = reader.readUEV(
          "ph_max_mtt_hierarchy_depth_intra_slice_luma",
          Options().withCheckRange({0, 2 * (sps->CtbLog2SizeY - sps->MinCbLog2SizeY)}));
      if (this->ph_max_mtt_hierarchy_depth_intra_slice_luma != 0)
      {
        this->ph_log2_diff_max_bt_min_qt_intra_slice_luma =
            reader.readUEV("ph_log2_diff_max_bt_min_qt_intra_slice_luma",
                           Options().withCheckRange({0,
                                                     (sps->sps_qtbtt_dual_tree_intra_flag
                                                          ? std::min(6u, sps->CtbLog2SizeY)
                                                          : sps->CtbLog2SizeY) -
                                                         sps->MinQtLog2SizeIntraY}));
        this->ph_log2_diff_max_tt_min_qt_intra_slice_luma =
            reader.readUEV("ph_log2_diff_max_tt_min_qt_intra_slice_luma",
                           Options().withCheckRange(
                               {0, std::min(6u, sps->CtbLog2SizeY) - sps->MinQtLog2SizeIntraY}));
      }
      if (sps->sps_qtbtt_dual_tree_intra_flag)
      {
        this->ph_log2_diff_min_qt_min_cb_intra_slice_chroma = reader.readUEV(
            "ph_log2_diff_min_qt_min_cb_intra_slice_chroma",
            Options().withCheckRange({0, std::min(6u, sps->CtbLog2SizeY) - sps->MinCbLog2SizeY}));
        this->ph_max_mtt_hierarchy_depth_intra_slice_chroma = reader.readUEV(
            "ph_max_mtt_hierarchy_depth_intra_slice_chroma",
            Options().withCheckRange({0, 2 * (sps->CtbLog2SizeY - sps->MinCbLog2SizeY)}));
        if (this->ph_max_mtt_hierarchy_depth_intra_slice_chroma != 0)
        {
          this->ph_log2_diff_max_bt_min_qt_intra_slice_chroma =
              reader.readUEV("ph_log2_diff_max_bt_min_qt_intra_slice_chroma",
                             Options().withCheckRange(
                                 {0, std::min(6u, sps->CtbLog2SizeY) - sps->MinQtLog2SizeIntraC}));
          this->ph_log2_diff_max_tt_min_qt_intra_slice_chroma =
              reader.readUEV("ph_log2_diff_max_tt_min_qt_intra_slice_chroma",
                             Options().withCheckRange(
                                 {0, std::min(6u, sps->CtbLog2SizeY) - sps->MinQtLog2SizeIntraC}));
        }
      }
    }
    if (pps->pps_cu_qp_delta_enabled_flag)
    {
      this->ph_cu_qp_delta_subdiv_intra_slice =
          reader.readUEV("ph_cu_qp_delta_subdiv_intra_slice", Options().withCheckRange({0, 2}));
    }
    if (pps->pps_cu_chroma_qp_offset_list_enabled_flag)
    {
      this->ph_cu_chroma_qp_offset_subdiv_intra_slice = reader.readUEV(
          "ph_cu_chroma_qp_offset_subdiv_intra_slice", Options().withCheckRange({0, 2}));
    }
  }

  // Inferred defaults
  this->ph_log2_diff_min_qt_min_cb_inter_slice = sps->sps_log2_diff_min_qt_min_cb_inter_slice;
  this->ph_max_mtt_hierarchy_depth_inter_slice = sps->sps_max_mtt_hierarchy_depth_inter_slice;
  this->ph_log2_diff_max_bt_min_qt_inter_slice = sps->sps_log2_diff_max_bt_min_qt_inter_slice;
  this->ph_log2_diff_max_tt_min_qt_inter_slice = sps->sps_log2_diff_max_tt_min_qt_inter_slice;
  if (sps->sps_bdof_control_present_in_ph_flag)
  {
    this->ph_bdof_disabled_flag = true;
  }
  if (sps->sps_dmvr_control_present_in_ph_flag)
  {
    this->ph_dmvr_disabled_flag = true;
  }
  this->ph_prof_disabled_flag = sps->sps_affine_prof_enabled_flag ? false : true;

  if (this->ph_inter_slice_allowed_flag)
  {
    if (this->ph_partition_constraints_override_flag)
    {
      this->ph_log2_diff_min_qt_min_cb_inter_slice = reader.readUEV(
          "ph_log2_diff_min_qt_min_cb_inter_slice",
          Options().withCheckRange({0, std::min(6u, sps->CtbLog2SizeY) - sps->MinCbLog2SizeY}));
      this->ph_max_mtt_hierarchy_depth_inter_slice = reader.readUEV(
          "ph_max_mtt_hierarchy_depth_inter_slice",
          Options().withCheckRange({0, 2 * (sps->CtbLog2SizeY - sps->MinCbLog2SizeY)}));
      if (this->ph_max_mtt_hierarchy_depth_inter_slice != 0)
      {
        this->ph_log2_diff_max_bt_min_qt_inter_slice = reader.readUEV(
            "ph_log2_diff_max_bt_min_qt_inter_slice",
            Options().withCheckRange({0, sps->CtbLog2SizeY - sps->MinQtLog2SizeInterY}));
        this->ph_log2_diff_max_tt_min_qt_inter_slice =
            reader.readUEV("ph_log2_diff_max_tt_min_qt_inter_slice",
                           Options().withCheckRange(
                               {0, std::min(6u, sps->CtbLog2SizeY) - sps->MinQtLog2SizeInterY}));
      }
    }
    if (pps->pps_cu_qp_delta_enabled_flag)
    {
      this->ph_cu_qp_delta_subdiv_inter_slice =
          reader.readUEV("ph_cu_qp_delta_subdiv_inter_slice", Options().withCheckRange({0, 2}));
    }
    if (pps->pps_cu_chroma_qp_offset_list_enabled_flag)
    {
      this->ph_cu_chroma_qp_offset_subdiv_inter_slice = reader.readUEV(
          "ph_cu_chroma_qp_offset_subdiv_inter_slice", Options().withCheckRange({0, 2}));
    }
    if (sps->sps_temporal_mvp_enabled_flag)
    {
      this->ph_temporal_mvp_enabled_flag = reader.readFlag("ph_temporal_mvp_enabled_flag");
      if (this->ph_temporal_mvp_enabled_flag && pps->pps_rpl_info_in_ph_flag)
      {
        auto rps0 = this->ref_pic_lists_instance->getActiveRefPixList(sps, 0);
        auto rps1 = this->ref_pic_lists_instance->getActiveRefPixList(sps, 1);
        this->ph_collocated_from_l0_flag = true;
        if (rps0.num_ref_entries > 0)
        {
          this->ph_collocated_from_l0_flag = reader.readFlag("ph_collocated_from_l0_flag");
        }
        if ((this->ph_collocated_from_l0_flag && rps0.num_ref_entries > 1) ||
            (!this->ph_collocated_from_l0_flag && rps1.num_ref_entries > 1))
        {
          this->ph_collocated_ref_idx = reader.readUEV("ph_collocated_ref_idx");
        }
      }
    }
    if (sps->sps_mmvd_fullpel_only_enabled_flag)
    {
      this->ph_mmvd_fullpel_only_flag = reader.readFlag("ph_mmvd_fullpel_only_flag");
    }
    bool presenceFlag = false;
    if (!pps->pps_rpl_info_in_ph_flag)
    {
      presenceFlag = true;
    }
    else
    {
      auto rps1 = this->ref_pic_lists_instance->getActiveRefPixList(sps, 1);
      if (rps1.num_ref_entries > 0)
      {
        presenceFlag = true;
      }
    }
    if (presenceFlag)
    {
      this->ph_mvd_l1_zero_flag = reader.readFlag("ph_mvd_l1_zero_flag");
      if (sps->sps_bdof_control_present_in_ph_flag)
      {
        this->ph_bdof_disabled_flag = reader.readFlag("ph_bdof_disabled_flag");
      }
      else
      {
        this->ph_bdof_disabled_flag = !sps->sps_bdof_enabled_flag;
      }
      if (sps->sps_dmvr_control_present_in_ph_flag)
      {
        this->ph_dmvr_disabled_flag = reader.readFlag("ph_dmvr_disabled_flag");
      }
      else
      {
        this->ph_dmvr_disabled_flag = !sps->sps_dmvr_enabled_flag;
      }
    }
    if (sps->sps_prof_control_present_in_ph_flag)
    {
      this->ph_prof_disabled_flag = reader.readFlag("ph_prof_disabled_flag");
    }
    if ((pps->pps_weighted_pred_flag || pps->pps_weighted_bipred_flag) &&
        pps->pps_wp_info_in_ph_flag)
    {
      this->pred_weight_table_instance.parse(reader, sps, pps, sl, this->ref_pic_lists_instance);
    }
  }
  if (pps->pps_qp_delta_info_in_ph_flag)
  {
    this->ph_qp_delta = reader.readSEV("ph_qp_delta");
  }
  if (sps->sps_joint_cbcr_enabled_flag)
  {
    this->ph_joint_cbcr_sign_flag = reader.readFlag("ph_joint_cbcr_sign_flag");
  }
  if (sps->sps_sao_enabled_flag && pps->pps_sao_info_in_ph_flag)
  {
    this->ph_sao_luma_enabled_flag = reader.readFlag("ph_sao_luma_enabled_flag");
    if (sps->sps_chroma_format_idc != 0)
    {
      this->ph_sao_chroma_enabled_flag = reader.readFlag("ph_sao_chroma_enabled_flag");
    }
  }
  if (pps->pps_dbf_info_in_ph_flag)
  {
    this->ph_deblocking_params_present_flag = reader.readFlag("ph_deblocking_params_present_flag");
    if (this->ph_deblocking_params_present_flag)
    {
      if (!pps->pps_deblocking_filter_disabled_flag)
      {
        this->ph_deblocking_filter_disabled_flag =
            reader.readFlag("ph_deblocking_filter_disabled_flag");
      }
      if (!this->ph_deblocking_filter_disabled_flag)
      {
        this->ph_luma_beta_offset_div2 = reader.readSEV("ph_luma_beta_offset_div2");
        this->ph_luma_tc_offset_div2   = reader.readSEV("ph_luma_tc_offset_div2");
        if (pps->pps_chroma_tool_offsets_present_flag)
        {
          this->ph_cb_beta_offset_div2 = reader.readSEV("ph_cb_beta_offset_div2");
          this->ph_cb_tc_offset_div2   = reader.readSEV("ph_cb_tc_offset_div2");
          this->ph_cr_beta_offset_div2 = reader.readSEV("ph_cr_beta_offset_div2");
          this->ph_cr_tc_offset_div2   = reader.readSEV("ph_cr_tc_offset_div2");
        }
      }
    }
  }
  if (pps->pps_picture_header_extension_present_flag)
  {
    this->ph_extension_length = reader.readUEV("ph_extension_length");
    for (unsigned i = 0; i < this->ph_extension_length; i++)
    {
      this->ph_extension_data_byte.push_back(reader.readBits("ph_extension_data_byte", 8));
    }
  }
}

void picture_header_structure::calculatePictureOrderCount(
    reader::SubByteReaderLogging &            reader,
    NalType                                   nalType,
    SPSMap &                                  spsMap,
    PPSMap &                                  ppsMap,
    std::shared_ptr<picture_header_structure> previousPicture)
{
  if (ppsMap.count(this->ph_pic_parameter_set_id) == 0)
    throw std::logic_error("PPS with given ph_pic_parameter_set_id not found.");
  auto pps = ppsMap[this->ph_pic_parameter_set_id];

  if (spsMap.count(pps->pps_seq_parameter_set_id) == 0)
    throw std::logic_error("SPS with given pps_seq_parameter_set_id not found.");
  auto sps = spsMap[pps->pps_seq_parameter_set_id];

  // 8.3.1 Calculate Picture Order Count (POC)
  // If multiple layers, all layers mus have the same POC. Not implemented yet.
  auto ph_poc_msb_cycle_val_present =
      (sps->sps_poc_msb_cycle_flag && this->ph_poc_msb_cycle_present_flag);
  // This is what the reference decoder does. In the standard it sounds like a CRA or GDR nal could
  // also be a CLVSS. This is also not implemented yet.
  auto isCLVSS = nalType == NalType::IDR_N_LP || nalType == NalType::IDR_W_RADL;
  if (ph_poc_msb_cycle_val_present)
    this->PicOrderCntMsb = ph_poc_msb_cycle_val * sps->MaxPicOrderCntLsb;
  else if (isCLVSS)
    this->PicOrderCntMsb = 0;
  else
  {
    if (!previousPicture)
      throw std::logic_error("Error when calculating POC. Previous picture not found.");

    auto prevPicOrderCntLsb = previousPicture->ph_pic_order_cnt_lsb;
    auto prevPicOrderCntMsb = previousPicture->PicOrderCntMsb;

    // (196)
    if ((this->ph_pic_order_cnt_lsb < prevPicOrderCntLsb) &&
        ((prevPicOrderCntLsb - this->ph_pic_order_cnt_lsb) >= sps->MaxPicOrderCntLsb / 2))
      this->PicOrderCntMsb = prevPicOrderCntMsb + sps->MaxPicOrderCntLsb;
    else if ((this->ph_pic_order_cnt_lsb > prevPicOrderCntLsb) &&
             ((this->ph_pic_order_cnt_lsb - prevPicOrderCntLsb) > sps->MaxPicOrderCntLsb / 2))
      this->PicOrderCntMsb = prevPicOrderCntMsb - sps->MaxPicOrderCntLsb;
    else
      this->PicOrderCntMsb = prevPicOrderCntMsb;
  }

  // (197)
  this->PicOrderCntVal = this->PicOrderCntMsb + this->ph_pic_order_cnt_lsb;

  reader.logCalculatedValue("PicOrderCntMsb", int64_t(this->PicOrderCntMsb));
  reader.logCalculatedValue("PicOrderCntVal", int64_t(this->PicOrderCntVal));
}

} // namespace parser::vvc
