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

#include "OBU.h"
#include "parser/common/ReaderHelper.h"

namespace AV1
{

struct SequenceHeader : OBU
{
  SequenceHeader(const OBU &obu) : OBU(obu) {};
  bool parse(const QByteArray &sequenceHeaderData, TreeItem *root);

  unsigned int seq_profile;
  bool still_picture;
  bool reduced_still_picture_header;

  bool timing_info_present_flag;
  bool decoder_model_info_present_flag;
  bool initial_display_delay_present_flag;
  unsigned int operating_points_cnt_minus_1;
  QList<unsigned int> operating_point_idc;
  QList<unsigned int> seq_level_idx;
  QList<bool> seq_tier;
  QList<bool> decoder_model_present_for_this_op;
  QList<bool> initial_display_delay_present_for_this_op;
  QList<unsigned int> initial_display_delay_minus_1;

  struct timing_info_struct
  {
    bool parse_timing_info(ReaderHelper &reader);

    unsigned int num_units_in_display_tick;
    unsigned int time_scale;
    bool equal_picture_interval;
    uint64_t num_ticks_per_picture_minus_1;
  };
  timing_info_struct timing_info;

  struct decoder_model_info_struct
  {
    bool parse_decoder_model(ReaderHelper &reader);

    unsigned int buffer_delay_length_minus_1;
    unsigned int num_units_in_decoding_tick;
    unsigned int buffer_removal_time_length_minus_1;
    unsigned int frame_presentation_time_length_minus_1;
  };
  decoder_model_info_struct decoder_model_info;

  struct operating_parameters_info_struct
  {
    bool parse_operating_parameters_info(ReaderHelper &reader, int op, decoder_model_info_struct &dmodel);

    QList<unsigned int> decoder_buffer_delay;
    QList<unsigned int> encoder_buffer_delay;
    QList<bool> low_delay_mode_flag;
  };
  operating_parameters_info_struct operating_parameters_info;

  int operatingPoint;
  int choose_operating_point() { return 0; }
  int OperatingPointIdc;

  unsigned int frame_width_bits_minus_1;
  unsigned int frame_height_bits_minus_1;
  unsigned int max_frame_width_minus_1;
  unsigned int max_frame_height_minus_1;
  bool frame_id_numbers_present_flag;
  unsigned int delta_frame_id_length_minus_2;
  unsigned int additional_frame_id_length_minus_1;
  bool use_128x128_superblock;
  bool enable_filter_intra;
  bool enable_intra_edge_filter;

  bool enable_interintra_compound;
  bool enable_masked_compound;
  bool enable_warped_motion;
  bool enable_dual_filter;
  bool enable_order_hint;
  bool enable_jnt_comp;
  bool enable_ref_frame_mvs;
  bool seq_choose_screen_content_tools;
  unsigned int seq_force_screen_content_tools;
  unsigned int seq_force_integer_mv;
  bool seq_choose_integer_mv;
  unsigned int order_hint_bits_minus_1;
  unsigned int OrderHintBits;

  bool enable_superres;
  bool enable_cdef;
  bool enable_restoration;

  struct color_config_struct
  {
    bool parse_color_config(ReaderHelper &reader, int seq_profile);

    bool high_bitdepth;
    bool twelve_bit;
    int BitDepth;
    bool mono_chrome;
    int NumPlanes;
    bool color_description_present_flag;

    enum color_primaries_enum
    {
      CP_BT_709 = 1,
      CP_UNSPECIFIED,
      CP_BT_470_M = 4,
      CP_BT_470_B_G,
      CP_BT_601,
      CP_SMPTE_240,
      CP_GENERIC_FILM,
      CP_BT_2020,
      CP_XYZ,
      CP_SMPTE_431,
      CP_SMPTE_432,
      CP_EBU_3213 = 22
    };
    color_primaries_enum color_primaries;

    enum transfer_characteristics_enum
    {
      TC_RESERVED_0 = 0,
      TC_BT_709,
      TC_UNSPECIFIED,
      TC_RESERVED_3,
      TC_BT_470_M,
      TC_BT_470_B_G,
      TC_BT_601,
      TC_SMPTE_240,
      TC_LINEAR,
      TC_LOG_100,
      TC_LOG_100_SQRT10,
      TC_IEC_61966,
      TC_BT_1361,
      TC_SRGB,
      TC_BT_2020_10_BIT,
      TC_BT_2020_12_BIT,
      TC_SMPTE_2084,
      TC_SMPTE_428,
      TC_HLG
    };
    transfer_characteristics_enum transfer_characteristics;

    enum matrix_coefficients_enum
    {
      MC_IDENTITY = 0,
      MC_BT_709,
      MC_UNSPECIFIED,
      MC_RESERVED_3,
      MC_FCC,
      MC_BT_470_B_G,
      MC_BT_601,
      MC_SMPTE_240,
      MC_SMPTE_YCGCO,
      MC_BT_2020_NCL,
      MC_BT_2020_CL,
      MC_SMPTE_2085,
      MC_CHROMAT_NCL,
      MC_CHROMAT_CL,
      MC_ICTCP
    };
    matrix_coefficients_enum matrix_coefficients;

    bool color_range;
    bool subsampling_x;
    bool subsampling_y;

    enum chroma_sample_position_enum
    {
      CSP_UNKNOWN = 0,
      CSP_VERTICAL,
      CSP_COLOCATED,
      CSP_RESERVED
    };
    chroma_sample_position_enum chroma_sample_position;

    bool separate_uv_delta_q;
  };
  color_config_struct color_config;

  bool film_grain_params_present;
};

} // namespace AV1