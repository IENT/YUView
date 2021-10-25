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

#include "seq_parameter_set_rbsp.h"

#include <parser/common/Functions.h>

#include <cmath>

namespace parser::hevc
{

using namespace reader;

void seq_parameter_set_rbsp::parse(SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "seq_parameter_set_rbsp()");

  this->sps_video_parameter_set_id   = reader.readBits("sps_video_parameter_set_id", 4);
  this->sps_max_sub_layers_minus1    = reader.readBits("sps_max_sub_layers_minus1", 3);
  this->sps_temporal_id_nesting_flag = reader.readFlag("sps_temporal_id_nesting_flag");

  // parse profile tier level
  this->profileTierLevel.parse(reader, true, sps_max_sub_layers_minus1);

  /// Back to the seq_parameter_set_rbsp
  this->sps_seq_parameter_set_id = reader.readUEV("sps_seq_parameter_set_id");
  this->chroma_format_idc        = reader.readUEV(
      "chroma_format_idc",
      Options().withMeaningVector({"moochrome", "4:2:0", "4:2:2", "4:4:4", "4:4:4"}));
  if (this->chroma_format_idc == 3)
    this->separate_colour_plane_flag = reader.readFlag("separate_colour_plane_flag");
  this->ChromaArrayType = (this->separate_colour_plane_flag) ? 0 : this->chroma_format_idc;
  reader.logCalculatedValue("ChromaArrayType", this->ChromaArrayType);

  // Rec. ITU-T H.265 v3 (04/2015) - 6.2 - Table 6-1
  this->SubWidthC  = (this->chroma_format_idc == 1 || this->chroma_format_idc == 2) ? 2 : 1;
  this->SubHeightC = (this->chroma_format_idc == 1) ? 2 : 1;
  reader.logCalculatedValue("SubWidthC", this->SubWidthC);
  reader.logCalculatedValue("SubHeightC", this->SubHeightC);

  this->pic_width_in_luma_samples  = reader.readUEV("pic_width_in_luma_samples");
  this->pic_height_in_luma_samples = reader.readUEV("pic_height_in_luma_samples");
  this->conformance_window_flag    = reader.readFlag("conformance_window_flag");

  if (this->conformance_window_flag)
  {
    this->conf_win_left_offset   = reader.readUEV("conf_win_left_offset");
    this->conf_win_right_offset  = reader.readUEV("conf_win_right_offset");
    this->conf_win_top_offset    = reader.readUEV("conf_win_top_offset");
    this->conf_win_bottom_offset = reader.readUEV("conf_win_bottom_offset");
  }

  this->bit_depth_luma_minus8             = reader.readUEV("bit_depth_luma_minus8");
  this->bit_depth_chroma_minus8           = reader.readUEV("bit_depth_chroma_minus8");
  this->log2_max_pic_order_cnt_lsb_minus4 = reader.readUEV("log2_max_pic_order_cnt_lsb_minus4");

  this->sps_sub_layer_ordering_info_present_flag =
      reader.readFlag("sps_sub_layer_ordering_info_present_flag");
  for (unsigned i =
           (this->sps_sub_layer_ordering_info_present_flag ? 0 : this->sps_max_sub_layers_minus1);
       i <= this->sps_max_sub_layers_minus1;
       i++)
  {
    this->sps_max_dec_pic_buffering_minus1.push_back(
        reader.readUEV(formatArray("sps_max_dec_pic_buffering_minus1", i)));
    this->sps_max_num_reorder_pics.push_back(
        reader.readUEV(formatArray("sps_max_num_reorder_pics", i)));
    this->sps_max_latency_increase_plus1.push_back(
        reader.readUEV(formatArray("sps_max_latency_increase_plus1", i)));
  }

  this->log2_min_luma_coding_block_size_minus3 =
      reader.readUEV("log2_min_luma_coding_block_size_minus3");
  this->log2_diff_max_min_luma_coding_block_size =
      reader.readUEV("log2_diff_max_min_luma_coding_block_size");
  this->log2_min_luma_transform_block_size_minus2 =
      reader.readUEV("log2_min_luma_transform_block_size_minus2");
  this->log2_diff_max_min_luma_transform_block_size =
      reader.readUEV("log2_diff_max_min_luma_transform_block_size");
  this->max_transform_hierarchy_depth_inter = reader.readUEV("max_transform_hierarchy_depth_inter");
  this->max_transform_hierarchy_depth_intra = reader.readUEV("max_transform_hierarchy_depth_intra");
  this->scaling_list_enabled_flag           = reader.readFlag("scaling_list_enabled_flag");

  if (this->scaling_list_enabled_flag)
  {
    this->sps_scaling_list_data_present_flag =
        reader.readFlag("sps_scaling_list_data_present_flag");
    if (this->sps_scaling_list_data_present_flag)
      this->scalingListData.parse(reader);
  }

  this->amp_enabled_flag = reader.readFlag("amp_enabled_flag");
  this->sample_adaptive_offset_enabled_flag =
      reader.readFlag("sample_adaptive_offset_enabled_flag");
  this->pcm_enabled_flag = reader.readFlag("pcm_enabled_flag");

  if (this->pcm_enabled_flag)
  {
    this->pcm_sample_bit_depth_luma_minus1 = reader.readBits("pcm_sample_bit_depth_luma_minus1", 4);
    this->pcm_sample_bit_depth_chroma_minus1 =
        reader.readBits("pcm_sample_bit_depth_chroma_minus1", 4);
    this->log2_min_pcm_luma_coding_block_size_minus3 =
        reader.readUEV("log2_min_pcm_luma_coding_block_size_minus3");
    this->log2_diff_max_min_pcm_luma_coding_block_size =
        reader.readUEV("log2_diff_max_min_pcm_luma_coding_block_size");
    this->pcm_loop_filter_disabled_flag = reader.readFlag("pcm_loop_filter_disabled_flag");
  }

  this->num_short_term_ref_pic_sets = reader.readUEV("num_short_term_ref_pic_sets");
  for (unsigned int i = 0; i < this->num_short_term_ref_pic_sets; i++)
  {
    st_ref_pic_set rps;
    rps.parse(reader, i, this->num_short_term_ref_pic_sets);
    this->stRefPicSets.push_back(rps);
  }

  this->long_term_ref_pics_present_flag = reader.readFlag("long_term_ref_pics_present_flag");
  if (this->long_term_ref_pics_present_flag)
  {
    this->num_long_term_ref_pics_sps = reader.readUEV("num_long_term_ref_pics_sps");
    for (unsigned int i = 0; i < this->num_long_term_ref_pics_sps; i++)
    {
      auto nrBits = this->log2_max_pic_order_cnt_lsb_minus4 + 4;
      this->lt_ref_pic_poc_lsb_sps.push_back(
          reader.readBits(formatArray("lt_ref_pic_poc_lsb_sps", i), nrBits));
      this->used_by_curr_pic_lt_sps_flag.push_back(
          reader.readFlag(formatArray("used_by_curr_pic_lt_sps_flag", i)));
    }
  }

  this->sps_temporal_mvp_enabled_flag = reader.readFlag("sps_temporal_mvp_enabled_flag");
  this->strong_intra_smoothing_enabled_flag =
      reader.readFlag("strong_intra_smoothing_enabled_flag");
  this->vui_parameters_present_flag = reader.readFlag("vui_parameters_present_flag");
  if (this->vui_parameters_present_flag)
    this->vuiParameters.parse(reader, this->sps_max_sub_layers_minus1);

  this->sps_extension_present_flag = reader.readFlag("sps_extension_present_flag");
  if (this->sps_extension_present_flag)
  {
    this->sps_range_extension_flag      = reader.readFlag("sps_range_extension_flag");
    this->sps_multilayer_extension_flag = reader.readFlag("sps_multilayer_extension_flag");
    this->sps_3d_extension_flag         = reader.readFlag("sps_3d_extension_flag");
    this->sps_extension_5bits           = reader.readBits("sps_extension_5bits", 5);
  }

  // Now the extensions follow ...
  // This would also be interesting but later.
  if (this->sps_range_extension_flag)
    reader.logArbitrary("sps_range_extension()", 0, "", "", "Not implemented yet...");

  if (this->sps_multilayer_extension_flag)
    reader.logArbitrary("sps_multilayer_extension()", 0, "", "", "Not implemented yet...");

  if (this->sps_3d_extension_flag)
    reader.logArbitrary("sps_3d_extension()", 0, "", "", "Not implemented yet...");

  if (this->sps_extension_5bits != 0)
    reader.logArbitrary("sps_extension_data_flag()", 0, "", "", "Not implemented yet...");

  // rbspTrailingBits.parse(reader);

  // Calculate some values - Rec. ITU-T H.265 v3 (04/2015) 7.4.3.2.1
  this->MinCbLog2SizeY = this->log2_min_luma_coding_block_size_minus3 + 3; // (7-10)
  this->CtbLog2SizeY =
      this->MinCbLog2SizeY + this->log2_diff_max_min_luma_coding_block_size; // (7-11)
  this->CtbSizeY = 1 << this->CtbLog2SizeY;                                  // (7-13)
  this->PicWidthInCtbsY =
      std::ceil((float)this->pic_width_in_luma_samples / this->CtbSizeY); // (7-15)
  this->PicHeightInCtbsY =
      std::ceil((float)this->pic_height_in_luma_samples / this->CtbSizeY); // (7-17)
  this->PicSizeInCtbsY = this->PicWidthInCtbsY * this->PicHeightInCtbsY;   // (7-19)

  reader.logCalculatedValue("MinCbLog2SizeY", this->MinCbLog2SizeY);
  reader.logCalculatedValue("CtbLog2SizeY", this->CtbLog2SizeY);
  reader.logCalculatedValue("CtbSizeY", this->CtbSizeY);
  reader.logCalculatedValue("PicWidthInCtbsY", this->PicWidthInCtbsY);
  reader.logCalculatedValue("PicHeightInCtbsY", this->PicHeightInCtbsY);
  reader.logCalculatedValue("PicSizeInCtbsY", this->PicSizeInCtbsY);
}

} // namespace parser::hevc