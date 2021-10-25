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

#include "seq_parameter_set_data.h"

#include <parser/common/Functions.h>
#include "typedef.h"

namespace parser::avc

{
using namespace reader;

void seq_parameter_set_data::parse(reader::SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "seq_parameter_set_data()");

  this->profile_idc = reader.readBits(
      "profile_idc",
      8,
      Options().withMeaningMap({{44, "CAVLC 4:4:4 Intra Profile"},
                                {66, "Baseline/Constrained Baseline Profile"},
                                {77, "Main Profile"},
                                {83, "Scalable Baseline/Scalable Constrained Baseline Profile"},
                                {88, "Extended Profile"},
                                {100, "High/Progressive High/Constrained High Profile"},
                                {110, "High 10/High 10 Intra Profile"},
                                {122, "High 4:2:2/High 4:2:2 Intra Profile"},
                                {244, "High 4:4:4 Profile"}}));

  this->constraint_set0_flag = reader.readFlag("constraint_set0_flag");
  this->constraint_set1_flag = reader.readFlag("constraint_set1_flag");
  this->constraint_set2_flag = reader.readFlag("constraint_set2_flag");
  this->constraint_set3_flag = reader.readFlag("constraint_set3_flag");
  this->constraint_set4_flag = reader.readFlag("constraint_set4_flag");
  this->constraint_set5_flag = reader.readFlag("constraint_set5_flag");
  this->reserved_zero_2bits  = reader.readBits("reserved_zero_2bits", 2);
  this->level_idc            = reader.readBits("level_idc", 8);
  this->seq_parameter_set_id = reader.readUEV("seq_parameter_set_id");

  if (this->profile_idc == 100 || this->profile_idc == 110 || this->profile_idc == 122 ||
      this->profile_idc == 244 || this->profile_idc == 44 || this->profile_idc == 83 ||
      this->profile_idc == 86 || this->profile_idc == 118 || this->profile_idc == 128 ||
      this->profile_idc == 138 || this->profile_idc == 139 || this->profile_idc == 134)
  {

    this->chroma_format_idc =
        reader.readUEV("chroma_format_idc",
                       Options()
                           .withMeaningVector({"moochrome", "4:2:0", "4:2:2", "4:4:4", "4:4:4"})
                           .withCheckRange({0, 3}));
    if (this->chroma_format_idc == 3)
      this->separate_colour_plane_flag = reader.readFlag("separate_colour_plane_flag");
    this->bit_depth_luma_minus8   = reader.readUEV("bit_depth_luma_minus8");
    this->bit_depth_chroma_minus8 = reader.readUEV("bit_depth_chroma_minus8");
    this->qpprime_y_zero_transform_bypass_flag =
        reader.readFlag("qpprime_y_zero_transform_bypass_flag");
    this->seq_scaling_matrix_present_flag = reader.readFlag("seq_scaling_matrix_present_flag");
    if (this->seq_scaling_matrix_present_flag)
    {
      for (unsigned i = 0; i < ((this->chroma_format_idc != 3) ? 8 : 12); i++)
      {
        this->seq_scaling_list_present_flag[i] =
            reader.readFlag(formatArray("seq_scaling_list_present_flag", i));
        if (this->seq_scaling_list_present_flag[i])
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
  }

  this->log2_max_frame_num_minus4 = reader.readUEV("log2_max_frame_num_minus4");
  this->pic_order_cnt_type        = reader.readUEV("pic_order_cnt_type");
  if (this->pic_order_cnt_type == 0)
  {
    this->log2_max_pic_order_cnt_lsb_minus4 =
        reader.readUEV("log2_max_pic_order_cnt_lsb_minus4", Options().withCheckRange({0, 12}));
    this->MaxPicOrderCntLsb = 1 << (this->log2_max_pic_order_cnt_lsb_minus4 + 4);
    reader.logCalculatedValue("MaxPicOrderCntLsb", this->MaxPicOrderCntLsb);
  }
  else if (this->pic_order_cnt_type == 1)
  {
    this->delta_pic_order_always_zero_flag = reader.readFlag("delta_pic_order_always_zero_flag");
    this->offset_for_non_ref_pic           = reader.readSEV("offset_for_non_ref_pic");
    this->offset_for_top_to_bottom_field   = reader.readSEV("offset_for_top_to_bottom_field");
    this->num_ref_frames_in_pic_order_cnt_cycle =
        reader.readUEV("num_ref_frames_in_pic_order_cnt_cycle");
    for (unsigned int i = 0; i < this->num_ref_frames_in_pic_order_cnt_cycle; i++)
      this->offset_for_ref_frame[i] = reader.readSEV(formatArray("offset_for_ref_frame", i));
  }

  this->max_num_ref_frames = reader.readUEV("max_num_ref_frames");
  this->gaps_in_frame_num_value_allowed_flag =
      reader.readFlag("gaps_in_frame_num_value_allowed_flag");
  this->pic_width_in_mbs_minus1        = reader.readUEV("pic_width_in_mbs_minus1");
  this->pic_height_in_map_units_minus1 = reader.readUEV("pic_height_in_map_units_minus1");
  this->frame_mbs_only_flag            = reader.readFlag("frame_mbs_only_flag");
  if (!frame_mbs_only_flag)
    this->mb_adaptive_frame_field_flag = reader.readFlag("mb_adaptive_frame_field_flag");

  this->direct_8x8_inference_flag = reader.readFlag("direct_8x8_inference_flag");
  if (!frame_mbs_only_flag && !direct_8x8_inference_flag)
    throw std::logic_error(
        "When frame_mbs_only_flag is equal to 0, direct_8x8_inference_flag shall be equal to 1.");

  this->frame_cropping_flag = reader.readFlag("frame_cropping_flag");
  if (this->frame_cropping_flag)
  {
    this->frame_crop_left_offset   = reader.readUEV("frame_crop_left_offset");
    this->frame_crop_right_offset  = reader.readUEV("frame_crop_right_offset");
    this->frame_crop_top_offset    = reader.readUEV("frame_crop_top_offset");
    this->frame_crop_bottom_offset = reader.readUEV("frame_crop_bottom_offset");
  }

  // Calculate some values
  this->BitDepthY           = 8 + this->bit_depth_luma_minus8;
  this->QpBdOffsetY         = 6 * this->bit_depth_luma_minus8;
  this->BitDepthC           = 8 + this->bit_depth_chroma_minus8;
  this->QpBdOffsetC         = 6 * this->bit_depth_chroma_minus8;
  this->PicWidthInMbs       = this->pic_width_in_mbs_minus1 + 1;
  this->PicHeightInMapUnits = this->pic_height_in_map_units_minus1 + 1;
  this->FrameHeightInMbs =
      this->frame_mbs_only_flag ? this->PicHeightInMapUnits : this->PicHeightInMapUnits * 2;
  // PicHeightInMbs is actually calculated per frame from the field_pic_flag (it can switch for
  // interlaced content). For the whole sequence, we will calculate the frame size (field_pic_flag =
  // 0). Real calculation per frame: PicHeightInMbs = FrameHeightInMbs / (1 + field_pic_flag);
  this->PicHeightInMbs = this->FrameHeightInMbs;
  this->PicSizeInMbs   = this->PicWidthInMbs * this->PicHeightInMbs;
  this->SubWidthC      = (this->chroma_format_idc == 3) ? 1 : 2;
  this->SubHeightC     = (this->chroma_format_idc == 1) ? 2 : 1;
  if (this->chroma_format_idc == 0) // Monochrome
  {
    this->MbHeightC = 0;
    this->MbWidthC  = 0;
  }
  else
  {
    this->MbWidthC  = 16 / this->SubWidthC;
    this->MbHeightC = 16 / this->SubHeightC;
  }

  // The picture size in samples
  this->PicHeightInSamplesL = this->PicHeightInMbs * 16;
  this->PicHeightInSamplesC = this->PicHeightInMbs * this->MbHeightC;
  this->PicWidthInSamplesL  = this->PicWidthInMbs * 16;
  this->PicWidthInSamplesC  = this->PicWidthInMbs * this->MbWidthC;

  this->PicSizeInMapUnits = this->PicWidthInMbs * this->PicHeightInMapUnits;
  if (!this->separate_colour_plane_flag)
    this->ChromaArrayType = this->chroma_format_idc;
  else
    this->ChromaArrayType = 0;

  // There may be cropping for output
  if (this->ChromaArrayType == 0)
  {
    this->CropUnitX = 1;
    this->CropUnitY = 2 - (this->frame_mbs_only_flag ? 1 : 0);
  }
  else
  {
    this->CropUnitX = this->SubWidthC;
    this->CropUnitY = this->SubHeightC * (2 - (this->frame_mbs_only_flag ? 1 : 0));
  }

  // Calculate the cropping rectangle
  this->PicCropLeftOffset = this->CropUnitX * this->frame_crop_left_offset;
  this->PicCropWidth      = this->PicWidthInSamplesL -
                       (this->CropUnitX * this->frame_crop_right_offset + 1) -
                       this->PicCropLeftOffset + 1;
  PicCropTopOffset    = CropUnitY * frame_crop_top_offset;
  this->PicCropHeight = (16 * this->FrameHeightInMbs) -
                        (this->CropUnitY * this->frame_crop_bottom_offset + 1) -
                        this->PicCropTopOffset + 1;

  auto field_pic_flag  = false; // For now, assume field_pic_flag false
  this->MbaffFrameFlag = (this->mb_adaptive_frame_field_flag && !field_pic_flag);
  if (this->pic_order_cnt_type == 1)
  {
    // (7-12)
    for (unsigned int i = 0; i < this->num_ref_frames_in_pic_order_cnt_cycle; i++)
      this->ExpectedDeltaPerPicOrderCntCycle += this->offset_for_ref_frame[i];
  }
  // 7-10
  this->MaxFrameNum = 1 << (this->log2_max_frame_num_minus4 + 4);

  // Parse the VUI
  this->vui_parameters_present_flag = reader.readFlag("vui_parameters_present_flag");
  if (this->vui_parameters_present_flag)
    vuiParameters.parse(reader,
                        this->BitDepthY,
                        this->BitDepthC,
                        this->chroma_format_idc,
                        this->frame_mbs_only_flag);

  // Log all the calculated values
  reader.logCalculatedValue("BitDepthY", this->BitDepthY);
  reader.logCalculatedValue("QpBdOffsetY", this->QpBdOffsetY);
  reader.logCalculatedValue("BitDepthC", this->BitDepthC);
  reader.logCalculatedValue("QpBdOffsetC", this->QpBdOffsetC);
  reader.logCalculatedValue("PicWidthInMbs", this->PicWidthInMbs);
  reader.logCalculatedValue("PicHeightInMapUnits", this->PicHeightInMapUnits);
  reader.logCalculatedValue("FrameHeightInMbs", this->FrameHeightInMbs);
  reader.logCalculatedValue("PicHeightInMbs", this->PicHeightInMbs);
  reader.logCalculatedValue("PicSizeInMbs", this->PicSizeInMbs);
  reader.logCalculatedValue("SubWidthC", this->SubWidthC);
  reader.logCalculatedValue("SubHeightC", this->SubHeightC);
  reader.logCalculatedValue("MbHeightC", this->MbHeightC);
  reader.logCalculatedValue("MbWidthC", this->MbWidthC);
  reader.logCalculatedValue("PicHeightInSamplesL", this->PicHeightInSamplesL);
  reader.logCalculatedValue("PicHeightInSamplesC", this->PicHeightInSamplesC);
  reader.logCalculatedValue("PicWidthInSamplesL", this->PicWidthInSamplesL);
  reader.logCalculatedValue("PicWidthInSamplesC", this->PicWidthInSamplesC);
  reader.logCalculatedValue("PicSizeInMapUnits", this->PicSizeInMapUnits);
  reader.logCalculatedValue("ChromaArrayType", this->ChromaArrayType);
  reader.logCalculatedValue("CropUnitX", this->CropUnitX);
  reader.logCalculatedValue("CropUnitY", this->CropUnitY);
  reader.logCalculatedValue("PicCropLeftOffset", this->PicCropLeftOffset);
  reader.logCalculatedValue("PicCropWidth", this->PicCropWidth);
  reader.logCalculatedValue("PicCropTopOffset", this->PicCropTopOffset);
  reader.logCalculatedValue("PicCropHeight", this->PicCropHeight);
  reader.logCalculatedValue("MbaffFrameFlag", this->MbaffFrameFlag);
  reader.logCalculatedValue("MaxFrameNum", this->MaxFrameNum);
}

} // namespace parser::av1