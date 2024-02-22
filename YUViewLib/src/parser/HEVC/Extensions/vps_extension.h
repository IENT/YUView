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

#include "../profile_tier_level.h"
#include "dpb_size.h"
#include "parser/common/SubByteReaderLogging.h"
#include "rep_format.h"
#include "vps_vui.h"

namespace parser::hevc
{

// F.7.3.2.1.1 Video parameter set extension syntax
class vps_extension
{
public:
  vps_extension() {}

  void parse(reader::SubByteReaderLogging &reader,
             const unsigned                vps_max_layers_minus1,
             const bool                    vps_base_layer_internal_flag,
             const unsigned                vps_max_sub_layers_minus1,
             const unsigned                vps_num_layer_sets_minus1);

  profile_tier_level profileTierLevel;
  bool               splitting_flag{};
  vector<bool>       scalability_mask_flag{};
  vector<unsigned>   dimension_id_len_minus1{};
  bool               vps_nuh_layer_id_present_flag{};
  vector<unsigned>   layer_id_in_nuh{};
  umap_1d<unsigned>  LayerIdxInVps{};
  vector2d<unsigned> dimension_id{};
  unsigned           view_id_len{};
  vector<unsigned>   view_id_val{};

  void               calcualteValuesF3();
  unsigned           MaxLayersMinus1{};
  vector2d<unsigned> ScalabilityId;
  umap_1d<unsigned>  ViewOrderIdx;
  umap_1d<unsigned>  DepthLayerFlag;
  umap_1d<unsigned>  DependencyId;
  umap_1d<unsigned>  AuxId;
  unsigned           NumViews{};

  umap_2d<bool> direct_dependency_flag;
  unsigned      num_add_layer_sets{};

  void          calcualteValuesF4();
  umap_2d<bool> DependencyFlag;

  void              calcualteValuesF5();
  umap_2d<unsigned> IdDirectRefLayer;
  umap_2d<unsigned> IdRefLayer;
  umap_2d<unsigned> IdPredictedLayer;
  umap_1d<unsigned> NumDirectRefLayers;
  umap_1d<unsigned> NumRefLayers;
  umap_1d<unsigned> NumPredictedLayers;

  void                     calcualteValuesF6();
  std::array<unsigned, 64> layerIdInListFlag{};
  umap_2d<unsigned>        TreePartitionLayerIdList;
  vector<unsigned>         NumLayersInTreePartition;
  unsigned                 NumIndependentLayers{};

  umap_2d<unsigned> highest_layer_idx_plus1;
  void              calculateValuesF9(const unsigned vps_num_layer_sets_minus1, const unsigned i);
  umap_2d<unsigned> LayerSetLayerIdList;
  umap_1d<unsigned> NumLayersInIdList;

  bool                        vps_sub_layers_max_minus1_present_flag{};
  vector<unsigned>            sub_layers_vps_max_minus1;
  void                        calcualteValuesF10();
  vector<unsigned>            MaxSubLayersInLayerSetMinus1;
  bool                        max_tid_ref_present_flag{};
  umap_2d<unsigned>           max_tid_il_ref_pics_plus1;
  bool                        default_ref_layers_active_flag{};
  unsigned                    vps_num_profile_tier_level_minus1{};
  umap_1d<bool>               vps_profile_present_flag{};
  umap_1d<profile_tier_level> profileTierLevelSubLayers;

  unsigned NumLayerSets{};
  unsigned num_add_olss{};
  unsigned default_output_layer_idc{};
  unsigned defaultOutputLayerIdc{};

  unsigned          NumOutputLayerSets{};
  umap_1d<unsigned> layer_set_idx_for_ols_minus1;
  void              calculateValuesF11(const unsigned i);
  umap_1d<unsigned> OlsIdxToLsIdx;

  umap_2d<bool>     output_layer_flag;
  umap_2d<bool>     OutputLayerFlag;
  void              calcualteValuesF12(const unsigned i);
  umap_1d<unsigned> NumOutputLayersInOutputLayerSet;
  umap_1d<unsigned> OlsHighestOutputLayerId;
  umap_2d<unsigned> profile_tier_level_idx;

  void              calculateValuesF13(const unsigned olsIdx);
  umap_2d<bool>     NecessaryLayerFlag;
  umap_1d<unsigned> NumNecessaryLayers;

  umap_1d<unsigned> alt_output_layer_flag;

  unsigned           vps_num_rep_formats_minus1{};
  vector<rep_format> repFormats;

  bool             rep_format_idx_present_flag{};
  vector<unsigned> vps_rep_format_idx;
  bool             max_one_active_ref_layer_flag{};
  bool             vps_poc_lsb_aligned_flag{};
  umap_1d<bool>    poc_lsb_not_present_flag;

  dpb_size dpbSize{};

  unsigned          direct_dep_type_len_minus2{};
  bool              direct_dependency_all_layers_flag{};
  unsigned          direct_dependency_all_layers_type{};
  umap_2d<unsigned> direct_dependency_type;

  unsigned vps_non_vui_extension_length{};
  unsigned vps_non_vui_extension_data_byte{};
  bool     vps_vui_present_flag{};

  vps_vui vpsVui;
};

} // namespace parser::hevc
