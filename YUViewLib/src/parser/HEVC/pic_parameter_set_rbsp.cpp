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

#include "pic_parameter_set_rbsp.h"

#include <parser/common/Functions.h>

namespace parser::hevc
{

using namespace reader;

void pic_parameter_set_rbsp::parse(SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "pic_parameter_set_rbsp()");

  this->pps_pic_parameter_set_id = reader.readUEV("pps_pic_parameter_set_id");
  this->pps_seq_parameter_set_id = reader.readUEV("pps_seq_parameter_set_id");
  this->dependent_slice_segments_enabled_flag =
      reader.readFlag("dependent_slice_segments_enabled_flag");
  this->output_flag_present_flag    = reader.readFlag("output_flag_present_flag");
  this->num_extra_slice_header_bits = reader.readBits("num_extra_slice_header_bits", 3);

  this->sign_data_hiding_enabled_flag = reader.readFlag("sign_data_hiding_enabled_flag");
  this->cabac_init_present_flag       = reader.readFlag("cabac_init_present_flag");
  this->num_ref_idx_l0_default_active_minus1 =
      reader.readUEV("num_ref_idx_l0_default_active_minus1");
  this->num_ref_idx_l1_default_active_minus1 =
      reader.readUEV("num_ref_idx_l1_default_active_minus1");
  this->init_qp_minus26             = reader.readSEV("init_qp_minus26");
  this->constrained_intra_pred_flag = reader.readFlag("constrained_intra_pred_flag");
  this->transform_skip_enabled_flag = reader.readFlag("transform_skip_enabled_flag");
  this->cu_qp_delta_enabled_flag    = reader.readFlag("cu_qp_delta_enabled_flag");
  if (this->cu_qp_delta_enabled_flag)
    this->diff_cu_qp_delta_depth = reader.readUEV("diff_cu_qp_delta_depth");
  this->pps_cb_qp_offset = reader.readSEV("pps_cb_qp_offset");
  this->pps_cr_qp_offset = reader.readSEV("pps_cr_qp_offset");
  this->pps_slice_chroma_qp_offsets_present_flag =
      reader.readFlag("pps_slice_chroma_qp_offsets_present_flag");
  this->weighted_pred_flag               = reader.readFlag("weighted_pred_flag");
  this->weighted_bipred_flag             = reader.readFlag("weighted_bipred_flag");
  this->transquant_bypass_enabled_flag   = reader.readFlag("transquant_bypass_enabled_flag");
  this->tiles_enabled_flag               = reader.readFlag("tiles_enabled_flag");
  this->entropy_coding_sync_enabled_flag = reader.readFlag("entropy_coding_sync_enabled_flag");
  if (this->tiles_enabled_flag)
  {
    this->num_tile_columns_minus1 = reader.readUEV("num_tile_columns_minus1");
    this->num_tile_rows_minus1    = reader.readUEV("num_tile_rows_minus1");
    this->uniform_spacing_flag    = reader.readFlag("uniform_spacing_flag");
    if (!this->uniform_spacing_flag)
    {
      for (unsigned i = 0; i < this->num_tile_columns_minus1; i++)
        this->column_width_minus1.push_back(reader.readUEV(formatArray("column_width_minus1", i)));
      for (unsigned i = 0; i < this->num_tile_rows_minus1; i++)
        this->row_height_minus1.push_back(reader.readUEV(formatArray("row_height_minus1", i)));
    }
    this->loop_filter_across_tiles_enabled_flag =
        reader.readFlag("loop_filter_across_tiles_enabled_flag");
  }
  this->pps_loop_filter_across_slices_enabled_flag =
      reader.readFlag("pps_loop_filter_across_slices_enabled_flag");
  this->deblocking_filter_control_present_flag =
      reader.readFlag("deblocking_filter_control_present_flag");
  if (this->deblocking_filter_control_present_flag)
  {
    this->deblocking_filter_override_enabled_flag =
        reader.readFlag("deblocking_filter_override_enabled_flag");
    this->pps_deblocking_filter_disabled_flag =
        reader.readFlag("pps_deblocking_filter_disabled_flag");
    if (!this->pps_deblocking_filter_disabled_flag)
    {
      this->pps_beta_offset_div2 = reader.readSEV("pps_beta_offset_div2");
      this->pps_tc_offset_div2   = reader.readSEV("pps_tc_offset_div2");
    }
  }
  this->pps_scaling_list_data_present_flag = reader.readFlag("pps_scaling_list_data_present_flag");
  if (this->pps_scaling_list_data_present_flag)
    this->scalingListData.parse(reader);
  this->lists_modification_present_flag  = reader.readFlag("lists_modification_present_flag");
  this->log2_parallel_merge_level_minus2 = reader.readUEV("log2_parallel_merge_level_minus2");
  this->slice_segment_header_extension_present_flag =
      reader.readFlag("slice_segment_header_extension_present_flag");
  this->pps_extension_present_flag = reader.readFlag("pps_extension_present_flag");
  if (this->pps_extension_present_flag)
  {
    this->pps_range_extension_flag      = reader.readFlag("pps_range_extension_flag");
    this->pps_multilayer_extension_flag = reader.readFlag("pps_multilayer_extension_flag");
    this->pps_3d_extension_flag         = reader.readFlag("pps_3d_extension_flag");
    this->pps_extension_5bits           = reader.readBits("pps_extension_5bits", 5);
  }

  // Now the extensions follow ...

  if (this->pps_range_extension_flag)
    this->ppsRangeExtension.parse(reader, this->transform_skip_enabled_flag);

  // This would also be interesting but later.
  if (this->pps_multilayer_extension_flag)
    reader.logArbitrary("pps_multilayer_extension()", "", "", "", "Not implemented yet...");

  if (this->pps_3d_extension_flag)
    reader.logArbitrary("pps_3d_extension()", "", "", "", "Not implemented yet...");

  if (this->pps_extension_5bits != 0)
    reader.logArbitrary("pps_extension_data_flag()", "", "", "", "Not implemented yet...");

  // There is more to parse but we are not interested in this information (for now)
  // this->rbspTrailingBits.parse(reader);

  if (entropy_coding_sync_enabled_flag && tiles_enabled_flag)
    reader.logArbitrary("Parallelism", "Mixed");
  else if (entropy_coding_sync_enabled_flag)
    reader.logArbitrary("Parallelism", "Wavefront");
  else if (tiles_enabled_flag)
    reader.logArbitrary("Parallelism", "Tile");
  else
    reader.logArbitrary("Parallelism", "Slice");
}

} // namespace parser::hevc