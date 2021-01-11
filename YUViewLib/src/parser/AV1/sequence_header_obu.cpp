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

#include "sequence_header_obu.h"

#include "parser/common/MeaningEnum.h"
#include "parser/common/functions.h"

#define SELECT_SCREEN_CONTENT_TOOLS 2
#define SELECT_INTEGER_MV 2

namespace parser::av1
{

using namespace reader;

void sequence_header_obu::parse(reader::SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "sequence_header_obu()");

  this->seq_profile = reader.readBits(
      "seq_profile",
      3,
      Options().withMeaningVector(
          {"Main Profile: Bit depth 8 or 10 bit, Monochrome support, Subsampling YUV 4:2:0",
           "High Profile: Bit depth 8 or 10 bit, No Monochrome support, Subsampling YUV 4:2:0 or "
           "4:4:4",
           "Professional Profile: Bit depth 8, 10 or 12 bit, Monochrome support, Subsampling YUV "
           "4:2:0, 4:2:2 or 4:4:4",
           "Reserved"}));
  this->still_picture                = reader.readFlag("still_picture");
  this->reduced_still_picture_header = reader.readFlag("reduced_still_picture_header");

  if (this->reduced_still_picture_header)
  {
    this->timing_info_present_flag           = false;
    this->decoder_model_info_present_flag    = false;
    this->initial_display_delay_present_flag = false;
    this->operating_points_cnt_minus_1       = 0;
    this->operating_point_idc.push_back(0);
    this->seq_level_idx.push_back(reader.readBits("seq_level_idx[0]", 5));
    this->seq_tier.push_back(false);
    this->decoder_model_present_for_this_op.push_back(false);
    this->initial_display_delay_present_for_this_op.push_back(false);
  }
  else
  {
    this->timing_info_present_flag = reader.readFlag("timing_info_present_flag");
    if (this->timing_info_present_flag)
    {
      this->timing_info.parse(reader);
      this->decoder_model_info_present_flag = reader.readFlag("decoder_model_info_present_flag");
      if (this->decoder_model_info_present_flag)
        this->decoder_model_info.parse(reader);
    }
    else
    {
      this->decoder_model_info_present_flag = false;
    }
    this->initial_display_delay_present_flag =
        reader.readFlag("initial_display_delay_present_flag");
    this->operating_points_cnt_minus_1 = reader.readBits("operating_points_cnt_minus_1", 5);
    for (unsigned int i = 0; i <= operating_points_cnt_minus_1; i++)
    {
      this->operating_point_idc.push_back(
          reader.readBits(formatArray("operating_point_idc", i), 12));
      this->seq_level_idx.push_back(reader.readBits(formatArray("seq_level_idx", i), 5));
      if (seq_level_idx[i] > 7)
      {
        this->seq_tier.push_back(reader.readFlag(formatArray("seq_tier", i)));
      }
      else
      {
        this->seq_tier.push_back(false);
      }
      if (this->decoder_model_info_present_flag)
      {
        this->decoder_model_present_for_this_op.push_back(
            reader.readFlag(formatArray("decoder_model_present_for_this_op", i)));
        if (this->decoder_model_present_for_this_op[i])
          operating_parameters_info.parse(reader, i, decoder_model_info);
      }
      else
      {
        this->decoder_model_present_for_this_op.push_back(0);
      }
      if (this->initial_display_delay_present_flag)
      {
        this->initial_display_delay_present_for_this_op.push_back(
            reader.readFlag(formatArray("initial_display_delay_present_for_this_op", i)));
        if (this->initial_display_delay_present_for_this_op[i])
        {
          this->initial_display_delay_minus_1.push_back(
              reader.readBits(formatArray("initial_display_delay_minus_1", i), 4));
        }
      }
    }
  }

  operatingPoint    = choose_operating_point();
  OperatingPointIdc = operating_point_idc[operatingPoint];

  this->frame_width_bits_minus_1  = reader.readBits("frame_width_bits_minus_1", 4);
  this->frame_height_bits_minus_1 = reader.readBits("frame_height_bits_minus_1", 4);
  this->max_frame_width_minus_1 =
      reader.readBits("max_frame_width_minus_1", frame_width_bits_minus_1 + 1);
  this->max_frame_height_minus_1 =
      reader.readBits("max_frame_height_minus_1", frame_height_bits_minus_1 + 1);
  if (reduced_still_picture_header)
    frame_id_numbers_present_flag = 0;
  else
    this->frame_id_numbers_present_flag = reader.readFlag("frame_id_numbers_present_flag");
  if (this->frame_id_numbers_present_flag)
  {
    this->delta_frame_id_length_minus_2 = reader.readBits("delta_frame_id_length_minus_2", 4);
    this->additional_frame_id_length_minus_1 =
        reader.readBits("additional_frame_id_length_minus_1", 3);
  }
  this->use_128x128_superblock   = reader.readFlag("use_128x128_superblock");
  this->enable_filter_intra      = reader.readFlag("enable_filter_intra");
  this->enable_intra_edge_filter = reader.readFlag("enable_intra_edge_filter");

  if (this->reduced_still_picture_header)
  {
    this->enable_interintra_compound     = false;
    this->enable_masked_compound         = false;
    this->enable_warped_motion           = false;
    this->enable_dual_filter             = false;
    this->enable_order_hint              = false;
    this->enable_jnt_comp                = false;
    this->enable_ref_frame_mvs           = false;
    this->seq_force_screen_content_tools = SELECT_SCREEN_CONTENT_TOOLS;
    this->seq_force_integer_mv           = SELECT_INTEGER_MV;
    this->OrderHintBits                  = 0;
  }
  else
  {
    this->enable_interintra_compound = reader.readFlag("enable_interintra_compound");
    this->enable_masked_compound     = reader.readFlag("enable_masked_compound");
    this->enable_warped_motion       = reader.readFlag("enable_warped_motion");
    this->enable_dual_filter         = reader.readFlag("enable_dual_filter");
    this->enable_order_hint          = reader.readFlag("enable_order_hint");
    if (this->enable_order_hint)
    {
      this->enable_jnt_comp      = reader.readFlag("enable_jnt_comp");
      this->enable_ref_frame_mvs = reader.readFlag("enable_ref_frame_mvs");
    }
    else
    {
      this->enable_jnt_comp      = false;
      this->enable_ref_frame_mvs = false;
    }
    this->seq_choose_screen_content_tools = reader.readFlag("seq_choose_screen_content_tools");
    if (this->seq_choose_screen_content_tools)
    {
      this->seq_force_screen_content_tools = SELECT_SCREEN_CONTENT_TOOLS;
      reader.logCalculatedValue("seq_force_screen_content_tools",
                                int64_t(seq_force_screen_content_tools));
    }
    else
      this->seq_force_screen_content_tools = reader.readBits("seq_force_screen_content_tools", 1);

    if (seq_force_screen_content_tools > 0)
    {
      this->seq_choose_integer_mv = reader.readFlag("seq_choose_integer_mv");
      if (this->seq_choose_integer_mv)
      {
        this->seq_force_integer_mv = SELECT_INTEGER_MV;
        reader.logCalculatedValue("seq_force_integer_mv", int64_t(seq_force_integer_mv));
      }
      else
        this->seq_force_integer_mv = reader.readBits("seq_force_integer_mv", 1);
    }
    else
    {
      seq_force_integer_mv = SELECT_INTEGER_MV;
      reader.logCalculatedValue("seq_force_integer_mv", int64_t(seq_force_integer_mv));
    }
    if (this->enable_order_hint)
    {
      this->order_hint_bits_minus_1 = reader.readBits("order_hint_bits_minus_1", 3);
      this->OrderHintBits           = order_hint_bits_minus_1 + 1;
    }
    else
    {
      this->OrderHintBits = 0;
    }
    reader.logCalculatedValue("OrderHintBits", int64_t(this->OrderHintBits));
  }

  this->enable_superres    = reader.readFlag("enable_superres");
  this->enable_cdef        = reader.readFlag("enable_cdef");
  this->enable_restoration = reader.readFlag("enable_restoration");

  this->color_config.parse(reader, seq_profile);

  this->film_grain_params_present = reader.readFlag("film_grain_params_present");

  // Calculate the RFC6381 'Codecs' parameter as it is needed in DASH
  {
    // We add the parameters from back to front. The last parameters can be omitted if they are the
    // default values
    std::string rfc6381CodecsParameter, rfc6381CodecsParameterShortened;
    bool        add = false;

    auto toStringPadded2 = [](unsigned idx) {
      auto s = std::to_string(idx);
      if (s.length() != 2)
        return "0" + s;
      return s;
    };

    // videoFullRangeFlag (1 digit)
    auto range             = (this->color_config.color_range ? ".1" : ".0");
    rfc6381CodecsParameter = range + rfc6381CodecsParameter;
    add |= (range != ".0");
    if (add)
      rfc6381CodecsParameterShortened = range + rfc6381CodecsParameterShortened;

    // matrixCoefficients	(2 digits)
    auto matrix            = (this->color_config.color_description_present_flag
                                  ? toStringPadded2(to_int(color_config.matrix_coefficients))
                                  : ".01");
    rfc6381CodecsParameter = matrix + rfc6381CodecsParameter;
    add |= (matrix != ".01");
    if (add)
      rfc6381CodecsParameterShortened = matrix + rfc6381CodecsParameterShortened;

    // transfer_characteristics (2 digits)
    auto transfer          = (this->color_config.color_description_present_flag
                                  ? toStringPadded2(to_int(color_config.transfer_characteristics))
                                  : ".01");
    rfc6381CodecsParameter = transfer + rfc6381CodecsParameter;
    add |= (transfer != ".01");
    if (add)
      rfc6381CodecsParameterShortened = transfer + rfc6381CodecsParameterShortened;

    // color_primaries (2 digits)
    auto primaries         = (this->color_config.color_description_present_flag
                                  ? toStringPadded2(to_int(color_config.color_primaries))
                                  : ".01");
    rfc6381CodecsParameter = primaries + rfc6381CodecsParameter;
    add |= (primaries != ".01");
    if (add)
      rfc6381CodecsParameterShortened = primaries + rfc6381CodecsParameterShortened;

    // chromaSubsampling (3 digits)
    auto subsampling = std::string((color_config.subsampling_x ? "1" : "0")) +
                       (color_config.subsampling_y ? "1" : "0") +
                       (color_config.subsampling_x && color_config.subsampling_y
                            ? toStringPadded2(to_int(color_config.chroma_sample_position))
                            : "0");
    rfc6381CodecsParameter = subsampling + rfc6381CodecsParameter;
    add |= (subsampling != ".110");
    if (add)
      rfc6381CodecsParameterShortened = subsampling + rfc6381CodecsParameterShortened;

    // Monochrome (1 digit)
    auto monochrome        = (color_config.mono_chrome ? "1" : "0");
    rfc6381CodecsParameter = monochrome + rfc6381CodecsParameter;
    add |= (monochrome != ".0");
    if (add)
      rfc6381CodecsParameterShortened = monochrome + rfc6381CodecsParameterShortened;

    // From here, all values are mandatory
    auto remainder = std::to_string(seq_profile) + toStringPadded2(this->seq_level_idx[0]) +
                     (seq_tier[0] ? "H" : "M") + toStringPadded2(this->color_config.BitDepth);
    rfc6381CodecsParameter          = remainder + rfc6381CodecsParameter;
    rfc6381CodecsParameterShortened = remainder + rfc6381CodecsParameterShortened;

    reader.logArbitrary("rfc6381CodecsParameter", rfc6381CodecsParameter);
    reader.logArbitrary("rfc6381CodecsParameterShortened", rfc6381CodecsParameterShortened);
  }
}

void sequence_header_obu::timing_info_struct::parse(SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "timing_info()");

  this->num_units_in_display_tick = reader.readBits("num_units_in_display_tick", 32);
  this->time_scale                = reader.readBits("time_scale", 32);
  this->equal_picture_interval    = reader.readFlag("equal_picture_interval");
  if (equal_picture_interval)
    this->num_ticks_per_picture_minus_1 = reader.readUEV("num_ticks_per_picture_minus_1");
}

void sequence_header_obu::decoder_model_info_struct::parse(SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "decoder_model_info()");

  this->buffer_delay_length_minus_1 = reader.readBits("buffer_delay_length_minus_1", 5);
  this->num_units_in_decoding_tick  = reader.readBits("num_units_in_decoding_tick", 32);
  this->buffer_removal_time_length_minus_1 =
      reader.readBits("buffer_removal_time_length_minus_1", 5);
  this->frame_presentation_time_length_minus_1 =
      reader.readBits("frame_presentation_time_length_minus_1", 5);
}

void sequence_header_obu::operating_parameters_info_struct::parse(
    SubByteReaderLogging &reader, int idx, sequence_header_obu::decoder_model_info_struct &dmodel)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "operating_parameters_info()");

  auto nrBits = dmodel.buffer_delay_length_minus_1 + 1;
  this->decoder_buffer_delay.push_back(
      reader.readBits(formatArray("decoder_buffer_delay", idx), nrBits));
  this->encoder_buffer_delay.push_back(
      reader.readBits(formatArray("encoder_buffer_delay", idx), nrBits));
  this->low_delay_mode_flag.push_back(reader.readFlag(formatArray("low_delay_mode_flag", idx)));
}

} // namespace parser::av1