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

#include "NalUnitHEVC.h"
#include "parser/common/SubByteReaderLogging.h"
#include "profile_tier_level.h"
#include "scaling_list_data.h"
#include "st_ref_pic_set.h"
#include "vui_parameters.h"

namespace parser::hevc
{

class seq_parameter_set_rbsp : public NalRBSP
{
public:
  seq_parameter_set_rbsp() {}

  void parse(reader::SubByteReaderLogging &reader);

  unsigned           sps_video_parameter_set_id{};
  unsigned           sps_max_sub_layers_minus1{};
  bool               sps_temporal_id_nesting_flag{};
  profile_tier_level profileTierLevel;

  unsigned sps_seq_parameter_set_id{};
  unsigned chroma_format_idc{};
  bool     separate_colour_plane_flag{false};
  unsigned pic_width_in_luma_samples{};
  unsigned pic_height_in_luma_samples{};
  bool     conformance_window_flag{};

  unsigned conf_win_left_offset{};
  unsigned conf_win_right_offset{};
  unsigned conf_win_top_offset{};
  unsigned conf_win_bottom_offset{};

  unsigned         bit_depth_luma_minus8{};
  unsigned         bit_depth_chroma_minus8{};
  unsigned         log2_max_pic_order_cnt_lsb_minus4{};
  bool             sps_sub_layer_ordering_info_present_flag{};
  vector<unsigned> sps_max_dec_pic_buffering_minus1;
  vector<unsigned> sps_max_num_reorder_pics;
  vector<unsigned> sps_max_latency_increase_plus1;

  unsigned          log2_min_luma_coding_block_size_minus3{};
  unsigned          log2_diff_max_min_luma_coding_block_size{};
  unsigned          log2_min_luma_transform_block_size_minus2{};
  unsigned          log2_diff_max_min_luma_transform_block_size{};
  unsigned          max_transform_hierarchy_depth_inter{};
  unsigned          max_transform_hierarchy_depth_intra{};
  bool              scaling_list_enabled_flag{};
  bool              sps_scaling_list_data_present_flag{};
  scaling_list_data scalingListData;

  bool                   amp_enabled_flag{};
  bool                   sample_adaptive_offset_enabled_flag{};
  bool                   pcm_enabled_flag{};
  unsigned               pcm_sample_bit_depth_luma_minus1{};
  unsigned               pcm_sample_bit_depth_chroma_minus1{};
  unsigned               log2_min_pcm_luma_coding_block_size_minus3{};
  unsigned               log2_diff_max_min_pcm_luma_coding_block_size{};
  bool                   pcm_loop_filter_disabled_flag{};
  unsigned               num_short_term_ref_pic_sets{};
  vector<st_ref_pic_set> stRefPicSets;
  bool                   long_term_ref_pics_present_flag{};
  unsigned               num_long_term_ref_pics_sps{};
  vector<unsigned>       lt_ref_pic_poc_lsb_sps;
  vector<bool>           used_by_curr_pic_lt_sps_flag;

  bool sps_temporal_mvp_enabled_flag{};
  bool strong_intra_smoothing_enabled_flag{};

  bool           vui_parameters_present_flag{};
  vui_parameters vuiParameters;

  bool     sps_extension_present_flag{};
  bool     sps_range_extension_flag{};
  bool     sps_multilayer_extension_flag{};
  bool     sps_3d_extension_flag{};
  unsigned sps_extension_5bits{};

  // 7.4.3.2.1
  unsigned ChromaArrayType{};
  unsigned SubWidthC, SubHeightC{};
  unsigned MinCbLog2SizeY;
  unsigned CtbLog2SizeY;
  unsigned CtbSizeY{};
  unsigned PicWidthInCtbsY{};
  unsigned PicHeightInCtbsY{};
  unsigned PicSizeInCtbsY{};

  // Get the actual size of the image that will be returned. Internally the image might be bigger.
  unsigned get_conformance_cropping_width() const
  {
    return (this->pic_width_in_luma_samples - (this->SubWidthC * this->conf_win_right_offset) -
            this->SubWidthC * this->conf_win_left_offset);
  }
  unsigned get_conformance_cropping_height() const
  {
    return (this->pic_height_in_luma_samples - (this->SubHeightC * this->conf_win_bottom_offset) -
            this->SubHeightC * this->conf_win_top_offset);
  }
};

} // namespace parser::hevc