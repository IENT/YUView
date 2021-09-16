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

namespace parser::hevc
{

using namespace reader;

void vui_parameters::parse(SubByteReaderLogging &reader, unsigned sps_max_sub_layers_minus1)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "vui_parameters()");

  this->aspect_ratio_info_present_flag = reader.readFlag("aspect_ratio_info_present_flag");
  if (this->aspect_ratio_info_present_flag)
  {
    this->aspect_ratio_idc = reader.readBits("aspect_ratio_idc",
                                             8,
                                             Options().withMeaningVector({"Unspecified",
                                                                          "1:1 (square)",
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
    if (this->aspect_ratio_idc == 255) // EXTENDED_SAR=255
    {
      this->sar_width  = reader.readBits("sar_width", 16);
      this->sar_height = reader.readBits("sar_height", 16);
    }
  }

  this->overscan_info_present_flag = reader.readFlag("overscan_info_present_flag");
  if (this->overscan_info_present_flag)
    this->overscan_appropriate_flag = reader.readFlag("overscan_appropriate_flag");

  this->video_signal_type_present_flag = reader.readFlag("video_signal_type_present_flag");
  if (video_signal_type_present_flag)
  {
    this->video_format =
        reader.readBits("video_format",
                        3,
                        Options()
                            .withMeaningVector({"Component", "PAL", "NTSC", "SECAM", "MAC"})
                            .withMeaning("Unspecified video format"));
    this->video_full_range_flag = reader.readFlag("video_full_range_flag");

    this->colour_description_present_flag = reader.readFlag("colour_description_present_flag");
    if (this->colour_description_present_flag)
    {
      this->colour_primaries = reader.readBits(
          "colour_primaries",
          8,
          Options()
              .withMeaningVector(
                  {"Reserved For future use by ITU-T | ISO/IEC",
                   "Rec. ITU-R BT.709-6 / BT.1361 / IEC 61966-2-1 (sRGB or sYCC)",
                   "Unspecified",
                   "Reserved For future use by ITU - T | ISO / IEC",
                   "Rec. ITU-R BT.470-6 System M (historical) (NTSC)",
                   "Rec. ITU-R BT.470-6 System B, G (historical) / BT.601 / BT.1358 / "
                   "BT.1700 PAL and 625 SECAM",
                   "Rec. ITU-R BT.601-6 525 / BT.1358 525 / BT.1700 NTSC",
                   "Society of Motion Picture and Television Engineers 240M (1999)",
                   "Generic film (colour filters using Illuminant C)",
                   "Rec. ITU-R BT.2020-2 Rec. ITU-R BT.2100-0",
                   "SMPTE ST 428-1 (CIE 1931 XYZ)",
                   "SMPTE RP 431-2 (2011)",
                   "SMPTE EG 432-1 (2010)",
                   "Reserved For future use by ITU - T | ISO / IEC",
                   "Reserved For future use by ITU - T | ISO / IEC",
                   "Reserved For future use by ITU - T | ISO / IEC",
                   "Reserved For future use by ITU - T | ISO / IEC",
                   "Reserved For future use by ITU - T | ISO / IEC",
                   "Reserved For future use by ITU - T | ISO / IEC",
                   "Reserved For future use by ITU - T | ISO / IEC",
                   "Reserved For future use by ITU - T | ISO / IEC",
                   "Reserved For future use by ITU - T | ISO / IEC",
                   "EBU Tech. 3213-E (1975)",
                   "Reserved For future use by ITU - T | ISO / IEC"})
              .withMeaning("Reserved"));
      this->transfer_characteristics = reader.readBits(
          "transfer_characteristics",
          8,
          Options()
              .withMeaningVector(
                  {"Reserved For future use by ITU-T | ISO/IEC",
                   "Rec. ITU-R BT.709-6 Rec.ITU - R BT.1361-0 conventional colour gamut system",
                   "Unspecified",
                   "Reserved For future use by ITU-T | ISO / IEC",
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
                   "Rec. ITU-R BT.2020-2 for 10 bit system",
                   "Rec. ITU-R BT.2020-2 for 12 bit system",
                   "SMPTE ST 2084 for 10, 12, 14 and 16-bit systems Rec. ITU-R BT.2100-0 "
                   "perceptual "
                   "quantization (PQ) system",
                   "SMPTE ST 428-1",
                   "Association of Radio Industries and Businesses (ARIB) STD-B67 Rec. ITU-R "
                   "BT.2100-0 "
                   "hybrid log-gamma (HLG) system"})
              .withMeaning("Reserved For future use by ITU-T | ISO/IEC"));
      this->matrix_coeffs = reader.readBits(
          "matrix_coeffs",
          8,
          Options()
              .withMeaningVector(
                  {"The identity matrix. RGB IEC 61966-2-1 (sRGB)",
                   "Rec. ITU-R BT.709-6, Rec. ITU-R BT.1361-0",
                   "Image characteristics are unknown or are determined by the application",
                   "For future use by ITU-T | ISO/IEC",
                   "United States Federal Communications Commission Title 47 Code of Federal "
                   "Regulations (2003) 73.682 (a) (20)",
                   "Rec. ITU-R BT.470-6 System B, G (historical), Rec. ITU-R BT.601-6 625, Rec. "
                   "ITU-R "
                   "BT.1358 625, Rec. ITU-R BT.1700 625 PAL and 625 SECAM",
                   "Rec. ITU-R BT.601-6 525, Rec. ITU-R BT.1358 525, Rec. ITU-R BT.1700 NTSC, "
                   "Society "
                   "of Motion Picture and Television Engineers 170M (2004)",
                   "Society of Motion Picture and Television Engineers 240M (1999)",
                   "YCgCo",
                   "Rec. ITU-R BT.2020-2 non-constant luminance system",
                   "Rec. ITU-R BT.2020-2 constant luminance system",
                   "SMPTE ST 2085 (2015)",
                   "Chromaticity-derived non-constant luminance system",
                   "Chromaticity-derived constant luminance system",
                   "Rec. ITU-R BT.2100-0 ICTCP",
                   "For future use by ITU-T | ISO/IEC"})
              .withMeaning("Reserved"));
    }
  }

  this->chroma_loc_info_present_flag = reader.readFlag("chroma_loc_info_present_flag");
  if (this->chroma_loc_info_present_flag)
  {
    auto meaningChromaLoc =
        std::vector<std::string>({"Left", "Center", "Top Left", "Top", "Bottom Left", "Bottom"});
    this->chroma_sample_loc_type_top_field =
        reader.readUEV("chroma_sample_loc_type_top_field",
                       Options().withMeaningVector(meaningChromaLoc).withCheckRange({0, 5}));
    this->chroma_sample_loc_type_bottom_field =
        reader.readUEV("chroma_sample_loc_type_bottom_field",
                       Options().withMeaningVector(meaningChromaLoc).withCheckRange({0, 5}));
  }

  this->neutral_chroma_indication_flag = reader.readFlag("neutral_chroma_indication_flag");
  this->field_seq_flag                 = reader.readFlag("field_seq_flag");
  this->frame_field_info_present_flag  = reader.readFlag("frame_field_info_present_flag");
  this->default_display_window_flag    = reader.readFlag("default_display_window_flag");
  if (this->default_display_window_flag)
  {
    this->def_disp_win_left_offset   = reader.readUEV("def_disp_win_left_offset");
    this->def_disp_win_right_offset  = reader.readUEV("def_disp_win_right_offset");
    this->def_disp_win_top_offset    = reader.readUEV("def_disp_win_top_offset");
    this->def_disp_win_bottom_offset = reader.readUEV("def_disp_win_bottom_offset");
  }

  this->vui_timing_info_present_flag = reader.readFlag("vui_timing_info_present_flag");
  if (this->vui_timing_info_present_flag)
  {
    // The VUI has timing information for us
    this->vui_num_units_in_tick = reader.readBits("vui_num_units_in_tick", 32);
    this->vui_time_scale        = reader.readBits("vui_time_scale", 32);

    this->frameRate = double(this->vui_time_scale) / double(this->vui_num_units_in_tick);
    reader.logArbitrary("frameRate", std::to_string(this->frameRate));

    this->vui_poc_proportional_to_timing_flag =
        reader.readFlag("vui_poc_proportional_to_timing_flag");
    if (this->vui_poc_proportional_to_timing_flag)
      this->vui_num_ticks_poc_diff_one_minus1 = reader.readUEV("vui_num_ticks_poc_diff_one_minus1");
    this->vui_hrd_parameters_present_flag = reader.readFlag("vui_hrd_parameters_present_flag");
    if (this->vui_hrd_parameters_present_flag)
      this->hrdParameters.parse(reader, 1, sps_max_sub_layers_minus1);
  }

  this->bitstream_restriction_flag = reader.readFlag("bitstream_restriction_flag");
  if (this->bitstream_restriction_flag)
  {
    this->tiles_fixed_structure_flag = reader.readFlag("tiles_fixed_structure_flag");
    this->motion_vectors_over_pic_boundaries_flag =
        reader.readFlag("motion_vectors_over_pic_boundaries_flag");
    this->restricted_ref_pic_lists_flag = reader.readFlag("restricted_ref_pic_lists_flag");
    this->min_spatial_segmentation_idc  = reader.readUEV("min_spatial_segmentation_idc");
    this->max_bytes_per_pic_denom       = reader.readUEV("max_bytes_per_pic_denom");
    this->max_bits_per_min_cu_denom     = reader.readUEV("max_bits_per_min_cu_denom");
    this->log2_max_mv_length_horizontal = reader.readUEV("log2_max_mv_length_horizontal");
    this->log2_max_mv_length_vertical   = reader.readUEV("log2_max_mv_length_vertical");
  }
}

} // namespace parser::hevc