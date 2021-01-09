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
#include "commonMaps.h"
#include "parser/common/SubByteReaderLogging.h"
#include "pred_weight_table.h"
#include "ref_pic_lists.h"

#include <memory>

namespace parser::vvc
{

class seq_parameter_set_rbsp;
class pic_parameter_set_rbsp;
class slice_layer_rbsp;

class picture_header_structure : public NalRBSP
{
public:
  picture_header_structure()  = default;
  ~picture_header_structure() = default;
  void parse(reader::SubByteReaderLogging &    reader,
             VPSMap &                          vpsMap,
             SPSMap &                          spsMap,
             PPSMap &                          ppsMap,
             std::shared_ptr<slice_layer_rbsp> sl);

  void calculatePictureOrderCount(reader::SubByteReaderLogging &            reader,
                                  NalType                                   nalType,
                                  SPSMap &                                  spsMap,
                                  PPSMap &                                  ppsMap,
                                  std::shared_ptr<picture_header_structure> previousPicture);

  bool                           ph_gdr_or_irap_pic_flag{};
  bool                           ph_non_ref_pic_flag{};
  bool                           ph_gdr_pic_flag{};
  bool                           ph_inter_slice_allowed_flag{};
  bool                           ph_intra_slice_allowed_flag{true};
  unsigned                       ph_pic_parameter_set_id{};
  unsigned                       ph_pic_order_cnt_lsb{};
  unsigned                       ph_recovery_poc_cnt{};
  vector<bool>                   ph_extra_bit{};
  bool                           ph_poc_msb_cycle_present_flag{};
  unsigned                       ph_poc_msb_cycle_val{};
  bool                           ph_alf_enabled_flag{};
  unsigned                       ph_num_alf_aps_ids_luma{};
  vector<unsigned>               ph_alf_aps_id_luma{};
  bool                           ph_alf_cb_enabled_flag{};
  bool                           ph_alf_cr_enabled_flag{};
  unsigned                       ph_alf_aps_id_chroma{};
  bool                           ph_alf_cc_cb_enabled_flag{};
  unsigned                       ph_alf_cc_cb_aps_id{};
  bool                           ph_alf_cc_cr_enabled_flag{};
  unsigned                       ph_alf_cc_cr_aps_id{};
  bool                           ph_lmcs_enabled_flag{};
  unsigned                       ph_lmcs_aps_id{};
  bool                           ph_chroma_residual_scale_flag{};
  bool                           ph_explicit_scaling_list_enabled_flag{};
  unsigned                       ph_scaling_list_aps_id{};
  bool                           ph_virtual_boundaries_present_flag{};
  unsigned                       ph_num_ver_virtual_boundaries{};
  vector<unsigned>               ph_virtual_boundary_pos_x_minus1{};
  unsigned                       ph_num_hor_virtual_boundaries{};
  vector<unsigned>               ph_virtual_boundary_pos_y_minus1{};
  bool                           ph_pic_output_flag{true};
  std::shared_ptr<ref_pic_lists> ref_pic_lists_instance;
  bool                           ph_partition_constraints_override_flag{};
  unsigned                       ph_log2_diff_min_qt_min_cb_intra_slice_luma{};
  unsigned                       ph_max_mtt_hierarchy_depth_intra_slice_luma{};
  unsigned                       ph_log2_diff_max_bt_min_qt_intra_slice_luma{};
  unsigned                       ph_log2_diff_max_tt_min_qt_intra_slice_luma{};
  unsigned                       ph_log2_diff_min_qt_min_cb_intra_slice_chroma{};
  unsigned                       ph_max_mtt_hierarchy_depth_intra_slice_chroma{};
  unsigned                       ph_log2_diff_max_bt_min_qt_intra_slice_chroma{};
  unsigned                       ph_log2_diff_max_tt_min_qt_intra_slice_chroma{};
  unsigned                       ph_cu_qp_delta_subdiv_intra_slice{};
  unsigned                       ph_cu_chroma_qp_offset_subdiv_intra_slice{};
  unsigned                       ph_log2_diff_min_qt_min_cb_inter_slice{};
  unsigned                       ph_max_mtt_hierarchy_depth_inter_slice{};
  unsigned                       ph_log2_diff_max_bt_min_qt_inter_slice{};
  unsigned                       ph_log2_diff_max_tt_min_qt_inter_slice{};
  unsigned                       ph_cu_qp_delta_subdiv_inter_slice{};
  unsigned                       ph_cu_chroma_qp_offset_subdiv_inter_slice{};
  bool                           ph_temporal_mvp_enabled_flag{};
  bool                           ph_collocated_from_l0_flag{};
  unsigned                       ph_collocated_ref_idx{};
  bool                           ph_mmvd_fullpel_only_flag{};
  bool                           ph_mvd_l1_zero_flag{};
  bool                           ph_bdof_disabled_flag{};
  bool                           ph_dmvr_disabled_flag{};
  bool                           ph_prof_disabled_flag{};
  pred_weight_table              pred_weight_table_instance;
  int                            ph_qp_delta{};
  bool                           ph_joint_cbcr_sign_flag{};
  bool                           ph_sao_luma_enabled_flag{};
  bool                           ph_sao_chroma_enabled_flag{};
  bool                           ph_deblocking_params_present_flag{};
  bool                           ph_deblocking_filter_disabled_flag{};
  int                            ph_luma_beta_offset_div2{};
  int                            ph_luma_tc_offset_div2{};
  int                            ph_cb_beta_offset_div2{};
  int                            ph_cb_tc_offset_div2{};
  int                            ph_cr_beta_offset_div2{};
  int                            ph_cr_tc_offset_div2{};
  unsigned                       ph_extension_length{};
  vector<unsigned>               ph_extension_data_byte{};

  unsigned PicOrderCntMsb{};
  unsigned PicOrderCntVal{};
};

} // namespace parser::vvc
