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

#include "pps_multilayer_extension.h"

#include <parser/common/Functions.h>

namespace parser::hevc
{

using namespace reader;

void pps_multilayer_extension::parse(SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "pps_multilayer_extension()");

  this->poc_reset_info_present_flag = reader.readFlag("poc_reset_info_present_flag");
  this->pps_infer_scaling_list_flag = reader.readFlag("pps_infer_scaling_list_flag");
  if (this->pps_infer_scaling_list_flag)
    this->pps_scaling_list_ref_layer_id = reader.readBits("pps_scaling_list_ref_layer_id", 6);
  this->num_ref_loc_offsets = reader.readUEV("num_ref_loc_offsets");
  for (unsigned i = 0; i < this->num_ref_loc_offsets; i++)
  {
    this->ref_loc_offset_layer_id.push_back(reader.readBits("ref_loc_offset_layer_id", 6));
    const auto index = this->ref_loc_offset_layer_id.at(i);

    this->scaled_ref_layer_offset_present_flag.push_back(
        reader.readFlag("scaled_ref_layer_offset_present_flag"));
    if (this->scaled_ref_layer_offset_present_flag.at(i))
    {
      this->scaled_ref_layer_left_offset[index] =
          reader.readSEV(formatArray("scaled_ref_layer_left_offset", index));
      this->scaled_ref_layer_top_offset[index] =
          reader.readSEV(formatArray("scaled_ref_layer_top_offset", index));
      this->scaled_ref_layer_right_offset[index] =
          reader.readSEV(formatArray("scaled_ref_layer_right_offset", index));
      this->scaled_ref_layer_bottom_offset[index] =
          reader.readSEV(formatArray("scaled_ref_layer_bottom_offset", index));
    }

    this->ref_region_offset_present_flag.push_back(
        reader.readFlag("ref_region_offset_present_flag"));
    if (this->ref_region_offset_present_flag.at(i))
    {
      this->ref_region_left_offset[index] =
          reader.readSEV(formatArray("ref_region_left_offset", index));
      this->ref_region_top_offset[index] =
          reader.readSEV(formatArray("ref_region_top_offset", index));
      this->ref_region_right_offset[index] =
          reader.readSEV(formatArray("ref_region_right_offset", index));
      this->ref_region_bottom_offset[index] =
          reader.readSEV(formatArray("ref_region_bottom_offset", index));
    }

    this->resample_phase_set_present_flag.push_back(
        reader.readFlag("resample_phase_set_present_flag"));
    if (this->resample_phase_set_present_flag.at(i))
    {
      this->phase_hor_luma[index] = reader.readUEV(formatArray("phase_hor_luma", index));
      this->phase_ver_luma[index] = reader.readUEV(formatArray("phase_ver_luma", index));
      this->phase_hor_chroma_plus8[index] =
          reader.readUEV(formatArray("phase_hor_chroma_plus8", index));
      this->phase_ver_chroma_plus8[index] =
          reader.readUEV(formatArray("phase_ver_chroma_plus8", index));
    }
  }

  this->colour_mapping_enabled_flag = reader.readFlag("colour_mapping_enabled_flag");
  if (this->colour_mapping_enabled_flag)
    this->colourMappingTable.parse(reader);
}

} // namespace parser::hevc
