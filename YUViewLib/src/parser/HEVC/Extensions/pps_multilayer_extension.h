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

#include "colour_mapping_table.h"
#include "parser/common/SubByteReaderLogging.h"


namespace parser::hevc
{

// F.7.3.2.3.4 Picture parameter set multilayer extension syntax
class pps_multilayer_extension
{
public:
  pps_multilayer_extension() {}

  void parse(reader::SubByteReaderLogging &reader);

  bool             poc_reset_info_present_flag{};
  bool             pps_infer_scaling_list_flag{};
  unsigned         pps_scaling_list_ref_layer_id{};
  unsigned         num_ref_loc_offsets{};
  vector<unsigned> ref_loc_offset_layer_id;

  vector<bool> scaled_ref_layer_offset_present_flag;
  umap_1d<int> scaled_ref_layer_left_offset;
  umap_1d<int> scaled_ref_layer_top_offset;
  umap_1d<int> scaled_ref_layer_right_offset;
  umap_1d<int> scaled_ref_layer_bottom_offset;

  vector<bool> ref_region_offset_present_flag;
  umap_1d<int> ref_region_left_offset;
  umap_1d<int> ref_region_top_offset;
  umap_1d<int> ref_region_right_offset;
  umap_1d<int> ref_region_bottom_offset;

  vector<bool>      resample_phase_set_present_flag;
  umap_1d<unsigned> phase_hor_luma;
  umap_1d<unsigned> phase_ver_luma;
  umap_1d<unsigned> phase_hor_chroma_plus8;
  umap_1d<unsigned> phase_ver_chroma_plus8;

  bool                 colour_mapping_enabled_flag{};
  colour_mapping_table colourMappingTable;
};

} // namespace parser::hevc
