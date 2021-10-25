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
#include "seq_parameter_set_rbsp.h"
#include "typedef.h"

#include <cmath>

namespace parser::avc

{
using namespace reader;

void pic_parameter_set_rbsp::parse(reader::SubByteReaderLogging &reader, SPSMap &spsMap)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "pic_parameter_set_rbsp()");

  this->pic_parameter_set_id =
      reader.readUEV("pic_parameter_set_id", Options().withCheckRange({0, 255}));
  this->seq_parameter_set_id =
      reader.readUEV("seq_parameter_set_id", Options().withCheckRange({0, 31}));

  // Get the referenced sps
  if (spsMap.count(this->seq_parameter_set_id) == 0)
    throw std::logic_error("The signaled SPS was not found in the bitstream.");
  auto refSPS = spsMap.at(this->seq_parameter_set_id);

  this->entropy_coding_mode_flag =
      reader.readFlag("entropy_coding_mode_flag", Options().withMeaningVector({"CAVLC", "CABAC"}));
  this->bottom_field_pic_order_in_frame_present_flag =
      reader.readFlag("bottom_field_pic_order_in_frame_present_flag");
  this->num_slice_groups_minus1 = reader.readUEV("num_slice_groups_minus1");
  if (this->num_slice_groups_minus1 > 0)
  {
    this->slice_group_map_type =
        reader.readUEV("slice_group_map_type", Options().withCheckRange({0, 6}));
    if (this->slice_group_map_type == 0)
    {
      for (unsigned iGroup = 0; iGroup <= this->num_slice_groups_minus1; iGroup++)
        this->run_length_minus1[iGroup] = reader.readUEV(formatArray("run_length_minus1", iGroup));
    }
    else if (this->slice_group_map_type == 2)
    {
      for (unsigned iGroup = 0; iGroup < this->num_slice_groups_minus1; iGroup++)
      {
        this->top_left[iGroup]     = reader.readUEV(formatArray("top_left", iGroup));
        this->bottom_right[iGroup] = reader.readUEV(formatArray("bottom_right", iGroup));
      }
    }
    else if (this->slice_group_map_type == 3 || this->slice_group_map_type == 4 ||
             this->slice_group_map_type == 5)
    {
      this->slice_group_change_direction_flag =
          reader.readFlag("slice_group_change_direction_flag");
      this->slice_group_change_rate_minus1 = reader.readUEV("slice_group_change_rate_minus1");
      this->SliceGroupChangeRate           = slice_group_change_rate_minus1 + 1;
    }
    else if (this->slice_group_map_type == 6)
    {
      this->pic_size_in_map_units_minus1 = reader.readUEV("pic_size_in_map_units_minus1");
      for (unsigned i = 0; i <= this->pic_size_in_map_units_minus1; i++)
      {
        auto nrBits = std::ceil(std::log2(this->num_slice_groups_minus1 + 1));
        this->slice_group_id.push_back(reader.readBits(formatArray("slice_group_id", i), nrBits));
      }
    }
  }
  this->num_ref_idx_l0_default_active_minus1 =
      reader.readUEV("num_ref_idx_l0_default_active_minus1", Options().withCheckRange({0, 31}));
  this->num_ref_idx_l1_default_active_minus1 =
      reader.readUEV("num_ref_idx_l1_default_active_minus1", Options().withCheckRange({0, 31}));
  this->weighted_pred_flag = reader.readFlag("weighted_pred_flag");
  this->weighted_bipred_idc =
      reader.readBits("weighted_bipred_idc", 2, Options().withCheckRange({0, 2}));
  this->pic_init_qp_minus26 = reader.readSEV(
      "pic_init_qp_minus26",
      Options().withCheckRange({-(26 + int(refSPS->seqParameterSetData.QpBdOffsetY)), 25}));
  this->pic_init_qs_minus26 =
      reader.readSEV("pic_init_qs_minus26", Options().withCheckRange({-26, 25}));
  this->chroma_qp_index_offset =
      reader.readSEV("chroma_qp_index_offset", Options().withCheckRange({-12, 12}));
  this->deblocking_filter_control_present_flag =
      reader.readFlag("deblocking_filter_control_present_flag");
  this->constrained_intra_pred_flag    = reader.readFlag("constrained_intra_pred_flag");
  this->redundant_pic_cnt_present_flag = reader.readFlag("redundant_pic_cnt_present_flag");
  if (reader.more_rbsp_data())
  {
    this->transform_8x8_mode_flag         = reader.readFlag("transform_8x8_mode_flag");
    this->pic_scaling_matrix_present_flag = reader.readFlag("pic_scaling_matrix_present_flag");
    if (this->pic_scaling_matrix_present_flag)
    {
      for (unsigned i = 0; i < 6 + ((refSPS->seqParameterSetData.chroma_format_idc != 3) ? 2u : 6u) *
                                       (this->transform_8x8_mode_flag ? 1 : 0);
           i++)
      {
        this->pic_scaling_list_present_flag[i] =
            reader.readFlag(formatArray("pic_scaling_list_present_flag", i));
        if (this->pic_scaling_list_present_flag[i])
        {
          if (i < 6)
          {
            auto newFlag = read_scaling_list<16>(reader, this->ScalingList4x4[i]);
            if (newFlag)
              this->UseDefaultScalingMatrix4x4Flag[i] = *newFlag;
          }
          else
          {
            auto newFlag = read_scaling_list<64>(reader, this->ScalingList8x8[i - 6]);
            if (newFlag)
              this->UseDefaultScalingMatrix8x8Flag[i - 6] = *newFlag;
          }
        }
      }
    }
    this->second_chroma_qp_index_offset =
        reader.readSEV("second_chroma_qp_index_offset", Options().withCheckRange({-12, 12}));
  }
  this->rbspTrailingBits.parse(reader);
}

} // namespace parser::avc