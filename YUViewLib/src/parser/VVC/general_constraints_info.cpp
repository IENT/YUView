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

#include "general_constraints_info.h"

namespace parser::vvc
{

using namespace parser::reader;

void general_constraints_info::parse(SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "general_constraints_info");

  this->gci_present_flag = reader.readFlag("gci_present_flag");
  if (gci_present_flag)
  {
    this->gci_intra_only_constraint_flag = reader.readFlag("gci_intra_only_constraint_flag");
    this->gci_all_layers_independent_constraint_flag =
        reader.readFlag("gci_all_layers_independent_constraint_flag");
    this->gci_one_au_only_constraint_flag = reader.readFlag("gci_one_au_only_constraint_flag");
    this->gci_sixteen_minus_max_bitdepth_constraint_idc = reader.readBits(
        "gci_sixteen_minus_max_bitdepth_constraint_idc", 4, Options().withCheckRange({0, 8}));
    this->gci_three_minus_max_chroma_format_constraint_idc =
        reader.readBits("gci_three_minus_max_chroma_format_constraint_idc", 2);
    this->gci_no_mixed_nalu_types_in_pic_constraint_flag =
        reader.readFlag("gci_no_mixed_nalu_types_in_pic_constraint_flag");
    this->gci_no_trail_constraint_flag   = reader.readFlag("gci_no_trail_constraint_flag");
    this->gci_no_stsa_constraint_flag    = reader.readFlag("gci_no_stsa_constraint_flag");
    this->gci_no_rasl_constraint_flag    = reader.readFlag("gci_no_rasl_constraint_flag");
    this->gci_no_radl_constraint_flag    = reader.readFlag("gci_no_radl_constraint_flag");
    this->gci_no_idr_constraint_flag     = reader.readFlag("gci_no_idr_constraint_flag");
    this->gci_no_cra_constraint_flag     = reader.readFlag("gci_no_cra_constraint_flag");
    this->gci_no_gdr_constraint_flag     = reader.readFlag("gci_no_gdr_constraint_flag");
    this->gci_no_aps_constraint_flag     = reader.readFlag("gci_no_aps_constraint_flag");
    this->gci_no_idr_rpl_constraint_flag = reader.readFlag("gci_no_idr_rpl_constraint_flag");
    this->gci_one_tile_per_pic_constraint_flag =
        reader.readFlag("gci_one_tile_per_pic_constraint_flag");
    this->gci_pic_header_in_slice_header_constraint_flag =
        reader.readFlag("gci_pic_header_in_slice_header_constraint_flag");
    this->gci_one_slice_per_pic_constraint_flag =
        reader.readFlag("gci_one_slice_per_pic_constraint_flag");
    this->gci_no_rectangular_slice_constraint_flag =
        reader.readFlag("gci_no_rectangular_slice_constraint_flag");
    this->gci_one_slice_per_subpic_constraint_flag =
        reader.readFlag("gci_one_slice_per_subpic_constraint_flag");
    this->gci_no_subpic_info_constraint_flag =
        reader.readFlag("gci_no_subpic_info_constraint_flag");
    this->gci_three_minus_max_log2_ctu_size_constraint_idc =
        reader.readBits("gci_three_minus_max_log2_ctu_size_constraint_idc", 2);
    this->gci_no_partition_constraints_override_constraint_flag =
        reader.readFlag("gci_no_partition_constraints_override_constraint_flag");
    this->gci_no_mtt_constraint_flag = reader.readFlag("gci_no_mtt_constraint_flag");
    this->gci_no_qtbtt_dual_tree_intra_constraint_flag =
        reader.readFlag("gci_no_qtbtt_dual_tree_intra_constraint_flag");
    this->gci_no_palette_constraint_flag = reader.readFlag("gci_no_palette_constraint_flag");
    this->gci_no_ibc_constraint_flag     = reader.readFlag("gci_no_ibc_constraint_flag");
    this->gci_no_isp_constraint_flag     = reader.readFlag("gci_no_isp_constraint_flag");
    this->gci_no_mrl_constraint_flag     = reader.readFlag("gci_no_mrl_constraint_flag");
    this->gci_no_mip_constraint_flag     = reader.readFlag("gci_no_mip_constraint_flag");
    this->gci_no_cclm_constraint_flag    = reader.readFlag("gci_no_cclm_constraint_flag");
    this->gci_no_ref_pic_resampling_constraint_flag =
        reader.readFlag("gci_no_ref_pic_resampling_constraint_flag");
    this->gci_no_res_change_in_clvs_constraint_flag =
        reader.readFlag("gci_no_res_change_in_clvs_constraint_flag");
    this->gci_no_weighted_prediction_constraint_flag =
        reader.readFlag("gci_no_weighted_prediction_constraint_flag");
    this->gci_no_ref_wraparound_constraint_flag =
        reader.readFlag("gci_no_ref_wraparound_constraint_flag");
    this->gci_no_temporal_mvp_constraint_flag =
        reader.readFlag("gci_no_temporal_mvp_constraint_flag");
    this->gci_no_sbtmvp_constraint_flag = reader.readFlag("gci_no_sbtmvp_constraint_flag");
    this->gci_no_amvr_constraint_flag   = reader.readFlag("gci_no_amvr_constraint_flag");
    this->gci_no_bdof_constraint_flag   = reader.readFlag("gci_no_bdof_constraint_flag");
    this->gci_no_smvd_constraint_flag   = reader.readFlag("gci_no_smvd_constraint_flag");
    this->gci_no_dmvr_constraint_flag   = reader.readFlag("gci_no_dmvr_constraint_flag");
    this->gci_no_mmvd_constraint_flag   = reader.readFlag("gci_no_mmvd_constraint_flag");
    this->gci_no_affine_motion_constraint_flag =
        reader.readFlag("gci_no_affine_motion_constraint_flag");
    this->gci_no_prof_constraint_flag = reader.readFlag("gci_no_prof_constraint_flag");
    this->gci_no_bcw_constraint_flag  = reader.readFlag("gci_no_bcw_constraint_flag");
    this->gci_no_ciip_constraint_flag = reader.readFlag("gci_no_ciip_constraint_flag");
    this->gci_no_gpm_constraint_flag  = reader.readFlag("gci_no_gpm_constraint_flag");
    this->gci_no_luma_transform_size_64_constraint_flag =
        reader.readFlag("gci_no_luma_transform_size_64_constraint_flag");
    this->gci_no_transform_skip_constraint_flag =
        reader.readFlag("gci_no_transform_skip_constraint_flag");
    this->gci_no_bdpcm_constraint_flag      = reader.readFlag("gci_no_bdpcm_constraint_flag");
    this->gci_no_mts_constraint_flag        = reader.readFlag("gci_no_mts_constraint_flag");
    this->gci_no_lfnst_constraint_flag      = reader.readFlag("gci_no_lfnst_constraint_flag");
    this->gci_no_joint_cbcr_constraint_flag = reader.readFlag("gci_no_joint_cbcr_constraint_flag");
    this->gci_no_sbt_constraint_flag        = reader.readFlag("gci_no_sbt_constraint_flag");
    this->gci_no_act_constraint_flag        = reader.readFlag("gci_no_act_constraint_flag");
    this->gci_no_explicit_scaling_list_constraint_flag =
        reader.readFlag("gci_no_explicit_scaling_list_constraint_flag");
    this->gci_no_dep_quant_constraint_flag = reader.readFlag("gci_no_dep_quant_constraint_flag");
    this->gci_no_sign_data_hiding_constraint_flag =
        reader.readFlag("gci_no_sign_data_hiding_constraint_flag");
    this->gci_no_cu_qp_delta_constraint_flag =
        reader.readFlag("gci_no_cu_qp_delta_constraint_flag");
    this->gci_no_chroma_qp_offset_constraint_flag =
        reader.readFlag("gci_no_chroma_qp_offset_constraint_flag");
    this->gci_no_sao_constraint_flag   = reader.readFlag("gci_no_sao_constraint_flag");
    this->gci_no_alf_constraint_flag   = reader.readFlag("gci_no_alf_constraint_flag");
    this->gci_no_ccalf_constraint_flag = reader.readFlag("gci_no_ccalf_constraint_flag");
    this->gci_no_lmcs_constraint_flag  = reader.readFlag("gci_no_lmcs_constraint_flag");
    this->gci_no_ladf_constraint_flag  = reader.readFlag("gci_no_ladf_constraint_flag");
    this->gci_no_virtual_boundaries_constraint_flag =
        reader.readFlag("gci_no_virtual_boundaries_constraint_flag");
    this->gci_num_reserved_bits =
        reader.readBits("gci_num_reserved_bits", 8, Options().withCheckEqualTo(0));
    for (unsigned i = 0; i < this->gci_num_reserved_bits; i++)
    {
      this->gci_reserved_zero_bit.push_back(reader.readFlag("gci_reserved_zero_bit"));
    }
  }
  while (!reader.byte_aligned())
  {
    this->gci_alignment_zero_bit = reader.readFlag("gci_alignment_zero_bit");
  }
}

} // namespace parser::vvc
