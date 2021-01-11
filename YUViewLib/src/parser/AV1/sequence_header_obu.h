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

#include "OpenBitstreamUnit.h"
#include "color_config.h"
#include "parser/common/SubByteReaderLogging.h"

namespace parser::av1
{

class sequence_header_obu : public ObuPayload
{
public:
  sequence_header_obu() = default;
  void parse(reader::SubByteReaderLogging &reader);

  unsigned seq_profile{};
  bool     still_picture{};
  bool     reduced_still_picture_header{};

  bool             timing_info_present_flag{};
  bool             decoder_model_info_present_flag{};
  bool             initial_display_delay_present_flag{};
  unsigned         operating_points_cnt_minus_1{};
  vector<unsigned> operating_point_idc;
  vector<unsigned> seq_level_idx;
  vector<bool>     seq_tier;
  vector<bool>     decoder_model_present_for_this_op;
  vector<bool>     initial_display_delay_present_for_this_op;
  vector<unsigned> initial_display_delay_minus_1;

  struct timing_info_struct
  {
    void parse(reader::SubByteReaderLogging &reader);

    unsigned num_units_in_display_tick{};
    unsigned time_scale{};
    bool     equal_picture_interval{};
    uint64_t num_ticks_per_picture_minus_1{};
  };
  timing_info_struct timing_info;

  struct decoder_model_info_struct
  {
    void parse(reader::SubByteReaderLogging &reader);

    unsigned buffer_delay_length_minus_1{};
    unsigned num_units_in_decoding_tick{};
    unsigned buffer_removal_time_length_minus_1{};
    unsigned frame_presentation_time_length_minus_1{};
  };
  decoder_model_info_struct decoder_model_info;

  struct operating_parameters_info_struct
  {
    void parse(reader::SubByteReaderLogging &reader, int idx, decoder_model_info_struct &dmodel);

    vector<unsigned> decoder_buffer_delay;
    vector<unsigned> encoder_buffer_delay;
    vector<bool>     low_delay_mode_flag;
  };
  operating_parameters_info_struct operating_parameters_info;

  int operatingPoint{};
  int choose_operating_point() { return 0; }
  int OperatingPointIdc{};

  unsigned frame_width_bits_minus_1{};
  unsigned frame_height_bits_minus_1{};
  unsigned max_frame_width_minus_1{};
  unsigned max_frame_height_minus_1{};
  bool     frame_id_numbers_present_flag{};
  unsigned delta_frame_id_length_minus_2{};
  unsigned additional_frame_id_length_minus_1{};
  bool     use_128x128_superblock{};
  bool     enable_filter_intra{};
  bool     enable_intra_edge_filter{};

  bool     enable_interintra_compound{};
  bool     enable_masked_compound{};
  bool     enable_warped_motion{};
  bool     enable_dual_filter{};
  bool     enable_order_hint{};
  bool     enable_jnt_comp{};
  bool     enable_ref_frame_mvs{};
  bool     seq_choose_screen_content_tools{};
  unsigned seq_force_screen_content_tools{};
  unsigned seq_force_integer_mv{};
  bool     seq_choose_integer_mv{};
  unsigned order_hint_bits_minus_1{};
  unsigned OrderHintBits{};

  bool enable_superres{};
  bool enable_cdef{};
  bool enable_restoration{};

  Color_config color_config;

  bool film_grain_params_present{};
};

} // namespace parser::av1
