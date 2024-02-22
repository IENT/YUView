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

#include "vps_extension.h"

#include <parser/common/Functions.h>

namespace parser::hevc
{

using namespace reader;

void vps_extension::parse(SubByteReaderLogging &reader,
                          const unsigned        vps_max_layers_minus1,
                          const bool            vps_base_layer_internal_flag,
                          const unsigned        vps_max_sub_layers_minus1,
                          const unsigned        vps_num_layer_sets_minus1)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "vps_extension()");

  if (vps_max_layers_minus1 > 0 && vps_base_layer_internal_flag)
    profileTierLevel.parse(reader, false, vps_max_sub_layers_minus1);
  this->splitting_flag = reader.readFlag("splitting_flag");

  unsigned NumScalabilityTypes = 0;
  for (unsigned i = 0; i < 16; i++)
  {
    this->scalability_mask_flag.push_back(reader.readFlag(formatArray("scalability_mask_flag", i)));
    if (this->scalability_mask_flag.at(i))
      NumScalabilityTypes++;
  }

  for (unsigned j = 0; j < (NumScalabilityTypes - (this->splitting_flag) ? 1u : 0u); j++)
    this->dimension_id_len_minus1.push_back(
        reader.readBits(formatArray("dimension_id_len_minus1", j), 3));

  this->vps_nuh_layer_id_present_flag = reader.readFlag("vps_nuh_layer_id_present_flag");
  this->MaxLayersMinus1               = std::min(62u, vps_max_layers_minus1);
  reader.logCalculatedValue("MaxLayersMinus1", this->MaxLayersMinus1);

  for (unsigned i = 1; i <= this->MaxLayersMinus1; i++)
  {
    if (this->vps_nuh_layer_id_present_flag)
      this->layer_id_in_nuh.push_back(reader.readBits(formatArray("layer_id_in_nuh", i), 6));
    else
      this->layer_id_in_nuh.push_back(i);
    this->LayerIdxInVps[this->layer_id_in_nuh[i]] = i;

    if (!this->splitting_flag)
    {
      this->dimension_id.push_back({});
      for (unsigned j = 0; j < NumScalabilityTypes; j++)
      {
        const auto nrBits = this->dimension_id_len_minus1[j] + 1;
        this->dimension_id[i].push_back(reader.readBits(formatArray("dimension_id", i, j), nrBits));
      }
    }
  }

  this->calcualteValuesF3();

  this->view_id_len = reader.readBits("view_id_len", 4);
  if (this->view_id_len > 0)
  {
    for (unsigned i = 0; i < NumViews; i++)
      this->view_id_val.push_back(
          reader.readBits(formatArray("view_id_val", i), this->view_id_len));
  }

  for (unsigned i = 1; i <= MaxLayersMinus1; i++)
    for (unsigned j = 0; j < i; j++)
      this->direct_dependency_flag[i][j] =
          reader.readFlag(formatArray("direct_dependency_flag", i, j));
  this->calcualteValuesF4();
  this->calcualteValuesF5();
  this->calcualteValuesF6();

  if (this->NumIndependentLayers > 1)
    this->num_add_layer_sets = reader.readUEV("num_add_layer_sets");

  for (unsigned i = 0; i < this->num_add_layer_sets; i++)
  {
    for (unsigned j = 1; j < this->NumIndependentLayers; j++)
    {
      const auto nrBits = std::ceil(std::log2(this->NumLayersInTreePartition[j] + 1));
      this->highest_layer_idx_plus1[i][j] =
          reader.readBits(formatArray("highest_layer_idx_plus1", i, j), nrBits);
    }
    this->calculateValuesF9(vps_num_layer_sets_minus1, i);
  }

  this->vps_sub_layers_max_minus1_present_flag =
      reader.readFlag("vps_sub_layers_max_minus1_present_flag");
  if (this->vps_sub_layers_max_minus1_present_flag)
    for (unsigned i = 0; i <= this->MaxLayersMinus1; i++)
      sub_layers_vps_max_minus1.push_back(
          reader.readBits(formatArray("sub_layers_vps_max_minus1", i), 3));
  this->calcualteValuesF10();

  this->max_tid_ref_present_flag = reader.readFlag("max_tid_ref_present_flag");
  if (this->max_tid_ref_present_flag)
  {
    for (unsigned i = 0; i < this->MaxLayersMinus1; i++)
      for (unsigned j = i + 1; j <= this->MaxLayersMinus1; j++)
        if (this->direct_dependency_flag[j][i])
          this->max_tid_il_ref_pics_plus1[i][j] =
              reader.readBits(formatArray("max_tid_il_ref_pics_plus1", i, j), 3);
  }

  this->default_ref_layers_active_flag    = reader.readFlag("default_ref_layers_active_flag");
  this->vps_num_profile_tier_level_minus1 = reader.readUEV("vps_num_profile_tier_level_minus1");
  for (unsigned i = vps_base_layer_internal_flag ? 2 : 1;
       i <= this->vps_num_profile_tier_level_minus1;
       i++)
  {
    this->vps_profile_present_flag[i] = reader.readFlag(formatArray("vps_profile_present_flag", i));
    profileTierLevelSubLayers[i].parse(
        reader, this->vps_profile_present_flag[i], vps_max_sub_layers_minus1);
  }

  this->NumLayerSets = vps_num_layer_sets_minus1 + 1 + num_add_layer_sets;
  if (this->NumLayerSets > 1)
  {
    this->num_add_olss             = reader.readUEV("num_add_olss");
    this->default_output_layer_idc = reader.readBits("default_output_layer_idc", 2);
  }
  this->defaultOutputLayerIdc = std::min(this->default_output_layer_idc, 2u);

  this->NumOutputLayerSets = this->num_add_olss + this->NumLayerSets;
  for (unsigned i = 1; i < this->NumOutputLayerSets; i++)
  {
    if (this->NumLayerSets > 2 && i >= this->NumLayerSets)
    {
      const auto nrBits = std::ceil(std::log2(this->NumLayerSets - 1));
      this->layer_set_idx_for_ols_minus1[i] =
          reader.readBits(formatArray("layer_set_idx_for_ols_minus1", i), nrBits);
    }
    this->calculateValuesF11(i);

    if (i > vps_num_layer_sets_minus1 || this->defaultOutputLayerIdc == 2)
    {
      for (unsigned j = 0; j < this->NumLayersInIdList[this->OlsIdxToLsIdx[i]]; j++)
      {
        this->output_layer_flag[i][j] = reader.readFlag(formatArray("output_layer_flag", i, j));
        this->OutputLayerFlag[i][j]   = this->output_layer_flag[i][j];
      }
    }
    else
    {
      for (unsigned j = 0; j < NumLayersInIdList[OlsIdxToLsIdx[i]] - 1; j++)
      {
        const auto layerIdAList = this->LayerSetLayerIdList[this->OlsIdxToLsIdx[i]];
        const auto nuhLayerIdA  = std::max_element(layerIdAList.begin(), layerIdAList.end());
        if (this->defaultOutputLayerIdc == 0 || layerIdAList.at(j) == nuhLayerIdA->second)
          this->OutputLayerFlag[i][j] = true;
        else
          this->OutputLayerFlag[i][j] = false;
      }
    }

    this->calcualteValuesF12(i);
    this->calculateValuesF13(i);

    for (unsigned j = 0; j < this->NumLayersInIdList[this->OlsIdxToLsIdx[i]]; j++)
    {
      if (this->NecessaryLayerFlag[i][j] && this->vps_num_profile_tier_level_minus1 > 0)
      {
        const auto nrBits = std::ceil(std::log2(this->vps_num_profile_tier_level_minus1 + 1));
        this->profile_tier_level_idx[i][j] =
            reader.readBits(formatArray("profile_tier_level_idx", i, j), nrBits);
      }
    }

    if (this->NumOutputLayersInOutputLayerSet[i] == 1 &&
        this->NumDirectRefLayers[OlsHighestOutputLayerId[i]] > 0)
      this->alt_output_layer_flag[i] = reader.readFlag(formatArray("alt_output_layer_flag", i));
  }

  this->vps_num_rep_formats_minus1 = reader.readUEV("vps_num_rep_formats_minus1");
  for (unsigned i = 0; i <= this->vps_num_rep_formats_minus1; i++)
  {
    rep_format repFormat;
    repFormat.parse(reader);
    repFormats.push_back(repFormat);
  }

  if (this->vps_num_rep_formats_minus1 > 0)
    this->rep_format_idx_present_flag = reader.readFlag("rep_format_idx_present_flag");

  if (this->rep_format_idx_present_flag)
  {
    for (unsigned i = vps_base_layer_internal_flag ? 1 : 0; i <= MaxLayersMinus1; i++)
    {
      const auto nrBits = std::ceil(std::log2(this->vps_num_rep_formats_minus1 + 1));
      this->vps_rep_format_idx.push_back(
          reader.readBits(formatArray("vps_rep_format_idx", i), nrBits));
    }
  }

  this->max_one_active_ref_layer_flag = reader.readFlag("max_one_active_ref_layer_flag");
  this->vps_poc_lsb_aligned_flag      = reader.readFlag("vps_poc_lsb_aligned_flag");
  for (unsigned i = 1; i <= this->MaxLayersMinus1; i++)
    if (this->NumDirectRefLayers[this->layer_id_in_nuh[i]] == 0)
      this->poc_lsb_not_present_flag[i] =
          reader.readFlag(formatArray("poc_lsb_not_present_flag", i));

  this->dpbSize.parse(reader,
                      this->NumOutputLayerSets,
                      this->OlsIdxToLsIdx,
                      this->MaxSubLayersInLayerSetMinus1,
                      this->NumLayersInIdList,
                      this->NecessaryLayerFlag,
                      vps_base_layer_internal_flag,
                      this->LayerSetLayerIdList);

  this->direct_dep_type_len_minus2        = reader.readUEV("direct_dep_type_len_minus2");
  this->direct_dependency_all_layers_flag = reader.readFlag("direct_dependency_all_layers_flag");
  if (this->direct_dependency_all_layers_flag)
  {
    const auto nrBits = this->direct_dep_type_len_minus2 + 2;
    this->direct_dependency_all_layers_type =
        reader.readBits("direct_dependency_all_layers_type", nrBits);
  }
  else
  {
    for (unsigned i = vps_base_layer_internal_flag ? 1 : 2; i <= MaxLayersMinus1; i++)
      for (unsigned j = vps_base_layer_internal_flag ? 0 : 1; j < i; j++)
        if (this->direct_dependency_flag[i][j])
        {
          const auto nrBits = this->direct_dep_type_len_minus2 + 2;
          direct_dependency_type[i][j] =
              reader.readBits(formatArray("direct_dependency_type", i, j), nrBits);
        }
  }

  this->vps_non_vui_extension_length = reader.readUEV("vps_non_vui_extension_length");
  for (unsigned i = 1; i <= this->vps_non_vui_extension_length; i++)
    this->vps_non_vui_extension_data_byte = reader.readBits("vps_non_vui_extension_data_byte", 8);
  this->vps_vui_present_flag = reader.readFlag("vps_vui_present_flag");
  if (this->vps_vui_present_flag)
  {
    while (!reader.byte_aligned())
      reader.readFlag("vps_vui_alignment_bit_equal_to_one", Options().withCheckEqualTo(0));
    vpsVui.parse(reader, this->vps_vui_present_flag);
  }
}

void vps_extension::calcualteValuesF3()
{
  // F.7.4.3.1.1 (F-3)
  this->NumViews = 1;
  for (unsigned i = 0; i <= this->MaxLayersMinus1; i++)
  {
    const auto lId = this->layer_id_in_nuh[i];
    ScalabilityId.emplace_back();
    unsigned j = 0;
    for (unsigned smIdx = 0; smIdx < 16; smIdx++)
    {
      if (this->scalability_mask_flag[smIdx])
        this->ScalabilityId[i].push_back(this->dimension_id[i][j++]);
      else
        this->ScalabilityId[i].push_back(0);
    }
    this->DepthLayerFlag[lId] = this->ScalabilityId[i][0];
    this->ViewOrderIdx[lId]   = this->ScalabilityId[i][1];
    this->DependencyId[lId]   = this->ScalabilityId[i][2];
    this->AuxId[lId]          = this->ScalabilityId[i][3];

    if (i > 0)
    {

      bool newViewFlag = true;
      for (unsigned j = 0; j < i; j++)
        if (this->ViewOrderIdx[lId] == this->ViewOrderIdx[layer_id_in_nuh[j]])
          newViewFlag = false;
      if (newViewFlag)
        this->NumViews++;
    }
  }
}

void vps_extension::calcualteValuesF4()
{
  for (unsigned i = 0; i <= MaxLayersMinus1; i++)
    for (unsigned j = 0; j <= MaxLayersMinus1; j++)
    {
      this->DependencyFlag[i][j] = this->direct_dependency_flag[i][j];
      for (unsigned k = 0; k < i; k++)
        if (this->direct_dependency_flag[i][k] && this->DependencyFlag[k][j])
          this->DependencyFlag[i][j] = true;
    }
}

void vps_extension::calcualteValuesF5()
{
  for (unsigned i = 0; i <= this->MaxLayersMinus1; i++)
  {
    const auto iNuhLId = this->layer_id_in_nuh[i];
    unsigned   d       = 0;
    unsigned   r       = 0;
    unsigned   p       = 0;
    for (unsigned j = 0; j <= this->MaxLayersMinus1; j++)
    {
      const auto jNuhLid = this->layer_id_in_nuh[j];
      if (this->direct_dependency_flag[i][j])
        this->IdDirectRefLayer[iNuhLId][d++] = jNuhLid;
      if (this->DependencyFlag[i][j])
        this->IdRefLayer[iNuhLId][r++] = jNuhLid;
      if (this->DependencyFlag[j][i])
        this->IdPredictedLayer[iNuhLId][p++] = jNuhLid;
    }
    this->NumDirectRefLayers[iNuhLId] = d;
    this->NumRefLayers[iNuhLId]       = r;
    this->NumPredictedLayers[iNuhLId] = p;
  }
}

void vps_extension::calcualteValuesF6()
{
  for (unsigned i = 0; i <= 63; i++)
    this->layerIdInListFlag[i] = 0;

  unsigned k = 0;
  for (unsigned i = 0; i <= this->MaxLayersMinus1; i++)
  {
    const auto iNuhLId = this->layer_id_in_nuh[i];
    if (this->NumDirectRefLayers[iNuhLId] == 0)
    {
      this->TreePartitionLayerIdList[k][0] = iNuhLId;
      unsigned h                           = 1;
      for (unsigned j = 0; j < this->NumPredictedLayers[iNuhLId]; j++)
      {
        const auto predLId = this->IdPredictedLayer[iNuhLId][j];
        if (!this->layerIdInListFlag[predLId])
        {
          this->TreePartitionLayerIdList[k][h++] = predLId;
          this->layerIdInListFlag[predLId]       = 1;
        }
      }
      this->NumLayersInTreePartition.push_back(h);
      k++;
    }
  }
  this->NumIndependentLayers = k;
}

void vps_extension::calculateValuesF9(const unsigned vps_num_layer_sets_minus1, const unsigned i)
{
  auto       layerNum = 0u;
  const auto lsIdx    = vps_num_layer_sets_minus1 + 1 + i;
  for (unsigned treeIdx = 1; treeIdx < this->NumIndependentLayers; treeIdx++)
    for (unsigned layerCnt = 0; layerCnt < this->highest_layer_idx_plus1[i][treeIdx]; layerCnt++)
      this->LayerSetLayerIdList[lsIdx][layerNum++] =
          this->TreePartitionLayerIdList[treeIdx][layerCnt];
  this->NumLayersInIdList[lsIdx] = layerNum;
}

void vps_extension::calcualteValuesF10()
{
  for (unsigned i = 0; i < this->NumLayerSets; i++)
  {
    auto maxSlMinus1 = 0u;
    for (unsigned k = 0; k < this->NumLayersInIdList[i]; k++)
    {
      auto lId    = this->LayerSetLayerIdList[i][k];
      maxSlMinus1 = std::max(maxSlMinus1, this->sub_layers_vps_max_minus1[LayerIdxInVps[lId]]);
    }
    MaxSubLayersInLayerSetMinus1.push_back(maxSlMinus1);
  }
}

void vps_extension::calculateValuesF11(const unsigned i)
{
  this->OlsIdxToLsIdx[i] =
      (i < this->NumLayerSets) ? i : (this->layer_set_idx_for_ols_minus1[i] + 1);
}

void vps_extension::calcualteValuesF12(const unsigned i)
{
  this->NumOutputLayersInOutputLayerSet[i] = 0;
  for (unsigned j = 0; j < this->NumLayersInIdList[OlsIdxToLsIdx[i]]; j++)
  {
    this->NumOutputLayersInOutputLayerSet[i] += this->OutputLayerFlag[i][j];
    if (this->OutputLayerFlag[i][j])
      this->OlsHighestOutputLayerId[i] = this->LayerSetLayerIdList[OlsIdxToLsIdx[i]][j];
  }
}

void vps_extension::calculateValuesF13(const unsigned olsIdx)
{
  const auto lsIdx = OlsIdxToLsIdx[olsIdx];
  for (unsigned lsLayerIdx = 0; lsLayerIdx < this->NumLayersInIdList[lsIdx]; lsLayerIdx++)
    this->NecessaryLayerFlag[olsIdx][lsLayerIdx] = false;
  for (unsigned lsLayerIdx = 0; lsLayerIdx < this->NumLayersInIdList[lsIdx]; lsLayerIdx++)
    if (this->OutputLayerFlag[olsIdx][lsLayerIdx])
    {
      this->NecessaryLayerFlag[olsIdx][lsLayerIdx] = true;
      const auto currLayerId                       = this->LayerSetLayerIdList[lsIdx][lsLayerIdx];
      for (unsigned rLsLayerIdx = 0; rLsLayerIdx < lsLayerIdx; rLsLayerIdx++)
      {
        const auto refLayerId = this->LayerSetLayerIdList[lsIdx][rLsLayerIdx];
        if (this->DependencyFlag[LayerIdxInVps[currLayerId]][LayerIdxInVps[refLayerId]])
          this->NecessaryLayerFlag[olsIdx][rLsLayerIdx] = 1;
      }
    }
  this->NumNecessaryLayers[olsIdx] = 0;
  for (unsigned lsLayerIdx = 0; lsLayerIdx < this->NumLayersInIdList[lsIdx]; lsLayerIdx++)
    this->NumNecessaryLayers[olsIdx] += this->NecessaryLayerFlag[olsIdx][lsLayerIdx];
}

} // namespace parser::hevc
