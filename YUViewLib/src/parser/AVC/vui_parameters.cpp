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

#include "vui_parameters.h"

#include "typedef.h"

namespace parser::av1

{
using namespace reader;

void vui_parameters::parse(reader::SubByteReaderLogging &reader,
                           unsigned                      BitDepthC,
                           unsigned                      BitDepthY,
                           unsigned                      chroma_format_idc,
                           bool                          frame_mbs_only_flag)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "vui_parameters()");

  this->aspect_ratio_info_present_flag = reader.readFlag("aspect_ratio_info_present_flag");
  if (aspect_ratio_info_present_flag)
  {
    this->aspect_ratio_idc = reader.readBits("aspect_ratio_idc",
                                             8,
                                             Options().withMeaningVector({"1:1 (square)",
                                                                          "12:11",
                                                                          "10:11",
                                                                          "16:11",
                                                                          "40:33",
                                                                          "24:11",
                                                                          "20:11",
                                                                          "32:11",
                                                                          "80:33",
                                                                          "18:11",
                                                                          "15:11",
                                                                          "64:33",
                                                                          "160:99",
                                                                          "4:3",
                                                                          "3:2",
                                                                          "2:1",
                                                                          "Reserved"}));
    if (aspect_ratio_idc == 255) // Extended_SAR
    {
      this->sar_width  = reader.readBits("sar_width", 16);
      this->sar_height = reader.readBits("sar_height", 16);
    }
  }
  this->overscan_info_present_flag = reader.readFlag("overscan_info_present_flag");
  if (this->overscan_info_present_flag)
    this->overscan_appropriate_flag = reader.readFlag("overscan_appropriate_flag");
  this->video_signal_type_present_flag = reader.readFlag("video_signal_type_present_flag");
  if (this->video_signal_type_present_flag)
  {
    this->video_format = reader.readBits(
        "video_format",
        3,
        Options().withMeaningVector(
            {"Component", "PAL", "NTSC", "SECAM", "MAC", "Unspecified video format", "Reserved"}));
    this->video_full_range_flag           = reader.readFlag("video_full_range_flag");
    this->colour_description_present_flag = reader.readFlag("colour_description_present_flag");
    if (this->colour_description_present_flag)
    {
      this->colour_primaries =
          reader.readBits("colour_primaries",
                          8,
                          Options().withMeaningVector(
                              {"Reserved For future use by ITU-T | ISO/IEC",
                               "Rec. ITU-R BT.709-5 / BT.1361 / IEC 61966-2-1 (sRGB or sYCC)",
                               "Unspecified",
                               "Reserved For future use by ITU - T | ISO / IEC",
                               "Rec. ITU-R BT.470-6 System M (historical) (NTSC)",
                               "Rec. ITU-R BT.470-6 System B, G (historical) / BT.601 / BT.1358 / "
                               "BT.1700 PAL and 625 SECAM",
                               "Rec. ITU-R BT.601-6 525 / BT.1358 525 / BT.1700 NTSC",
                               "Society of Motion Picture and Television Engineers 240M (1999)",
                               "Generic film (colour filters using Illuminant C)",
                               "Rec. ITU-R BT.2020",
                               "Reserved For future use by ITU-T | ISO/IEC"}));

      this->transfer_characteristics = reader.readBits(
          "transfer_characteristics",
          8,
          Options().withMeaningVector(
              {"Reserved For future use by ITU-T | ISO/IEC",
               "Rec. ITU-R BT.709-5 Rec.ITU - R BT.1361 conventional colour gamut system",
               "Unspecified",
               "Reserved For future use by ITU - T | ISO / IEC",
               "Rec. ITU-R BT.470-6 System M (historical) (NTSC)",
               "Rec. ITU-R BT.470-6 System B, G (historical)",
               "Rec. ITU-R BT.601-6 525 or 625, Rec.ITU - R BT.1358 525 or 625, Rec.ITU - R "
               "BT.1700 NTSC Society of Motion Picture and Television Engineers 170M(2004)",
               "Society of Motion Picture and Television Engineers 240M (1999)",
               "Linear transfer characteristics",
               "Logarithmic transfer characteristic (100:1 range)",
               "Logarithmic transfer characteristic (100 * Sqrt( 10 ) : 1 range)",
               "IEC 61966-2-4",
               "Rec. ITU-R BT.1361 extended colour gamut system",
               "IEC 61966-2-1 (sRGB or sYCC)",
               "Rec. ITU-R BT.2020 for 10 bit system",
               "Rec. ITU-R BT.2020 for 12 bit system",
               "Reserved For future use by ITU-T | ISO/IEC"}));

      this->matrix_coefficients = reader.readBits(
          "matrix_coefficients",
          8,
          Options().withMeaningVector(
              {"RGB IEC 61966-2-1 (sRGB)",
               "Rec. ITU-R BT.709-5, Rec. ITU-R BT.1361",
               "Image characteristics are unknown or are determined by the application",
               "For future use by ITU-T | ISO/IEC",
               "United States Federal Communications Commission Title 47 Code of Federal "
               "Regulations (2003) 73.682 (a) (20)",
               "Rec. ITU-R BT.470-6 System B, G (historical), Rec. ITU-R BT.601-6 625, Rec. ITU-R "
               "BT.1358 625, Rec. ITU-R BT.1700 625 PAL and 625 SECAM",
               "Rec. ITU-R BT.601-6 525, Rec. ITU-R BT.1358 525, Rec. ITU-R BT.1700 NTSC, Society "
               "of Motion Picture and Television Engineers 170M (2004)",
               "Society of Motion Picture and Television Engineers 240M (1999)",
               "YCgCo",
               "Rec. ITU-R BT.2020 non-constant luminance system",
               "Rec. ITU-R BT.2020 constant luminance system",
               "For future use by ITU-T | ISO/IEC"}));
      if ((BitDepthC != BitDepthY || chroma_format_idc != 3) && matrix_coefficients == 0)
        throw std::logic_error(
            "matrix_coefficients shall not be equal to 0 unless both of the following conditions "
            "are true: 1 BitDepthC is equal to BitDepthY, 2 chroma_format_idc is equal to 3 "
            "(4:4:4).");
    }
  }
  this->chroma_loc_info_present_flag = reader.readFlag("chroma_loc_info_present_flag");
  if (chroma_format_idc != 1 && !chroma_loc_info_present_flag)
    throw std::logic_error("When chroma_format_idc is not equal to 1, "
                           "chroma_loc_info_present_flag should be equal to 0.");
  if (chroma_loc_info_present_flag)
  {
    this->chroma_sample_loc_type_top_field = reader.readUEV("chroma_sample_loc_type_top_field");
    this->chroma_sample_loc_type_bottom_field =
        reader.readUEV("chroma_sample_loc_type_bottom_field");
    if (chroma_sample_loc_type_top_field > 5 || chroma_sample_loc_type_bottom_field > 5)
      throw std::logic_error(
          "The value of chroma_sample_loc_type_top_field and chroma_sample_loc_type_bottom_field "
          "shall be in the range of 0 to 5, inclusive.");
  }
  this->timing_info_present_flag = reader.readFlag("timing_info_present_flag");
  if (timing_info_present_flag)
  {
    this->num_units_in_tick     = reader.readBits("num_units_in_tick", 32);
    this->time_scale            = reader.readBits("time_scale", 32, Options().withCheckGreater(0));
    this->fixed_frame_rate_flag = reader.readFlag("fixed_frame_rate_flag");

    // TODO: This is definitely not correct. num_units_in_tick and time_scale just define the
    // minimal
    //       time interval that can be represented in the bitstream
    this->frameRate = (double)time_scale / (double)num_units_in_tick;
    if (frame_mbs_only_flag)
      this->frameRate /= 2.0;
    reader.logCalculatedValue("frameRate", this->frameRate);
  }

  this->nal_hrd_parameters_present_flag = reader.readFlag("nal_hrd_parameters_present_flag");
  if (this->nal_hrd_parameters_present_flag)
    this->nalHrdParameters.parse(reader);
  this->vcl_hrd_parameters_present_flag = reader.readFlag("vcl_hrd_parameters_present_flag");
  if (this->vcl_hrd_parameters_present_flag)
    this->vclHrdParameters.parse(reader);

  if (this->nal_hrd_parameters_present_flag && this->vcl_hrd_parameters_present_flag)
  {
    if (nalHrdParameters.initial_cpb_removal_delay_length_minus1 !=
        vclHrdParameters.initial_cpb_removal_delay_length_minus1)
      throw std::logic_error(
          "initial_cpb_removal_delay_length_minus1 and initial_cpb_removal_delay_length_minus1 "
          "shall be equal.");
    if (nalHrdParameters.cpb_removal_delay_length_minus1 !=
        vclHrdParameters.cpb_removal_delay_length_minus1)
      throw std::logic_error(
          "cpb_removal_delay_length_minus1 and cpb_removal_delay_length_minus1 shall be equal.");
    if (nalHrdParameters.dpb_output_delay_length_minus1 !=
        vclHrdParameters.dpb_output_delay_length_minus1)
      throw std::logic_error(
          "dpb_output_delay_length_minus1 and dpb_output_delay_length_minus1 shall be equal.");
    if (nalHrdParameters.time_offset_length != vclHrdParameters.time_offset_length)
      throw std::logic_error("time_offset_length and time_offset_length shall be equal.");
  }

  if (this->nal_hrd_parameters_present_flag || this->vcl_hrd_parameters_present_flag)
    this->low_delay_hrd_flag = reader.readFlag("low_delay_hrd_flag");
  this->pic_struct_present_flag    = reader.readFlag("pic_struct_present_flag");
  this->bitstream_restriction_flag = reader.readFlag("bitstream_restriction_flag");
  if (this->bitstream_restriction_flag)
  {
    this->motion_vectors_over_pic_boundaries_flag =
        reader.readFlag("motion_vectors_over_pic_boundaries_flag");
    this->max_bytes_per_pic_denom       = reader.readUEV("max_bytes_per_pic_denom");
    this->max_bits_per_mb_denom         = reader.readUEV("max_bits_per_mb_denom");
    this->log2_max_mv_length_horizontal = reader.readUEV("log2_max_mv_length_horizontal");
    this->log2_max_mv_length_vertical   = reader.readUEV("log2_max_mv_length_vertical");
    this->max_num_reorder_frames        = reader.readUEV("max_num_reorder_frames");
    this->max_dec_frame_buffering       = reader.readUEV("max_dec_frame_buffering");
  }
}

} // namespace parser::av1