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
#include "dpb_parameters.h"
#include "general_timing_hrd_parameters.h"
#include "ols_timing_hrd_parameters.h"
#include "parser/common/SubByteReaderLogging.h"
#include "profile_tier_level.h"
#include "rbsp_trailing_bits.h"

namespace parser::vvc
{

class video_parameter_set_rbsp : public NalRBSP
{
public:
  video_parameter_set_rbsp()  = default;
  ~video_parameter_set_rbsp() = default;
  void parse(reader::SubByteReaderLogging &reader);

  unsigned                      vps_video_parameter_set_id{};
  unsigned                      vps_max_layers_minus1{};
  unsigned                      vps_max_sublayers_minus1{};
  bool                          vps_default_ptl_dpb_hrd_max_tid_flag{true};
  bool                          vps_all_independent_layers_flag{true};
  vector<unsigned>              vps_layer_id{};
  vector<bool>                  vps_independent_layer_flag{};
  vector<bool>                  vps_max_tid_ref_present_flag{};
  vector2d<bool>                vps_direct_ref_layer_flag{};
  vector2d<unsigned>            vps_max_tid_il_ref_pics_plus1{};
  bool                          vps_each_layer_is_an_ols_flag{};
  unsigned                      vps_ols_mode_idc{};
  unsigned                      vps_num_output_layer_sets_minus2{};
  vector2d<bool>                vps_ols_output_layer_flag{};
  unsigned                      vps_num_ptls_minus1{};
  vector<bool>                  vps_pt_present_flag{};
  vector<unsigned>              vps_ptl_max_tid{};
  bool                          vps_ptl_alignment_zero_bit{};
  profile_tier_level            profile_tier_level_instance;
  vector<unsigned>              vps_ols_ptl_idx{};
  unsigned                      vps_num_dpb_params_minus1{};
  bool                          vps_sublayer_dpb_params_present_flag{};
  vector<unsigned>              vps_dpb_max_tid{};
  dpb_parameters                dpb_parameters_instance;
  vector<unsigned>              vps_ols_dpb_pic_width{};
  vector<unsigned>              vps_ols_dpb_pic_height{};
  vector<unsigned>              vps_ols_dpb_chroma_format{};
  vector<unsigned>              vps_ols_dpb_bitdepth_minus8{};
  vector<unsigned>              vps_ols_dpb_params_idx{};
  bool                          vps_timing_hrd_params_present_flag{};
  general_timing_hrd_parameters general_timing_hrd_parameters_instance;
  bool                          vps_sublayer_cpb_params_present_flag{};
  unsigned                      vps_num_ols_timing_hrd_params_minus1{};
  vector<unsigned>              vps_hrd_max_tid{};
  ols_timing_hrd_parameters     ols_timing_hrd_parameters_instance;
  vector<unsigned>              vps_ols_timing_hrd_idx{};
  bool                          vps_extension_flag{};
  bool                          vps_extension_data_flag{};
  rbsp_trailing_bits            rbsp_trailing_bits_instance;

  umap_1d<bool>     LayerUsedAsRefLayerFlag;
  umap_2d<unsigned> DirectRefLayerIdx;
  umap_2d<unsigned> ReferenceLayerIdx;
  umap_1d<unsigned> NumDirectRefLayers;
  umap_1d<unsigned> NumRefLayers;
  unsigned          olsModeIdc{};
  unsigned          TotalNumOlss{};
  umap_1d<unsigned> NumOutputLayersInOls;
  umap_2d<unsigned> OutputLayerIdInOls;
  umap_2d<unsigned> NumSubLayersInLayerInOLS;
  umap_1d<bool>     LayerUsedAsOutputLayerFlag;
  umap_2d<bool>     layerIncludedInOlsFlag;
  umap_1d<unsigned> NumLayersInOls;
  umap_2d<unsigned> LayerIdInOls;
  umap_1d<unsigned> MultiLayerOlsIdx;
  unsigned          NumMultiLayerOlss;
  unsigned          VpsNumDpbParams{};
};

} // namespace parser::vvc
