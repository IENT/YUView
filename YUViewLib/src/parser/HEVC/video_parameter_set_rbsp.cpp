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

#include <parser/common/Functions.h>

namespace parser::hevc
{

using namespace reader;

void video_parameter_set_rbsp::parse(SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "video_parameter_set_rbsp()");

  this->vps_video_parameter_set_id    = reader.readBits("vps_video_parameter_set_id", 4);
  this->vps_base_layer_internal_flag  = reader.readFlag("vps_base_layer_internal_flag");
  this->vps_base_layer_available_flag = reader.readFlag("vps_base_layer_available_flag");
  this->vps_max_layers_minus1         = reader.readBits("vps_max_layers_minus1", 6);
  this->vps_max_sub_layers_minus1     = reader.readBits("vps_max_sub_layers_minus1", 3);
  this->vps_temporal_id_nesting_flag  = reader.readFlag("vps_temporal_id_nesting_flag");
  this->vps_reserved_0xffff_16bits    = reader.readBits("vps_reserved_0xffff_16bits", 16);

  this->profileTierLevel.parse(reader, true, vps_max_sub_layers_minus1);

  this->vps_sub_layer_ordering_info_present_flag =
      reader.readFlag("vps_sub_layer_ordering_info_present_flag");
  for (unsigned int i =
           (this->vps_sub_layer_ordering_info_present_flag ? 0 : this->vps_max_sub_layers_minus1);
       i <= this->vps_max_sub_layers_minus1;
       i++)
  {
    this->vps_max_dec_pic_buffering_minus1[i] =
        reader.readUEV(formatArray("vps_max_dec_pic_buffering_minus1", i));
    this->vps_max_num_reorder_pics[i] = reader.readUEV(formatArray("vps_max_num_reorder_pics", i));
    this->vps_max_latency_increase_plus1[i] =
        reader.readUEV(formatArray("vps_max_latency_increase_plus1", i));
  }

  this->vps_max_layer_id = reader.readBits("vps_max_layer_id", 6);
  this->vps_num_layer_sets_minus1 =
      reader.readUEV("vps_num_layer_sets_minus1", Options().withCheckRange({0, 1023}));

  for (unsigned int i = 1; i <= this->vps_num_layer_sets_minus1; i++)
    for (unsigned int j = 0; j <= this->vps_max_layer_id; j++)
      this->layer_id_included_flag->push_back(
          reader.readFlag(formatArray("layer_id_included_flag", i)));

  this->vps_timing_info_present_flag = reader.readFlag("vps_timing_info_present_flag");
  if (this->vps_timing_info_present_flag)
  {
    // The VPS can provide timing info (the time between two POCs. So the refresh rate)
    this->vps_num_units_in_tick = reader.readBits("vps_num_units_in_tick", 32);
    this->vps_time_scale        = reader.readBits("vps_time_scale", 32);
    this->vps_poc_proportional_to_timing_flag =
        reader.readFlag("vps_poc_proportional_to_timing_flag");
    if (this->vps_poc_proportional_to_timing_flag)
      this->vps_num_ticks_poc_diff_one_minus1 = reader.readUEV("vps_num_ticks_poc_diff_one_minus1");

    // HRD parameters ...
    this->vps_num_hrd_parameters = reader.readUEV("vps_num_hrd_parameters");
    for (unsigned int i = 0; i < vps_num_hrd_parameters; i++)
    {
      this->hrd_layer_set_idx.push_back(reader.readUEV(formatArray("hrd_layer_set_idx", i)));

      if (i == 0)
        this->cprms_present_flag.push_back(1);
      else
        this->cprms_present_flag.push_back(reader.readFlag(formatArray("cprms_present_flag", i)));

      hrd_parameters hrdParameters;
      hrdParameters.parse(reader, cprms_present_flag[i], vps_max_sub_layers_minus1);
      this->vps_hrd_parameters.push_back(hrdParameters);
    }

    this->frameRate = (double)vps_time_scale / (double)vps_num_units_in_tick;
    reader.logArbitrary("frameRate", std::to_string(this->frameRate));
  }

  this->vps_extension_flag = reader.readFlag("vps_extension_flag");
  if (vps_extension_flag)
  {
    while (!reader.byte_aligned())
      reader.readFlag("vps_extension_alignment_bit_equal_to_one", Options().withCheckEqualTo(1));

    vpsExtension.parse(reader,
                       this->vps_max_layers_minus1,
                       this->vps_base_layer_internal_flag,
                       this->vps_max_sub_layers_minus1,
                       this->vps_num_layer_sets_minus1,
                       this->vps_num_hrd_parameters);

    this->vps_extension2_flag = reader.readFlag("vps_extension2_flag");
    if (this->vps_extension2_flag)
    {
      while (reader.more_rbsp_data())
        reader.readFlag("vps_extension_data_flag");
    }
  }

  rbspTrailingBits.parse(reader);
}

} // namespace parser::hevc