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

#include "ProfileTierLevel.h"

#include "parser/common/parserMacros.h"

namespace VVC
{

bool GeneralConstraintInfo::parse(ReaderHelper &reader)
{
  ReaderSubLevel s(reader, "general_constraint_info()");

  READFLAG(this->general_non_packed_constraint_flag);
  READFLAG(this->general_frame_only_constraint_flag);
  READFLAG(this->general_non_projected_constraint_flag);
  READFLAG(this->general_one_picture_only_constraint_flag);
  READFLAG(this->intra_only_constraint_flag);
  READBITS(this->max_bitdepth_constraint_idc, 4);
  READBITS(this->max_chroma_format_constraint_idc, 2);
  READFLAG(this->single_layer_constraint_flag);
  READFLAG(this->all_layers_independent_constraint_flag);
  READFLAG(this->no_ref_pic_resampling_constraint_flag);
  READFLAG(this->no_res_change_in_clvs_constraint_flag);
  READFLAG(this->one_tile_per_pic_constraint_flag);
  READFLAG(this->pic_header_in_slice_header_constraint_flag);
  READFLAG(this->one_slice_per_pic_constraint_flag);
  READFLAG(this->one_subpic_per_pic_constraint_flag);
  READFLAG(this->no_qtbtt_dual_tree_intra_constraint_flag);
  READFLAG(this->no_partition_constraints_override_constraint_flag);
  READFLAG(this->no_sao_constraint_flag);
  READFLAG(this->no_alf_constraint_flag);
  READFLAG(this->no_ccalf_constraint_flag);
  READFLAG(this->no_joint_cbcr_constraint_flag);
  READFLAG(this->no_mrl_constraint_flag);
  READFLAG(this->no_isp_constraint_flag);
  READFLAG(this->no_mip_constraint_flag);
  READFLAG(this->no_ref_wraparound_constraint_flag);
  READFLAG(this->no_temporal_mvp_constraint_flag);
  READFLAG(this->no_sbtmvp_constraint_flag );
  READFLAG(this->no_amvr_constraint_flag);
  READFLAG(this->no_bdof_constraint_flag);
  READFLAG(this->no_dmvr_constraint_flag);
  READFLAG(this->no_cclm_constraint_flag);
  READFLAG(this->no_mts_constraint_flag);
  READFLAG(this->no_sbt_constraint_flag);
  READFLAG(this->no_lfnst_constraint_flag);
  READFLAG(this->no_affine_motion_constraint_flag);
  READFLAG(this->no_mmvd_constraint_flag);
  READFLAG(this->no_smvd_constraint_flag);
  READFLAG(this->no_prof_constraint_flag);
  READFLAG(this->no_bcw_constraint_flag);
  READFLAG(this->no_ibc_constraint_flag);
  READFLAG(this->no_ciip_constraint_flag);
  READFLAG(this->no_gpm_constraint_flag);
  READFLAG(this->no_ladf_constraint_flag);
  READFLAG(this->no_transform_skip_constraint_flag);
  READFLAG(this->no_bdpcm_constraint_flag);
  READFLAG(this->no_palette_constraint_flag);
  READFLAG(this->no_act_constraint_flag);
  READFLAG(this->no_lmcs_constraint_flag);
  READFLAG(this->no_cu_qp_delta_constraint_flag);
  READFLAG(this->no_chroma_qp_offset_constraint_flag);
  READFLAG(this->no_dep_quant_constraint_flag);
  READFLAG(this->no_sign_data_hiding_constraint_flag);
  READFLAG(this->no_tsrc_constraint_flag);
  READFLAG(this->no_mixed_nalu_types_in_pic_constraint_flag);
  READFLAG(this->no_trail_constraint_flag);
  READFLAG(this->no_stsa_constraint_flag);
  READFLAG(this->no_rasl_constraint_flag);
  READFLAG(this->no_radl_constraint_flag);
  READFLAG(this->no_idr_constraint_flag);
  READFLAG(this->no_cra_constraint_flag);
  READFLAG(this->no_gdr_constraint_flag);
  READFLAG(this->no_aps_constraint_flag);

  while (!reader.isByteAligned())
  {
    bool gci_alignment_zero_bit;
    READFLAG(gci_alignment_zero_bit);
    if (gci_alignment_zero_bit)
      return reader.addErrorMessageChildItem("gci_alignment_zero_bit should be zero");
  }
  READBITS(gci_num_reserved_bytes, 8);
  for (unsigned i = 0; i < gci_num_reserved_bytes; i++)
  {
    READBITS_A(gci_reserved_byte, 8, i);
  }

  return true;
}

bool ProfileTierLevel::parse(ReaderHelper &reader, bool profilePresentFlag, int maxNumSubLayersMinus1)
{
  ReaderSubLevel s(reader, "profile_tier_level()");

  if (profilePresentFlag)
  {
    READBITS(this->general_profile_idc, 7);
    READFLAG(this->general_tier_flag);
    general_constraint_info.parse(reader);
  }

  READBITS(general_level_idc, 8);
  if (profilePresentFlag)
  {
    READBITS(ptl_num_sub_profiles, 8);
    for (unsigned i = 0; i < ptl_num_sub_profiles; i++)
    {
      READBITS_A(general_sub_profile_idc, 32, i);
    }
  }
  for (int i = 0; i < maxNumSubLayersMinus1; i++)
  {
    READFLAG_A(ptl_sublayer_level_present_flag, i);
  }
  while (!reader.isByteAligned())
  {
    bool ptl_alignment_zero_bit;
    READFLAG(ptl_alignment_zero_bit);
    if (ptl_alignment_zero_bit)
      return reader.addErrorMessageChildItem("ptl_alignment_zero_bit should be zero");
  }
  for (int i = 0; i < maxNumSubLayersMinus1; i++)
  {
    if (ptl_sublayer_level_present_flag[i])
    {
      READBITS_A(sublayer_level_idc, 8, i);
    }
  }

  return true;
}

} // namespace VVC