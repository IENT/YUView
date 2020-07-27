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

#include "SequenceHeader.h"

#include "parser/common/ParserMacros.h"

namespace AV1
{

constexpr auto SELECT_SCREEN_CONTENT_TOOLS = 2;
constexpr auto SELECT_INTEGER_MV = 2;

bool SequenceHeader::parse(const QByteArray &sequenceHeaderData, TreeItem *root)
{
  this->obuPayload = sequenceHeaderData;
  ReaderHelper reader(sequenceHeaderData, root, "sequence_header_obu()");

  QStringList seq_profile_meaning = QStringList()
    << "Main Profile: Bit depth 8 or 10 bit, Monochrome support, Subsampling YUV 4:2:0"
    << "High Profile: Bit depth 8 or 10 bit, No Monochrome support, Subsampling YUV 4:2:0 or 4:4:4"
    << "Professional Profile: Bit depth 8, 10 or 12 bit, Monochrome support, Subsampling YUV 4:2:0, 4:2:2 or 4:4:4"
    << "Reserved";
  READBITS_M(seq_profile, 3, seq_profile_meaning);
  READFLAG(still_picture);
  READFLAG(reduced_still_picture_header);

  if (reduced_still_picture_header) 
  {
    timing_info_present_flag = false;
    decoder_model_info_present_flag = false;
    initial_display_delay_present_flag = false;
    operating_points_cnt_minus_1 = 0;
    operating_point_idc.append(0);
    READBITS_A(seq_level_idx, 5, 0);
    seq_tier.append(false);
    decoder_model_present_for_this_op.append(false);
    initial_display_delay_present_for_this_op.append(false);
  }
  else
  {
    READFLAG(timing_info_present_flag);
    if (timing_info_present_flag)
    {
      if (!timing_info.parse_timing_info(reader))
        return false;
      READFLAG(decoder_model_info_present_flag);
      if (decoder_model_info_present_flag)
        if (!decoder_model_info.parse_decoder_model(reader))
          return false;
    }
    else
    {
      decoder_model_info_present_flag = false;
    }
    READFLAG(initial_display_delay_present_flag);
    READBITS(operating_points_cnt_minus_1, 5);
    for (unsigned int i = 0; i <= operating_points_cnt_minus_1; i++)
    {
      READBITS_A(operating_point_idc, 12, i);
      READBITS_A(seq_level_idx, 5, i);
      if (seq_level_idx[i] > 7)
      {
        READFLAG_A(seq_tier, i);
      }
      else 
      {
        seq_tier.append(false);
      }
      if (decoder_model_info_present_flag)
      {
        READFLAG_A(decoder_model_present_for_this_op, i);
        if (decoder_model_present_for_this_op[i])
        {
          if (!operating_parameters_info.parse_operating_parameters_info(reader, i, decoder_model_info))
            return false;
        }
      }
      else
      {
        decoder_model_present_for_this_op.append(0);
      }
      if (initial_display_delay_present_flag)
      {
        READFLAG_A(initial_display_delay_present_for_this_op, i);
        if (initial_display_delay_present_for_this_op[i])
        {
          READBITS_A(initial_display_delay_minus_1, 4, i);
        }
      }
    }
  }

  operatingPoint = choose_operating_point();
  OperatingPointIdc = operating_point_idc[operatingPoint];

  READBITS(frame_width_bits_minus_1, 4);
  READBITS(frame_height_bits_minus_1, 4);
  READBITS(max_frame_width_minus_1, frame_width_bits_minus_1+1);
  READBITS(max_frame_height_minus_1, frame_height_bits_minus_1+1);
  if (reduced_still_picture_header)
    frame_id_numbers_present_flag = 0;
  else
    READFLAG(frame_id_numbers_present_flag);
  if (frame_id_numbers_present_flag)
  {
    READBITS(delta_frame_id_length_minus_2, 4);
    READBITS(additional_frame_id_length_minus_1, 3);
  }
  READFLAG(use_128x128_superblock);
  READFLAG(enable_filter_intra);
  READFLAG(enable_intra_edge_filter);

  if (reduced_still_picture_header)
  {
    enable_interintra_compound = false;
    enable_masked_compound = false;
    enable_warped_motion = false;
    enable_dual_filter = false;
    enable_order_hint = false;
    enable_jnt_comp = false;
    enable_ref_frame_mvs = false;
    seq_force_screen_content_tools = SELECT_SCREEN_CONTENT_TOOLS;
    seq_force_integer_mv = SELECT_INTEGER_MV;
    OrderHintBits = 0;
  }
  else
  {
    READFLAG(enable_interintra_compound);
    READFLAG(enable_masked_compound);
    READFLAG(enable_warped_motion);
    READFLAG(enable_dual_filter);
    READFLAG(enable_order_hint);
    if (enable_order_hint)
    {
      READFLAG(enable_jnt_comp);
      READFLAG(enable_ref_frame_mvs);
    }
    else
    {
      enable_jnt_comp = false;
      enable_ref_frame_mvs = false;
    }
    READFLAG(seq_choose_screen_content_tools);
    if (seq_choose_screen_content_tools)
    {
      seq_force_screen_content_tools = SELECT_SCREEN_CONTENT_TOOLS;
      LOGVAL(seq_force_screen_content_tools);
    }
    else
      READBITS(seq_force_screen_content_tools, 1);
    
    if (seq_force_screen_content_tools > 0)
    {
      READFLAG(seq_choose_integer_mv);
      if (seq_choose_integer_mv)
      {
        seq_force_integer_mv = SELECT_INTEGER_MV;
        LOGVAL(seq_force_integer_mv);
      }
      else
        READBITS(seq_force_integer_mv, 1);
    }
    else
    {
      seq_force_integer_mv = SELECT_INTEGER_MV;
      LOGVAL(seq_force_integer_mv);
    }
    if (enable_order_hint)
    {
      READBITS(order_hint_bits_minus_1, 3);
      OrderHintBits = order_hint_bits_minus_1 + 1;
    }
    else
    {
      OrderHintBits = 0;
    }
    LOGVAL(OrderHintBits);
  }

  READFLAG(enable_superres);
  READFLAG(enable_cdef);
  READFLAG(enable_restoration);

  if (!color_config.parse_color_config(reader, seq_profile))
    return false;

  READFLAG(film_grain_params_present);

  // Calculate the RFC6381 'Codecs' parameter as it is needed in DASH
  {
    // We add the parameters from back to front. The last parameters can be omitted if they are the default values
    QString rfc6381CodecsParameter, rfc6381CodecsParameterShortened;
    bool add = false;

    // videoFullRangeFlag (1 digit)
    QString range = (color_config.color_range ? ".1" : ".0");
    rfc6381CodecsParameter.prepend(range);
    add |= (range != ".0");
    if (add)
      rfc6381CodecsParameterShortened.prepend(range);

    // matrixCoefficients	(2 digits)
    QString matrix = (color_config.color_description_present_flag ? QString(".%1").arg(color_config.matrix_coefficients, 2, 10, QChar('0')) : ".01");
    rfc6381CodecsParameter.prepend(matrix);
    add |= (matrix != ".01");
    if (add)
      rfc6381CodecsParameterShortened.prepend(matrix);

    // transfer_characteristics (2 digits)
    QString transfer = (color_config.color_description_present_flag ? QString(".%1").arg(color_config.transfer_characteristics, 2, 10, QChar('0')) : ".01");
    rfc6381CodecsParameter.prepend(transfer);
    add |= (transfer != ".01");
    if (add)
      rfc6381CodecsParameterShortened.prepend(transfer);

    // color_primaries (2 digits)
    QString primaries = (color_config.color_description_present_flag ? QString(".%1").arg(color_config.color_primaries, 2, 10, QChar('0')) : ".01");
    rfc6381CodecsParameter.prepend(primaries);
    add |= (primaries != ".01");
    if (add)
      rfc6381CodecsParameterShortened.prepend(primaries);

    // chromaSubsampling (3 digits)
    QString subsampling = QString(".%1%2%3").arg(color_config.subsampling_x ? "1" : "0").arg(color_config.subsampling_y ? "1" : "0").arg(color_config.subsampling_x && color_config.subsampling_y ? color_config.chroma_sample_position : 0);
    rfc6381CodecsParameter.prepend(subsampling);
    add |= (subsampling != ".110");
    if (add)
      rfc6381CodecsParameterShortened.prepend(subsampling);

    // Monochrome (1 digit)
    QString monochrome = QString(".%1").arg(color_config.mono_chrome ? "1" : "0");
    rfc6381CodecsParameter.prepend(monochrome);
    add |= (monochrome != ".0");
    if (add)
      rfc6381CodecsParameterShortened.prepend(monochrome);

    // From here, all values are mandatory
    QString remainder = QString("av01.%1.%2%3.%4").arg(seq_profile).arg(seq_level_idx[0], 2, 10, QChar('0')).arg(seq_tier[0] ? "H" : "M").arg(color_config.BitDepth, 2, 10, QChar('0'));
    rfc6381CodecsParameter.prepend(remainder);
    rfc6381CodecsParameterShortened.prepend(remainder);
    
    LOGVAL(rfc6381CodecsParameter);
    LOGVAL(rfc6381CodecsParameterShortened);
  }

  return true;
}

bool SequenceHeader::timing_info_struct::parse_timing_info(ReaderHelper &reader)
{
  ReaderSubLevel r(reader, "timing_info()");

  READBITS(num_units_in_display_tick, 32);
  READBITS(time_scale, 32);
  READFLAG(equal_picture_interval);
  if (equal_picture_interval)
    READUVLC(num_ticks_per_picture_minus_1);

  return true;
}

bool SequenceHeader::decoder_model_info_struct::parse_decoder_model(ReaderHelper &reader)
{
  ReaderSubLevel r(reader, "decoder_model_info()");

  READBITS(buffer_delay_length_minus_1, 5);
  READBITS(num_units_in_decoding_tick, 32);
  READBITS(buffer_removal_time_length_minus_1, 5);
  READBITS(frame_presentation_time_length_minus_1, 5);

  return true;
}

bool SequenceHeader::operating_parameters_info_struct::parse_operating_parameters_info(ReaderHelper &reader, int op, decoder_model_info_struct &dmodel)
{
  ReaderSubLevel r(reader, "operating_parameters_info()");

  int n = dmodel.buffer_delay_length_minus_1 + 1;
  READBITS_A(decoder_buffer_delay, n, op);
  READBITS_A(encoder_buffer_delay, n, op);
  READFLAG_A(low_delay_mode_flag, op);

  return false;
}

bool SequenceHeader::color_config_struct::parse_color_config(ReaderHelper &reader, int seq_profile)
{
  ReaderSubLevel r(reader, "color_config()");

  READFLAG(high_bitdepth);
  if (seq_profile == 2 && high_bitdepth) 
  {
    READFLAG(twelve_bit);
    BitDepth = twelve_bit ? 12 : 10;
  }
  else if (seq_profile <= 2) 
  {
    BitDepth = high_bitdepth ? 10 : 8;
  }
  LOGVAL(BitDepth);

  if (seq_profile == 1) 
    mono_chrome = 0;
  else 
    READFLAG(mono_chrome);

  NumPlanes = mono_chrome ? 1 : 3;
  LOGVAL(NumPlanes);

  READFLAG(color_description_present_flag);
  if (color_description_present_flag) 
  {
    QStringList color_primaries_meaning = QStringList()
      << "Unused"
      << "BT.709" 
      << "Unspecified"
      << "Unused"
      << "BT.470 System M (historical)"
      << "BT.470 System B, G (historical)"
      << "BT.601"
      << "SMPTE 240"
      << "Generic film (color filters using illuminant C)"
      << "BT.2020, BT.2100"
      << "SMPTE 428 (CIE 1921 XYZ)"
      << "SMPTE RP 431-2"
      << "SMPTE EG 432-1"
      << "Unused" << "Unused" << "Unused" << "Unused" << "Unused" << "Unused" << "Unused" << "Unused" << "Unused"
      << "EBU Tech. 3213-E"
      << "Unused";
    READBITS_M_E(color_primaries, 8, color_primaries_meaning, color_primaries_enum);

    QStringList transfer_characteristics_meaning = QStringList()
      << "For future use"
      << "BT.709"
      << "Unspecified"
      << "For future use"
      << "BT.470 System M (historical)"
      << "BT.470 System B, G (historical)"
      << "BT.601"
      << "SMPTE 240 M"
      << "Linear"
      << "Logarithmic (100 : 1 range)"
      << "Logarithmic (100 * Sqrt(10) : 1 range)"
      << "IEC 61966-2-4"
      << "BT.1361"
      << "sRGB or sYCC"
      << "BT.2020 10-bit systems"
      << "BT.2020 12-bit systems"
      << "SMPTE ST 2084, ITU BT.2100 PQ"
      << "SMPTE ST 428"
      << "BT.2100 HLG, ARIB STD-B67"
      << "Unused";
    READBITS_M_E(transfer_characteristics, 8, transfer_characteristics_meaning, transfer_characteristics_enum);

    QStringList matrix_coefficients_meaning = QStringList()
      << "Identity matrix"
      << "BT.709"
      << "Unspecified"
      << "For future use"
      << "US FCC 73.628"
      << "BT.470 System B, G (historical)"
      << "BT.601"
      << "SMPTE 240 M"
      << "YCgCo"
      << "BT.2020 non-constant luminance, BT.2100 YCbCr"
      << "BT.2020 constant luminance"
      << "SMPTE ST 2085 YDzDx"
      << "Chromaticity-derived non-constant luminance"
      << "Chromaticity-derived constant luminance"
      << "BT.2100 ICtCp"
      << "Unused";
    READBITS_M_E(matrix_coefficients, 8, matrix_coefficients_meaning, matrix_coefficients_enum);
  }
  else
  {
    color_primaries = CP_UNSPECIFIED;
    transfer_characteristics = TC_UNSPECIFIED;
    matrix_coefficients = MC_UNSPECIFIED;
  }

  if (mono_chrome)
  {
    READFLAG(color_range);
    subsampling_x = true;
    subsampling_y = true;
    chroma_sample_position = CSP_UNKNOWN;
    separate_uv_delta_q = false;
  }
  else if (color_primaries == CP_BT_709 && transfer_characteristics == TC_SRGB && matrix_coefficients == MC_IDENTITY)
  {
    color_range = 1;
    subsampling_x = false;
    subsampling_y = false;
  }
  else
  {
    READFLAG(color_range);
    if (seq_profile == 0) 
    {
      subsampling_x = true;
      subsampling_y = true;
    } 
    else if (seq_profile == 1) 
    {
      subsampling_x = false;
      subsampling_y = false;
    }
    else
    {
      if (BitDepth == 12) 
      {
        READFLAG(subsampling_x);
        if (subsampling_x)
          READFLAG(subsampling_y);
        else
          subsampling_y = false;
      } 
      else 
      {
        subsampling_x = true;
        subsampling_y = false;
      }
    }
    if (subsampling_x && subsampling_y)
    {
      QStringList chroma_sample_position_meaning = QStringList()
        << "Unknown (in this case the source video transfer function must be signaled outside the AV1 bitstream)"
        << "Horizontally co-located with (0, 0) luma sample, vertical position in the middle between two luma samples"
        << "co-located with (0, 0) luma sample"
        << "Reserved";
      READBITS_M_E(chroma_sample_position, 2, chroma_sample_position_meaning, chroma_sample_position_enum);
    }
  }
  if (!mono_chrome)
    READFLAG(separate_uv_delta_q);

  QString subsamplingFormat = "Unknown";
  if (subsampling_x && subsampling_y)
    subsamplingFormat = "4:2:0";
  else if (subsampling_x && !subsampling_y)
    subsamplingFormat = "4:2:2";
  else if (!subsampling_x && !subsampling_y)
    subsamplingFormat = "4:4:4";
  LOGSTRVAL("Subsampling format", subsamplingFormat);

  return true;
}

} // namespace AV1