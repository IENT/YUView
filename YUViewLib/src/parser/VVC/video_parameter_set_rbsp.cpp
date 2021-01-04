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

#include "video_parameter_set_rbsp.h"

namespace parser::vvc
{

using namespace parser::reader;

void video_parameter_set_rbsp::parse(SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "video_parameter_set_rbsp");

  this->vps_video_parameter_set_id =
      reader.readBits("vps_video_parameter_set_id", 4, Options().withCheckGreater(0));
  this->vps_max_layers_minus1 = reader.readBits("vps_max_layers_minus1", 6);
  this->vps_max_sublayers_minus1 =
      reader.readBits("vps_max_sublayers_minus1", 3, Options().withCheckRange({0, 6}));
  if (this->vps_max_layers_minus1 > 0 && vps_max_sublayers_minus1 > 0)
  {
    this->vps_default_ptl_dpb_hrd_max_tid_flag =
        reader.readFlag("vps_default_ptl_dpb_hrd_max_tid_flag");
  }
  if (this->vps_max_layers_minus1 > 0)
  {
    this->vps_all_independent_layers_flag = reader.readFlag("vps_all_independent_layers_flag");
  }
  for (unsigned i = 0; i <= this->vps_max_layers_minus1; i++)
  {
    this->vps_layer_id.push_back(reader.readBits("vps_layer_id", 6));
    if (i > 0 && !this->vps_all_independent_layers_flag)
    {
      this->vps_independent_layer_flag.push_back(reader.readFlag("vps_independent_layer_flag"));
      if (!this->vps_independent_layer_flag[i])
      {
        this->vps_max_tid_ref_present_flag.push_back(
            reader.readFlag("vps_max_tid_ref_present_flag"));
        this->vps_direct_ref_layer_flag.push_back({});
        this->vps_max_tid_il_ref_pics_plus1.push_back({});
        for (unsigned j = 0; j < i; j++)
        {
          this->vps_direct_ref_layer_flag[i].push_back(
              reader.readFlag("vps_direct_ref_layer_flag"));
          if (this->vps_max_tid_ref_present_flag[i] && this->vps_direct_ref_layer_flag[i][j])
          {
            this->vps_max_tid_il_ref_pics_plus1[i].push_back(
                reader.readBits("vps_max_tid_il_ref_pics_plus1", 3));
          }
        }
      }
    }
    else
    {
      this->vps_independent_layer_flag.push_back(true);
    }
  }
  if (this->vps_max_layers_minus1 > 0)
  {
    if (this->vps_all_independent_layers_flag)
    {
      this->vps_each_layer_is_an_ols_flag = reader.readFlag("vps_each_layer_is_an_ols_flag");
    }
    if (!this->vps_each_layer_is_an_ols_flag)
    {
      if (!this->vps_all_independent_layers_flag)
      {
        this->vps_ols_mode_idc =
            reader.readBits("vps_ols_mode_idc", 2, Options().withCheckRange({0, 2}));
      }
      else
      {
        this->vps_ols_mode_idc = 2;
      }
      if (this->vps_ols_mode_idc == 2)
      {
        this->vps_num_output_layer_sets_minus2 =
            reader.readBits("vps_num_output_layer_sets_minus2", 8);
        this->vps_ols_output_layer_flag.push_back({});
        for (unsigned i = 1; i <= this->vps_num_output_layer_sets_minus2 + 1; i++)
        {
          this->vps_ols_output_layer_flag.push_back({});
          for (unsigned j = 0; j <= this->vps_max_layers_minus1; j++)
          {
            this->vps_ols_output_layer_flag[i].push_back(
                reader.readFlag("vps_ols_output_layer_flag"));
          }
        }
      }
    }
    this->vps_num_ptls_minus1 =
        reader.readBits("vps_num_ptls_minus1", 8, Options().withCheckSmaller(TotalNumOlss));
  }
  else
  {
    this->vps_each_layer_is_an_ols_flag = true;
    if (this->vps_all_independent_layers_flag && !this->vps_each_layer_is_an_ols_flag)
      this->vps_ols_mode_idc = 2;
  }

  // The variables NumDirectRefLayers[], DirectRefLayerIdx[][], NumRefLayers[],
  // ReferenceLayerIdx[][], and LayerUsedAsRefLayerFlag[] are derived as follows (28):
  {
    umap_2d<bool> dependencyFlag;
    for (unsigned i = 0; i <= this->vps_max_layers_minus1; i++)
    {
      for (unsigned j = 0; j <= this->vps_max_layers_minus1; j++)
      {
        dependencyFlag[i][j] = this->vps_direct_ref_layer_flag[i][j];
        for (unsigned k = 0; k < i; k++)
          if (this->vps_direct_ref_layer_flag[i][k] && dependencyFlag[k][j])
            dependencyFlag[i][j] = 1;
      }
      this->LayerUsedAsRefLayerFlag[i] = 0;
    }
    for (unsigned i = 0; i <= this->vps_max_layers_minus1; i++)
    {
      auto d = 0u;
      auto r = 0u;
      for (unsigned j = 0; j <= this->vps_max_layers_minus1; j++)
      {
        if (this->vps_direct_ref_layer_flag[i][j])
        {
          this->DirectRefLayerIdx[i][d++]  = j;
          this->LayerUsedAsRefLayerFlag[j] = 1;
        }
        if (dependencyFlag[i][j])
          this->ReferenceLayerIdx[i][r++] = j;
      }
      this->NumDirectRefLayers[i] = d;
      this->NumRefLayers[i]       = r;
    }
  }

  // The variable olsModeIdc is derived as follows (30):
  if (!this->vps_each_layer_is_an_ols_flag)
    this->olsModeIdc = this->vps_ols_mode_idc;
  else
    olsModeIdc = 4;

  // The variable TotalNumOlss, specifying the total number of OLSs specified by the VPS, is derived
  // as follows (31):
  if (this->olsModeIdc == 4 || this->olsModeIdc == 0 || this->olsModeIdc == 1)
    this->TotalNumOlss = this->vps_max_layers_minus1 + 1;
  else if (this->olsModeIdc == 2)
    this->TotalNumOlss = this->vps_num_output_layer_sets_minus2 + 2;

  // NumOutputLayersInOls[], NumSubLayersInLayerInOLS[i][j], OutputLayerIdInOls[i][j],
  // LayerUsedAsOutputLayerFlag[] (32):
  this->NumOutputLayersInOls[0]        = 1;
  this->OutputLayerIdInOls[0][0]       = this->vps_layer_id[0];
  this->NumSubLayersInLayerInOLS[0][0] = this->vps_ptl_max_tid[vps_ols_ptl_idx[0]] + 1;
  this->LayerUsedAsOutputLayerFlag[0]  = true;
  for (unsigned i = 1; i <= this->vps_max_layers_minus1; i++)
  {
    if (this->olsModeIdc == 4 || this->olsModeIdc < 2)
      this->LayerUsedAsOutputLayerFlag[i] = true;
    else if (this->vps_ols_mode_idc == 2)
      this->LayerUsedAsOutputLayerFlag[i] = false;
  }
  for (unsigned i = 1; i < this->TotalNumOlss; i++)
  {
    if (this->olsModeIdc == 4 || this->olsModeIdc == 0)
    {
      this->NumOutputLayersInOls[i]  = 1;
      this->OutputLayerIdInOls[i][0] = this->vps_layer_id[i];
      if (this->vps_each_layer_is_an_ols_flag)
        this->NumSubLayersInLayerInOLS[i][0] = this->vps_ptl_max_tid[vps_ols_ptl_idx[i]] + 1;
      else
      {
        this->NumSubLayersInLayerInOLS[i][i] = this->vps_ptl_max_tid[vps_ols_ptl_idx[i]] + 1;
        for (int k = i - 1; k >= 0; k--)
        {
          this->NumSubLayersInLayerInOLS[i][k] = 0;
          for (unsigned m = k + 1; m <= i; m++)
          {
            auto maxSublayerNeeded = std::min(this->NumSubLayersInLayerInOLS[i][m],
                                              unsigned(this->vps_max_tid_il_ref_pics_plus1[m][k]));
            if (this->vps_direct_ref_layer_flag[m][k] &&
                this->NumSubLayersInLayerInOLS[i][k] < maxSublayerNeeded)
              this->NumSubLayersInLayerInOLS[i][k] = maxSublayerNeeded;
          }
        }
      }
    }
    else if (this->vps_ols_mode_idc == 1)
    {
      this->NumOutputLayersInOls[i] = i + 1;
      for (unsigned j = 0; j < this->NumOutputLayersInOls[i]; j++)
      {
        this->OutputLayerIdInOls[i][j]       = this->vps_layer_id[j];
        this->NumSubLayersInLayerInOLS[i][j] = this->vps_ptl_max_tid[vps_ols_ptl_idx[i]] + 1;
      }
    }
    else if (this->vps_ols_mode_idc == 2)
    {
      for (unsigned j = 0; j <= this->vps_max_layers_minus1; j++)
      {
        this->layerIncludedInOlsFlag[i][j]   = false;
        this->NumSubLayersInLayerInOLS[i][j] = 0;
      }
      auto              highestIncludedLayer = 0u;
      umap_2d<unsigned> OutputLayerIdx;
      {
        auto j = 0u;
        for (unsigned k = 0; k <= this->vps_max_layers_minus1; k++)
        {
          if (this->vps_ols_output_layer_flag[i][k])
          {
            this->layerIncludedInOlsFlag[i][k]  = true;
            highestIncludedLayer                = k;
            this->LayerUsedAsOutputLayerFlag[k] = true;
            OutputLayerIdx[i][j]                = k;
            this->OutputLayerIdInOls[i][j++]    = this->vps_layer_id[k];
            this->NumSubLayersInLayerInOLS[i][k] =
                this->vps_ptl_max_tid[this->vps_ols_ptl_idx[i]] + 1;
          }
        }
        this->NumOutputLayersInOls[i] = j;
      }
      for (unsigned j = 0; j < this->NumOutputLayersInOls[i]; j++)
      {
        auto idx = OutputLayerIdx[i][j];
        for (unsigned k = 0; k < this->NumRefLayers[idx]; k++)
        {
          if (!this->layerIncludedInOlsFlag[i][this->ReferenceLayerIdx[idx][k]])
            this->layerIncludedInOlsFlag[i][this->ReferenceLayerIdx[idx][k]] = 1;
        }
      }
      for (int k = highestIncludedLayer - 1; k >= 0; k--)
      {
        if (this->layerIncludedInOlsFlag[i][k] && !this->vps_ols_output_layer_flag[i][k])
        {
          for (unsigned m = k + 1; m <= highestIncludedLayer; m++)
          {
            auto maxSublayerNeeded = std::min(this->NumSubLayersInLayerInOLS[i][m],
                                              unsigned(this->vps_max_tid_il_ref_pics_plus1[m][k]));
            if (this->vps_direct_ref_layer_flag[m][k] && this->layerIncludedInOlsFlag[i][m] &&
                this->NumSubLayersInLayerInOLS[i][k] < maxSublayerNeeded)
              this->NumSubLayersInLayerInOLS[i][k] = maxSublayerNeeded;
          }
        }
      }
    }
  }

  // NumLayersInOls[], LayerIdInOls[][], NumMultiLayerOlss, MultiLayerOlsIdx[] (33):
  this->NumLayersInOls[0]  = 1;
  this->LayerIdInOls[0][0] = vps_layer_id[0];
  this->NumMultiLayerOlss  = 0;
  for (unsigned i = 1; i < this->TotalNumOlss; i++)
  {
    if (this->vps_each_layer_is_an_ols_flag)
    {
      this->NumLayersInOls[i]  = 1;
      this->LayerIdInOls[i][0] = this->vps_layer_id[i];
    }
    else if (this->vps_ols_mode_idc == 0 || this->vps_ols_mode_idc == 1)
    {
      this->NumLayersInOls[i] = i + 1;
      for (unsigned j = 0; j < this->NumLayersInOls[i]; j++)
        this->LayerIdInOls[i][j] = this->vps_layer_id[j];
    }
    else if (this->vps_ols_mode_idc == 2)
    {
      auto j = 0u;
      for (unsigned k = 0; k <= this->vps_max_layers_minus1; k++)
        if (this->layerIncludedInOlsFlag[i][k])
          this->LayerIdInOls[i][j++] = this->vps_layer_id[k];
      this->NumLayersInOls[i] = j;
    }
    if (this->NumLayersInOls[i] > 1)
    {
      this->MultiLayerOlsIdx[i] = this->NumMultiLayerOlss;
      this->NumMultiLayerOlss++;
    }
  }

  // The variable VpsNumDpbParams, specifying the number of dpb_parameters() syntax strutcures in
  // the VPS, is derived as follows (34):
  if (this->vps_each_layer_is_an_ols_flag)
    this->VpsNumDpbParams = 0;
  else
    this->VpsNumDpbParams = this->vps_num_dpb_params_minus1 + 1;

  for (unsigned i = 0; i <= this->vps_num_ptls_minus1; i++)
  {
    if (i > 0)
    {
      this->vps_pt_present_flag.push_back(reader.readFlag("vps_pt_present_flag"));
    }
    else
    {
      this->vps_pt_present_flag.push_back(true);
    }
    if (!this->vps_default_ptl_dpb_hrd_max_tid_flag)
    {
      this->vps_ptl_max_tid.push_back(reader.readBits("vps_ptl_max_tid", 3));
    }
    else
    {
      this->vps_ptl_max_tid.push_back(true);
    }
  }
  while (!reader.byte_aligned())
  {
    this->vps_ptl_alignment_zero_bit /* equal to 0 */ =
        reader.readFlag("vps_ptl_alignment_zero_bit  /* equal to 0 */");
  }
  for (unsigned i = 0; i <= this->vps_num_ptls_minus1; i++)
  {
    this->profile_tier_level_instance.parse(
        reader, this->vps_pt_present_flag[i], this->vps_ptl_max_tid[i]);
  }
  for (unsigned i = 0; i < this->TotalNumOlss; i++)
  {
    if (this->vps_num_ptls_minus1 > 0 && this->vps_num_ptls_minus1 + 1 != this->TotalNumOlss)
    {
      this->vps_ols_ptl_idx.push_back(reader.readBits("vps_ols_ptl_idx", 8));
    }
    else
    {
      // If not present...
      if (this->vps_num_ptls_minus1 == 0)
        this->vps_ols_ptl_idx.push_back(0);
      else
        this->vps_ols_ptl_idx.push_back(i);
    }
  }
  if (!this->vps_each_layer_is_an_ols_flag)
  {
    this->vps_num_dpb_params_minus1 = reader.readUEV(
        "vps_num_dpb_params_minus1", Options().withCheckRange({0, NumMultiLayerOlss}));
    if (this->vps_max_sublayers_minus1 > 0)
    {
      this->vps_sublayer_dpb_params_present_flag =
          reader.readFlag("vps_sublayer_dpb_params_present_flag");
    }
    for (unsigned i = 0; i < this->VpsNumDpbParams; i++)
    {
      if (!this->vps_default_ptl_dpb_hrd_max_tid_flag)
      {
        this->vps_dpb_max_tid.push_back(reader.readBits("vps_dpb_max_tid", 3));
      }
      else
      {
        this->vps_dpb_max_tid.push_back(this->vps_max_sublayers_minus1);
      }
      this->dpb_parameters_instance.parse(
          reader, this->vps_dpb_max_tid[i], this->vps_sublayer_dpb_params_present_flag);
    }
    for (unsigned i = 0; i < this->NumMultiLayerOlss; i++)
    {
      this->vps_ols_dpb_pic_width.push_back(reader.readUEV("vps_ols_dpb_pic_width"));
      this->vps_ols_dpb_pic_height.push_back(reader.readUEV("vps_ols_dpb_pic_height"));
      this->vps_ols_dpb_chroma_format.push_back(reader.readBits("vps_ols_dpb_chroma_format", 2));
      this->vps_ols_dpb_bitdepth_minus8.push_back(reader.readUEV("vps_ols_dpb_bitdepth_minus8"));
      if (this->VpsNumDpbParams > 1 && this->VpsNumDpbParams != this->NumMultiLayerOlss)
      {
        this->vps_ols_dpb_params_idx.push_back(reader.readUEV("vps_ols_dpb_params_idx"));
      }
      else // Inferr to ..
      {
        if (this->VpsNumDpbParams == 1)
          this->vps_ols_dpb_params_idx.push_back(0);
        else
          this->vps_ols_dpb_params_idx.push_back(i);
      }
    }
    this->vps_timing_hrd_params_present_flag =
        reader.readFlag("vps_timing_hrd_params_present_flag");
    if (this->vps_timing_hrd_params_present_flag)
    {
      this->general_timing_hrd_parameters_instance.parse(reader);
      if (this->vps_max_sublayers_minus1 > 0)
      {
        this->vps_sublayer_cpb_params_present_flag =
            reader.readFlag("vps_sublayer_cpb_params_present_flag");
      }
      this->vps_num_ols_timing_hrd_params_minus1 = reader.readUEV(
          "vps_num_ols_timing_hrd_params_minus1", Options().withCheckRange({0, NumMultiLayerOlss}));
      for (unsigned i = 0; i <= this->vps_num_ols_timing_hrd_params_minus1; i++)
      {
        if (!this->vps_default_ptl_dpb_hrd_max_tid_flag)
        {
          this->vps_hrd_max_tid.push_back(reader.readBits("vps_hrd_max_tid", 3));
        }
        else // inferr ...
        {
          this->vps_hrd_max_tid.push_back(this->vps_max_sublayers_minus1);
        }
        auto firstSubLayer =
            this->vps_sublayer_cpb_params_present_flag ? 0u : this->vps_hrd_max_tid[i];
        this->ols_timing_hrd_parameters_instance.parse(
            reader,
            firstSubLayer,
            this->vps_hrd_max_tid[i],
            &this->general_timing_hrd_parameters_instance);
      }
      if (this->vps_num_ols_timing_hrd_params_minus1 > 0 &&
          this->vps_num_ols_timing_hrd_params_minus1 + 1 != this->NumMultiLayerOlss)
      {
        for (unsigned i = 0; i < this->NumMultiLayerOlss; i++)
        {
          this->vps_ols_timing_hrd_idx.push_back(reader.readUEV("vps_ols_timing_hrd_idx"));
        }
      }
      else
      {
        for (unsigned i = 0; i < this->NumMultiLayerOlss; i++)
        {
          auto inferredVal = this->vps_num_ols_timing_hrd_params_minus1 == 0 ? 0u : i;
          this->vps_ols_timing_hrd_idx.push_back(inferredVal);
        }
      }
    }
  }
  this->vps_extension_flag = reader.readFlag("vps_extension_flag", Options().withCheckEqualTo(0));
  if (this->vps_extension_flag)
  {
    while (reader.more_rbsp_data())
    {
      this->vps_extension_data_flag = reader.readFlag("vps_extension_data_flag");
    }
  }
  this->rbsp_trailing_bits_instance.parse(reader);
}

} // namespace parser::vvc
