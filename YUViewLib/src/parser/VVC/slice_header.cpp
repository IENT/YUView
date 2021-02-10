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

#include "slice_header.h"

#include "parser/common/functions.h"
#include "pic_parameter_set_rbsp.h"
#include "seq_parameter_set_rbsp.h"

#include <cmath>

namespace parser::vvc
{

std::string to_string(SliceType sliceType)
{
  if (sliceType == SliceType::B)
    return "B";
  if (sliceType == SliceType::P)
    return "P";
  return "I";
}

using namespace parser::reader;

void slice_header::parse(SubByteReaderLogging &                    reader,
                         NalType                                   nal_unit_type,
                         VPSMap &                                  vpsMap,
                         SPSMap &                                  spsMap,
                         PPSMap &                                  ppsMap,
                         std::shared_ptr<slice_layer_rbsp>         sliceLayer,
                         std::shared_ptr<picture_header_structure> picHeader)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "slice_header");

  this->sh_picture_header_in_slice_header_flag =
      reader.readFlag("sh_picture_header_in_slice_header_flag");
  if (this->sh_picture_header_in_slice_header_flag)
  {
    this->picture_header_structure_instance = std::make_shared<picture_header_structure>();
    this->picture_header_structure_instance->parse(reader, vpsMap, spsMap, ppsMap, sliceLayer);
    picHeader = this->picture_header_structure_instance;
  }

  if (!picHeader)
    throw std::logic_error("No picture_header_structure given for parsing of slice_header.");

  if (ppsMap.count(picHeader->ph_pic_parameter_set_id) == 0)
    throw std::logic_error("PPS with given ph_pic_parameter_set_id not found.");
  auto pps = ppsMap[picHeader->ph_pic_parameter_set_id];

  if (spsMap.count(pps->pps_seq_parameter_set_id) == 0)
    throw std::logic_error("SPS with given pps_seq_parameter_set_id not found.");
  auto sps = spsMap[pps->pps_seq_parameter_set_id];

  auto CurrSubpicIdx = 0u;
  if (sps->sps_subpic_info_present_flag)
  {
    auto nrBits        = sps->sps_subpic_id_len_minus1 + 1;
    this->sh_subpic_id = reader.readBits("sh_subpic_id", nrBits);

    for (unsigned i = 0; i < pps->SubpicIdVal.size(); i++)
    {
      if (pps->SubpicIdVal.at(i) == this->sh_subpic_id)
      {
        CurrSubpicIdx = i;
        break;
      }
    }
  }

  if ((pps->pps_rect_slice_flag && pps->NumSlicesInSubpic[CurrSubpicIdx] > 1) ||
      (!pps->pps_rect_slice_flag && pps->NumTilesInPic > 1))
  {
    unsigned nrBits;
    Options  opt;
    if (!pps->pps_rect_slice_flag)
    {
      nrBits = std::ceil(std::log2(pps->NumTilesInPic));
      opt    = Options().withCheckRange({0, pps->NumTilesInPic - 1});
    }
    else
    {
      nrBits = std::ceil(std::log2(pps->NumSlicesInSubpic[CurrSubpicIdx]));
      opt    = Options().withCheckRange({0, pps->NumSlicesInSubpic[CurrSubpicIdx] - 1});
    }
    this->sh_slice_address = reader.readBits("sh_slice_address", nrBits, opt);
  }
  for (unsigned i = 0; i < sps->NumExtraShBits; i++)
  {
    this->sh_extra_bit.push_back(reader.readFlag(formatArray("sh_extra_bit", i)));
  }
  if (!pps->pps_rect_slice_flag && pps->NumTilesInPic - sh_slice_address > 1)
  {
    this->sh_num_tiles_in_slice_minus1 = reader.readUEV(
        "sh_num_tiles_in_slice_minus1", Options().withCheckRange({0, pps->NumTilesInPic}));
  }
  if (picHeader->ph_inter_slice_allowed_flag)
  {
    Options opt = Options().withCheckRange({0, 2});
    if (!picHeader->ph_intra_slice_allowed_flag)
    {
      opt = Options().withCheckRange({0, 1});
    }
    // if ((nal_unit_type == NalType::IDR_W_RADL || nal_unit_type == NalType::IDR_N_LP ||
    //      nal_unit_type == NalType::CRA_NUT) &&
    //     (vps->vps_independent_layer_flag[GeneralLayerIdx[nuh_layer_id]] || firstPicInAU))
    // {
    //   opt = Options().withCheckEqualTo(2);
    // }

    auto sliceTypeID    = reader.readUEV("sh_slice_type", opt);
    auto sliceTypeVec   = std::vector<SliceType>({SliceType::B, SliceType::P, SliceType::I});
    this->sh_slice_type = sliceTypeVec.at(sliceTypeID);
  }
  if (nal_unit_type == NalType::IDR_W_RADL || nal_unit_type == NalType::IDR_N_LP ||
      nal_unit_type == NalType::CRA_NUT || nal_unit_type == NalType::GDR_NUT)
  {
    this->sh_no_output_of_prior_pics_flag = reader.readFlag("sh_no_output_of_prior_pics_flag");
  }
  if (sps->sps_alf_enabled_flag && !pps->pps_alf_info_in_ph_flag)
  {
    this->sh_alf_enabled_flag = reader.readFlag("sh_alf_enabled_flag");
    if (this->sh_alf_enabled_flag)
    {
      this->sh_num_alf_aps_ids_luma = reader.readBits("sh_num_alf_aps_ids_luma", 3);
      for (unsigned i = 0; i < sh_num_alf_aps_ids_luma; i++)
      {
        this->sh_alf_aps_id_luma.push_back(
            reader.readBits(formatArray("sh_alf_aps_id_luma", i), 3));
      }
      if (sps->sps_chroma_format_idc != 0)
      {
        this->sh_alf_cb_enabled_flag = reader.readFlag("sh_alf_cb_enabled_flag");
        this->sh_alf_cr_enabled_flag = reader.readFlag("sh_alf_cr_enabled_flag");
      }
      if (this->sh_alf_cb_enabled_flag || sh_alf_cr_enabled_flag)
      {
        this->sh_alf_aps_id_chroma = reader.readBits("sh_alf_aps_id_chroma", 3);
      }
      if (sps->sps_ccalf_enabled_flag)
      {
        this->sh_alf_cc_cb_enabled_flag = reader.readFlag("sh_alf_cc_cb_enabled_flag");
        if (this->sh_alf_cc_cb_enabled_flag)
        {
          this->sh_alf_cc_cb_aps_id = reader.readBits("sh_alf_cc_cb_aps_id", 3);
        }
        this->sh_alf_cc_cr_enabled_flag = reader.readFlag("sh_alf_cc_cr_enabled_flag");
        if (this->sh_alf_cc_cr_enabled_flag)
        {
          this->sh_alf_cc_cr_aps_id = reader.readBits("sh_alf_cc_cr_aps_id", 3);
        }
      }
    }
  }
  if (picHeader->ph_lmcs_enabled_flag && !this->sh_picture_header_in_slice_header_flag)
  {
    this->sh_lmcs_used_flag = reader.readFlag("sh_lmcs_used_flag");
  }
  if (picHeader->ph_explicit_scaling_list_enabled_flag &&
      !this->sh_picture_header_in_slice_header_flag)
  {
    this->sh_explicit_scaling_list_used_flag =
        reader.readFlag("sh_explicit_scaling_list_used_flag");
  }

  this->ref_pic_lists_instance = std::make_shared<ref_pic_lists>();

  if (!pps->pps_rpl_info_in_ph_flag &&
      ((nal_unit_type != NalType::IDR_W_RADL && nal_unit_type != NalType::IDR_N_LP) ||
       sps->sps_idr_rpl_present_flag))
  {
    this->ref_pic_lists_instance->parse(reader, sps, pps);
  }
  if ((this->sh_slice_type != SliceType::I &&
       this->ref_pic_lists_instance->getActiveRefPixList(sps, 0).num_ref_entries > 1) ||
      (this->sh_slice_type == SliceType::B &&
       this->ref_pic_lists_instance->getActiveRefPixList(sps, 1).num_ref_entries > 1))
  {
    this->sh_num_ref_idx_active_override_flag =
        reader.readFlag("sh_num_ref_idx_active_override_flag");
    if (this->sh_num_ref_idx_active_override_flag)
    {
      for (unsigned i = 0; i < (this->sh_slice_type == SliceType::B ? 2u : 1u); i++)
      {
        if (this->ref_pic_lists_instance->getActiveRefPixList(sps, i).num_ref_entries > 1)
        {
          this->sh_num_ref_idx_active_minus1[i] = reader.readUEV(formatArray("sh_num_ref_idx_active_minus1", i));
        }
      }
    }
  }

  // (138)
  this->NumRefIdxActive = vector<unsigned>({0, 0});
  for (unsigned i = 0; i < 2; i++)
  {
    if (this->sh_slice_type == SliceType::B || (this->sh_slice_type == SliceType::P && i == 0))
    {
      if (this->sh_num_ref_idx_active_override_flag)
        this->NumRefIdxActive[i] = this->sh_num_ref_idx_active_minus1[i] + 1;
      else
      {
        if (this->ref_pic_lists_instance->getActiveRefPixList(sps, i).num_ref_entries >=
            pps->pps_num_ref_idx_default_active_minus1[i] + 1)
          this->NumRefIdxActive[i] = pps->pps_num_ref_idx_default_active_minus1[i] + 1;
        else
          this->NumRefIdxActive[i] =
              this->ref_pic_lists_instance->getActiveRefPixList(sps, i).num_ref_entries;
      }
    }
    else // this->sh_slice_type == I || (this->sh_slice_type == P && i == 1)
      this->NumRefIdxActive[i] = 0;
  }

  if (this->sh_slice_type != SliceType::I)
  {
    if (pps->pps_cabac_init_present_flag)
    {
      this->sh_cabac_init_flag = reader.readFlag("sh_cabac_init_flag");
    }
    if (picHeader->ph_temporal_mvp_enabled_flag && !pps->pps_rpl_info_in_ph_flag)
    {
      if (this->sh_slice_type == SliceType::B)
      {
        this->sh_collocated_from_l0_flag = reader.readFlag("sh_collocated_from_l0_flag");
      }
      if ((this->sh_collocated_from_l0_flag && this->NumRefIdxActive[0] > 1) ||
          (!this->sh_collocated_from_l0_flag && this->NumRefIdxActive[1] > 1))
      {
        this->sh_collocated_ref_idx = reader.readUEV(
            "sh_collocated_ref_idx", Options().withCheckRange({0, this->NumRefIdxActive[0] - 1}));
      }
    }
    if (!pps->pps_wp_info_in_ph_flag &&
        ((pps->pps_weighted_pred_flag && this->sh_slice_type == SliceType::P) ||
         (pps->pps_weighted_bipred_flag && this->sh_slice_type == SliceType::B)))
    {
      this->pred_weight_table_instance.parse(
          reader, sps, pps, sliceLayer, this->ref_pic_lists_instance);
    }
  }
  if (!pps->pps_qp_delta_info_in_ph_flag)
  {
    this->sh_qp_delta = reader.readSEV("sh_qp_delta");
  }
  if (pps->pps_slice_chroma_qp_offsets_present_flag)
  {
    this->sh_cb_qp_offset = reader.readSEV("sh_cb_qp_offset", Options().withCheckRange({-12, +12}));
    this->sh_cr_qp_offset = reader.readSEV("sh_cr_qp_offset", Options().withCheckRange({-12, +12}));
    if (sps->sps_joint_cbcr_enabled_flag)
    {
      this->sh_joint_cbcr_qp_offset =
          reader.readSEV("sh_joint_cbcr_qp_offset", Options().withCheckRange({-12, +12}));
    }
  }
  if (pps->pps_cu_chroma_qp_offset_list_enabled_flag)
  {
    this->sh_cu_chroma_qp_offset_enabled_flag =
        reader.readFlag("sh_cu_chroma_qp_offset_enabled_flag");
  }
  if (sps->sps_sao_enabled_flag && !pps->pps_sao_info_in_ph_flag)
  {
    this->sh_sao_luma_used_flag = reader.readFlag("sh_sao_luma_used_flag");
    if (sps->sps_chroma_format_idc != 0)
    {
      this->sh_sao_chroma_used_flag = reader.readFlag("sh_sao_chroma_used_flag");
    }
  }
  if (pps->pps_deblocking_filter_override_enabled_flag && !pps->pps_dbf_info_in_ph_flag)
  {
    this->sh_deblocking_params_present_flag = reader.readFlag("sh_deblocking_params_present_flag");
  }
  if (this->sh_deblocking_params_present_flag)
  {
    if (!pps->pps_deblocking_filter_disabled_flag)
    {
      this->sh_deblocking_filter_disabled_flag =
          reader.readFlag("sh_deblocking_filter_disabled_flag");
    }
    if (!this->sh_deblocking_filter_disabled_flag)
    {
      this->sh_luma_beta_offset_div2 = reader.readSEV("sh_luma_beta_offset_div2");
      this->sh_luma_tc_offset_div2   = reader.readSEV("sh_luma_tc_offset_div2");
      if (pps->pps_chroma_tool_offsets_present_flag)
      {
        this->sh_cb_beta_offset_div2 = reader.readSEV("sh_cb_beta_offset_div2");
        this->sh_cb_tc_offset_div2   = reader.readSEV("sh_cb_tc_offset_div2");
        this->sh_cr_beta_offset_div2 = reader.readSEV("sh_cr_beta_offset_div2");
        this->sh_cr_tc_offset_div2   = reader.readSEV("sh_cr_tc_offset_div2");
      }
    }
  }
  if (sps->sps_dep_quant_enabled_flag)
  {
    this->sh_dep_quant_used_flag = reader.readFlag("sh_dep_quant_used_flag");
  }
  if (sps->sps_sign_data_hiding_enabled_flag && !this->sh_dep_quant_used_flag)
  {
    this->sh_sign_data_hiding_used_flag = reader.readFlag("sh_sign_data_hiding_used_flag");
  }
  if (sps->sps_transform_skip_enabled_flag && !this->sh_dep_quant_used_flag &&
      !this->sh_sign_data_hiding_used_flag)
  {
    this->sh_ts_residual_coding_disabled_flag =
        reader.readFlag("sh_ts_residual_coding_disabled_flag");
  }
  if (pps->pps_slice_header_extension_present_flag)
  {
    this->sh_slice_header_extension_length = reader.readUEV("sh_slice_header_extension_length");
    for (unsigned i = 0; i < this->sh_slice_header_extension_length; i++)
    {
      this->sh_slice_header_extension_data_byte.push_back(
          reader.readBits(formatArray("sh_slice_header_extension_data_byte", i), 8));
    }
  }

  // (112)
  umap_1d<unsigned> CtbAddrInCurrSlice;
  unsigned          NumCtusInCurrSlice;
  if (pps->pps_rect_slice_flag)
  {
    auto picLevelSliceIdx = sh_slice_address;
    for (unsigned j = 0; j < CurrSubpicIdx; j++)
      picLevelSliceIdx += pps->NumSlicesInSubpic[j];
    NumCtusInCurrSlice = pps->NumCtusInSlice[picLevelSliceIdx];
    for (unsigned i = 0; i < NumCtusInCurrSlice; i++)
      CtbAddrInCurrSlice[i] = pps->CtbAddrInSlice[picLevelSliceIdx][i];
  }
  else
  {
    NumCtusInCurrSlice = 0;
    for (auto tileIdx = sh_slice_address;
         tileIdx <= sh_slice_address + sh_num_tiles_in_slice_minus1;
         tileIdx++)
    {
      auto tileX = tileIdx % pps->NumTileColumns;
      auto tileY = tileIdx / pps->NumTileColumns;
      for (unsigned ctbY = pps->TileRowBdVal[tileY]; ctbY < pps->TileRowBdVal[tileY + 1]; ctbY++)
      {
        for (unsigned ctbX = pps->TileColBdVal[tileX]; ctbX < pps->TileColBdVal[tileX + 1]; ctbX++)
        {
          CtbAddrInCurrSlice[NumCtusInCurrSlice] = ctbY * pps->PicWidthInCtbsY + ctbX;
          NumCtusInCurrSlice++;
        }
      }
    }
  }

  // (140)
  auto NumEntryPoints = 0u;
  if (sps->sps_entry_point_offsets_present_flag)
  {
    for (unsigned i = 1; i < NumCtusInCurrSlice; i++)
    {
      auto ctbAddrX     = CtbAddrInCurrSlice[i] % pps->PicWidthInCtbsY;
      auto ctbAddrY     = CtbAddrInCurrSlice[i] / pps->PicWidthInCtbsY;
      auto prevCtbAddrX = CtbAddrInCurrSlice[i - 1] % pps->PicWidthInCtbsY;
      auto prevCtbAddrY = CtbAddrInCurrSlice[i - 1] / pps->PicWidthInCtbsY;
      if (pps->CtbToTileRowBd[ctbAddrY] != pps->CtbToTileRowBd[prevCtbAddrY] ||
          pps->CtbToTileColBd[ctbAddrX] != pps->CtbToTileColBd[prevCtbAddrX] ||
          (ctbAddrY != prevCtbAddrY && sps->sps_entropy_coding_sync_enabled_flag))
        NumEntryPoints++;
    }
  }

  if (NumEntryPoints > 0)
  {
    this->sh_entry_offset_len_minus1 =
        reader.readUEV("sh_entry_offset_len_minus1", Options().withCheckRange({0, 31}));
    for (unsigned i = 0; i < NumEntryPoints; i++)
    {
      auto nrBits                        = sh_entry_offset_len_minus1 + 1;
      this->sh_entry_point_offset_minus1 = reader.readBits("sh_entry_point_offset_minus1", nrBits);
    }
  }
  this->byte_alignment_instance.parse(reader);
}

} // namespace parser::vvc
