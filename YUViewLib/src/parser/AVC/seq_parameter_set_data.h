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

#include "parser/common/SubByteReaderLogging.h"
#include "vui_parameters.h"

#include <array>

namespace parser::avc
{

class seq_parameter_set_data
{
public:
  seq_parameter_set_data() = default;

  void parse(reader::SubByteReaderLogging &reader);

  unsigned       profile_idc{};
  bool           constraint_set0_flag{};
  bool           constraint_set1_flag{};
  bool           constraint_set2_flag{};
  bool           constraint_set3_flag{};
  bool           constraint_set4_flag{};
  bool           constraint_set5_flag{};
  unsigned       reserved_zero_2bits{};
  unsigned       level_idc{};
  unsigned       seq_parameter_set_id{};
  unsigned       chroma_format_idc{1};
  bool           separate_colour_plane_flag{};
  unsigned       bit_depth_luma_minus8{0};
  unsigned       bit_depth_chroma_minus8{0};
  bool           qpprime_y_zero_transform_bypass_flag{};
  bool           seq_scaling_matrix_present_flag{};
  array<bool, 8> seq_scaling_list_present_flag{};

  array2d<int, 6, 16> ScalingList4x4;
  array<bool, 6>      UseDefaultScalingMatrix4x4Flag{};
  array2d<int, 6, 64> ScalingList8x8{};
  array<bool, 2>      UseDefaultScalingMatrix8x8Flag{};

  unsigned        log2_max_frame_num_minus4{};
  unsigned        pic_order_cnt_type{};
  unsigned        log2_max_pic_order_cnt_lsb_minus4{};
  bool            delta_pic_order_always_zero_flag{};
  int             offset_for_non_ref_pic{};
  int             offset_for_top_to_bottom_field{};
  unsigned        num_ref_frames_in_pic_order_cnt_cycle{};
  array<int, 256> offset_for_ref_frame{};
  unsigned        max_num_ref_frames{};
  bool            gaps_in_frame_num_value_allowed_flag{};
  unsigned        pic_width_in_mbs_minus1{};
  unsigned        pic_height_in_map_units_minus1{};
  bool            frame_mbs_only_flag{};
  bool            mb_adaptive_frame_field_flag{};
  bool            direct_8x8_inference_flag{};
  bool            frame_cropping_flag{};
  unsigned        frame_crop_left_offset{0};
  unsigned        frame_crop_right_offset{0};
  unsigned        frame_crop_top_offset{0};
  unsigned        frame_crop_bottom_offset{0};
  bool            vui_parameters_present_flag{};

  vui_parameters vuiParameters;

  // The following values are not read from the bitstream but are calculated from the read values.
  unsigned int BitDepthY{};
  unsigned int QpBdOffsetY{};
  unsigned int BitDepthC{};
  unsigned int QpBdOffsetC{};
  unsigned int PicWidthInMbs{};
  unsigned int FrameHeightInMbs{};
  unsigned int PicHeightInMbs{};
  unsigned int PicHeightInMapUnits{};
  unsigned int PicSizeInMbs{};
  unsigned int PicSizeInMapUnits{};
  unsigned int SubWidthC{};
  unsigned int SubHeightC{};
  unsigned int MbHeightC{};
  unsigned int MbWidthC{};
  unsigned int PicHeightInSamplesL{};
  unsigned int PicHeightInSamplesC{};
  unsigned int PicWidthInSamplesL{};
  unsigned int PicWidthInSamplesC{};
  unsigned int ChromaArrayType{};
  unsigned int CropUnitX{};
  unsigned int CropUnitY{};
  unsigned int PicCropLeftOffset{};
  int          PicCropWidth{};
  unsigned int PicCropTopOffset{};
  int          PicCropHeight{};
  bool         MbaffFrameFlag{};
  unsigned int MaxPicOrderCntLsb{};
  int          ExpectedDeltaPerPicOrderCntCycle{};
  unsigned int MaxFrameNum{};
};

} // namespace parser::av1
