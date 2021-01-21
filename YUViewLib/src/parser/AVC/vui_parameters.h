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

#include "hrd_parameters.h"
#include "parser/common/SubByteReaderLogging.h"


namespace parser::avc
{

class vui_parameters
{
public:
  vui_parameters() = default;

  void parse(reader::SubByteReaderLogging &reader,
             unsigned                      BitDepthC,
             unsigned                      BitDepthY,
             unsigned                      chroma_format_idc,
             bool                          frame_mbs_only_flag);

  bool         aspect_ratio_info_present_flag{};
  unsigned int aspect_ratio_idc{};
  unsigned int sar_width{};
  unsigned int sar_height{};

  bool overscan_info_present_flag{};
  bool overscan_appropriate_flag{};

  bool         video_signal_type_present_flag{};
  unsigned int video_format{5};
  bool         video_full_range_flag{};
  bool         colour_description_present_flag{};
  unsigned int colour_primaries{2};
  unsigned int transfer_characteristics{2};
  unsigned int matrix_coefficients{2};

  bool         chroma_loc_info_present_flag{};
  unsigned int chroma_sample_loc_type_top_field{};
  unsigned int chroma_sample_loc_type_bottom_field{};

  bool         timing_info_present_flag{};
  unsigned int num_units_in_tick{};
  unsigned int time_scale{};
  bool         fixed_frame_rate_flag{};

  hrd_parameters nalHrdParameters;
  hrd_parameters vclHrdParameters;

  bool         nal_hrd_parameters_present_flag{};
  bool         vcl_hrd_parameters_present_flag{};
  bool         low_delay_hrd_flag{};
  bool         pic_struct_present_flag{};
  bool         bitstream_restriction_flag{};
  bool         motion_vectors_over_pic_boundaries_flag{};
  unsigned int max_bytes_per_pic_denom{};
  unsigned int max_bits_per_mb_denom{};
  unsigned int log2_max_mv_length_horizontal{};
  unsigned int log2_max_mv_length_vertical{};
  unsigned int max_num_reorder_frames{};
  unsigned int max_dec_frame_buffering{};

  // The following values are not read from the bitstream but are calculated from the read values.
  double frameRate{};
};

} // namespace parser::av1
