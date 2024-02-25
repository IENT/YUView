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

#include "parser/common/SubByteReaderLogging.h"
#include "video_signal_info.h"
#include "vps_vui_bsp_hrd_params.h"

namespace parser::hevc
{

// F.7.3.2.1.4 VPS VUI syntax
class vps_vui
{
public:
  vps_vui() {}

  void parse(reader::SubByteReaderLogging &reader,
             const bool                    vps_vui_present_flag,
             const bool                    vps_base_layer_internal_flag,
             const unsigned                NumLayerSets,
             const vector<unsigned> &      MaxSubLayersInLayerSetMinus1,
             const unsigned                MaxLayersMinus1,
             const umap_1d<unsigned> &     NumDirectRefLayers,
             const umap_1d<unsigned> &      layer_id_in_nuh,
             const umap_1d<unsigned> &     LayerIdxInVps,
             const umap_2d<unsigned> &     IdDirectRefLayer,
             const unsigned                vps_num_hrd_parameters,
             const unsigned                NumOutputLayerSets,
             const umap_1d<unsigned> &     NumLayersInIdList,
             const umap_1d<unsigned> &     OlsIdxToLsIdx);

  bool cross_layer_pic_type_aligned_flag{};
  bool cross_layer_irap_aligned_flag{};
  bool all_layers_idr_aligned_flag{};
  bool bit_rate_present_vps_flag{};
  bool pic_rate_present_vps_flag{};

  umap_2d<bool>     bit_rate_present_flag;
  umap_2d<bool>     pic_rate_present_flag;
  umap_2d<unsigned> avg_bit_rate;
  umap_2d<unsigned> max_bit_rate;
  umap_2d<unsigned> constant_pic_rate_idc;
  umap_2d<unsigned> avg_pic_rate;

  bool                      video_signal_info_idx_present_flag{};
  unsigned                  vps_num_video_signal_info_minus1{};
  vector<video_signal_info> videoSignalInfoList;

  umap_1d<unsigned> vps_video_signal_info_idx;

  bool          tiles_not_in_use_flag{};
  umap_1d<bool> tiles_in_use_flag;
  umap_1d<bool> loop_filter_not_across_tiles_flag;
  umap_2d<bool> tile_boundaries_aligned_flag;

  bool          wpp_not_in_use_flag{};
  umap_1d<bool> wpp_in_use_flag;

  bool              single_layer_for_non_irap_flag{};
  bool              higher_layer_irap_skip_flag{};
  bool              ilp_restricted_ref_layers_flag{};
  umap_2d<unsigned> min_spatial_segment_offset_plus1;
  umap_2d<bool>     ctu_based_offset_enabled_flag;
  umap_2d<unsigned> min_horizontal_ctu_offset_plus1;

  bool                   vps_vui_bsp_hrd_present_flag{};
  vps_vui_bsp_hrd_params vpsVuiBspHrdParams{};

  umap_1d<bool> base_layer_parameter_set_compatibility_flag;
};

} // namespace parser::hevc
