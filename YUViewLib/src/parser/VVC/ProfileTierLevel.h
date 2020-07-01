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

#include "parser/common/ReaderHelper.h"

namespace VVC
{

struct GeneralConstraintInfo
{
  bool parse(ReaderHelper &reader);

  bool general_non_packed_constraint_flag;
  bool general_frame_only_constraint_flag;
  bool general_non_projected_constraint_flag;
  bool general_one_picture_only_constraint_flag;
  bool intra_only_constraint_flag;
  unsigned int max_bitdepth_constraint_idc;
  unsigned int max_chroma_format_constraint_idc;
  bool single_layer_constraint_flag;
  bool all_layers_independent_constraint_flag;
  bool no_ref_pic_resampling_constraint_flag;
  bool no_res_change_in_clvs_constraint_flag;
  bool one_tile_per_pic_constraint_flag;
  bool pic_header_in_slice_header_constraint_flag;
  bool one_slice_per_pic_constraint_flag;
  bool one_subpic_per_pic_constraint_flag;
  bool no_qtbtt_dual_tree_intra_constraint_flag;
  bool no_partition_constraints_override_constraint_flag;
  bool no_sao_constraint_flag;
  bool no_alf_constraint_flag;
  bool no_ccalf_constraint_flag;
  bool no_joint_cbcr_constraint_flag;
  bool no_mrl_constraint_flag;
  bool no_isp_constraint_flag;
  bool no_mip_constraint_flag;
  bool no_ref_wraparound_constraint_flag;
  bool no_temporal_mvp_constraint_flag;
  bool no_sbtmvp_constraint_flag ;
  bool no_amvr_constraint_flag;
  bool no_bdof_constraint_flag;
  bool no_dmvr_constraint_flag;
  bool no_cclm_constraint_flag;
  bool no_mts_constraint_flag;
  bool no_sbt_constraint_flag;
  bool no_lfnst_constraint_flag;
  bool no_affine_motion_constraint_flag;
  bool no_mmvd_constraint_flag;
  bool no_smvd_constraint_flag;
  bool no_prof_constraint_flag;
  bool no_bcw_constraint_flag;
  bool no_ibc_constraint_flag;
  bool no_ciip_constraint_flag;
  bool no_gpm_constraint_flag;
  bool no_ladf_constraint_flag;
  bool no_transform_skip_constraint_flag;
  bool no_bdpcm_constraint_flag;
  bool no_palette_constraint_flag;
  bool no_act_constraint_flag;
  bool no_lmcs_constraint_flag;
  bool no_cu_qp_delta_constraint_flag;
  bool no_chroma_qp_offset_constraint_flag;
  bool no_dep_quant_constraint_flag;
  bool no_sign_data_hiding_constraint_flag;
  bool no_tsrc_constraint_flag;
  bool no_mixed_nalu_types_in_pic_constraint_flag;
  bool no_trail_constraint_flag;
  bool no_stsa_constraint_flag;
  bool no_rasl_constraint_flag;
  bool no_radl_constraint_flag;
  bool no_idr_constraint_flag;
  bool no_cra_constraint_flag;
  bool no_gdr_constraint_flag;
  bool no_aps_constraint_flag;
};

struct ProfileTierLevel
{
  bool parse(ReaderHelper &reader, bool profilePresentFlag, int maxNumSubLayersMinus1);

  unsigned int general_profile_idc;
  bool general_tier_flag;
  GeneralConstraintInfo general_constraint_info ;

};

}