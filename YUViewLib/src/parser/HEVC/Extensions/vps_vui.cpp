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

#include "vps_vui.h"

#include <parser/common/Functions.h>

namespace parser::hevc
{

using namespace reader;

void vps_vui::parse(SubByteReaderLogging &   reader,
                    const bool               vps_vui_present_flag,
                    const bool               vps_base_layer_internal_flag,
                    const unsigned           NumLayerSets,
                    const vector<unsigned> & MaxSubLayersInLayerSetMinus1,
                    const unsigned           MaxLayersMinus1,
                    const umap_1d<unsigned> &NumDirectRefLayers,
                    const vector<unsigned> & layer_id_in_nuh,
                    const umap_1d<unsigned> &LayerIdxInVps,
                    const umap_2d<unsigned> &IdDirectRefLayer,
                    const unsigned           vps_num_hrd_parameters,
                    const unsigned           NumOutputLayerSets,
                    const umap_1d<unsigned> &NumLayersInIdList,
                    const umap_1d<unsigned> &OlsIdxToLsIdx)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "vps_vui()");

  this->cross_layer_pic_type_aligned_flag = reader.readFlag("cross_layer_pic_type_aligned_flag");
  if (!this->cross_layer_pic_type_aligned_flag)
    this->cross_layer_irap_aligned_flag = reader.readFlag("cross_layer_irap_aligned_flag");
  else
    this->cross_layer_irap_aligned_flag = vps_vui_present_flag;
  if (this->cross_layer_irap_aligned_flag)
    this->all_layers_idr_aligned_flag = reader.readFlag("all_layers_idr_aligned_flag");
  this->bit_rate_present_vps_flag = reader.readFlag("bit_rate_present_vps_flag");
  this->pic_rate_present_vps_flag = reader.readFlag("pic_rate_present_vps_flag");

  if (this->bit_rate_present_vps_flag || this->pic_rate_present_vps_flag)
  {
    for (unsigned i = vps_base_layer_internal_flag ? 0 : 1; i < NumLayerSets; i++)
    {
      for (unsigned j = 0; j <= MaxSubLayersInLayerSetMinus1[i]; j++)
      {
        if (this->bit_rate_present_vps_flag)
          this->bit_rate_present_flag[i][j] =
              reader.readFlag(formatArray("all_layers_idr_aligned_flag", i, j));
        if (this->pic_rate_present_vps_flag)
          this->pic_rate_present_flag[i][j] =
              reader.readFlag(formatArray("pic_rate_present_flag", i, j));
        if (this->bit_rate_present_flag[i][j])
        {
          this->avg_bit_rate[i][j] = reader.readBits(formatArray("avg_bit_rate", i, j), 16);
          this->max_bit_rate[i][j] = reader.readBits(formatArray("max_bit_rate", i, j), 16);
        }
        if (this->pic_rate_present_flag[i][j])
        {
          this->constant_pic_rate_idc[i][j] =
              reader.readBits(formatArray("constant_pic_rate_idc", i, j), 2);
          this->avg_pic_rate[i][j] = reader.readBits(formatArray("avg_pic_rate", i, j), 16);
        }
      }
    }
  }

  this->video_signal_info_idx_present_flag = reader.readFlag("video_signal_info_idx_present_flag");
  if (this->video_signal_info_idx_present_flag)
    this->vps_num_video_signal_info_minus1 = reader.readBits("vps_num_video_signal_info_minus1", 4);

  for (unsigned i = 0; i <= this->vps_num_video_signal_info_minus1; i++)
  {
    video_signal_info videoSignalInfo;
    videoSignalInfo.parse(reader);
    this->videoSignalInfoList.push_back(videoSignalInfo);
  }

  if (this->video_signal_info_idx_present_flag && this->vps_num_video_signal_info_minus1 > 0)
    for (unsigned i = vps_base_layer_internal_flag ? 0 : 1; i <= MaxLayersMinus1; i++)
      this->vps_video_signal_info_idx[i] = reader.readBits("vps_video_signal_info_idx", 4);

  this->tiles_not_in_use_flag = reader.readFlag("tiles_not_in_use_flag");

  if (!this->tiles_not_in_use_flag)
  {
    for (unsigned i = vps_base_layer_internal_flag ? 0 : 1; i <= MaxLayersMinus1; i++)
    {
      this->tiles_in_use_flag[i] = reader.readFlag("tiles_in_use_flag");
      if (this->tiles_in_use_flag[i])
        this->loop_filter_not_across_tiles_flag[i] = reader.readFlag("tiles_in_use_flag");
    }
    for (unsigned i = vps_base_layer_internal_flag ? 1 : 2; i <= MaxLayersMinus1; i++)
      for (unsigned j = 0; j < NumDirectRefLayers.at(layer_id_in_nuh[i]); j++)
      {
        const auto layerIdx = LayerIdxInVps.at(IdDirectRefLayer.at(layer_id_in_nuh[i]).at(j));
        if (tiles_in_use_flag[i] && tiles_in_use_flag[layerIdx])
          this->tile_boundaries_aligned_flag[i][j] =
              reader.readFlag(formatArray("tile_boundaries_aligned_flag", i, j));
      }
  }

  this->wpp_not_in_use_flag = reader.readFlag("wpp_not_in_use_flag");
  if (!this->wpp_not_in_use_flag)
    for (unsigned i = vps_base_layer_internal_flag ? 0 : 1; i <= MaxLayersMinus1; i++)
      this->wpp_in_use_flag[i] = reader.readFlag(formatArray("wpp_in_use_flag", i));

  this->single_layer_for_non_irap_flag = reader.readFlag("single_layer_for_non_irap_flag");
  this->higher_layer_irap_skip_flag    = reader.readFlag("higher_layer_irap_skip_flag");
  this->ilp_restricted_ref_layers_flag = reader.readFlag("ilp_restricted_ref_layers_flag");
  if (this->ilp_restricted_ref_layers_flag)
  {
    for (unsigned i = 1; i <= MaxLayersMinus1; i++)
      for (unsigned j = 0; j < NumDirectRefLayers.at(layer_id_in_nuh[i]); j++)
      {
        if (vps_base_layer_internal_flag || IdDirectRefLayer.at(layer_id_in_nuh[i]).at(j) > 0)
        {
          this->min_spatial_segment_offset_plus1[i][j] =
              reader.readUEV(formatArray("min_spatial_segment_offset_plus1", i, j));
          if (this->min_spatial_segment_offset_plus1[i][j] > 0)
          {
            this->ctu_based_offset_enabled_flag[i][j] =
                reader.readFlag(formatArray("ctu_based_offset_enabled_flag", i, j));
            if (this->ctu_based_offset_enabled_flag[i][j])
              this->min_horizontal_ctu_offset_plus1[i][j] =
                  reader.readUEV(formatArray("min_horizontal_ctu_offset_plus1", i, j));
          }
        }
      }
  }

  this->vps_vui_bsp_hrd_present_flag = reader.readFlag("vps_vui_bsp_hrd_present_flag");
  if (this->vps_vui_bsp_hrd_present_flag)
    vpsVuiBspHrdParams.parse(reader,
                             vps_num_hrd_parameters,
                             NumOutputLayerSets,
                             NumLayersInIdList,
                             OlsIdxToLsIdx,
                             MaxSubLayersInLayerSetMinus1);

  for (unsigned i = 1; i <= MaxLayersMinus1; i++)
    if (NumDirectRefLayers.at(layer_id_in_nuh[i]) == 0)
      this->base_layer_parameter_set_compatibility_flag[i] =
          reader.readFlag(formatArray("base_layer_parameter_set_compatibility_flag", i));
}

} // namespace parser::hevc
