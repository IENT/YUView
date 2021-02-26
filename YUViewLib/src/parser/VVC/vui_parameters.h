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

#include "parser/common/CodingEnum.h"
#include "parser/common/SubByteReaderLogging.h"

namespace parser::vvc
{

// Table 7 in T-REC-H.273-201612
static CodingEnum<Ratio> sampleAspectRatioCoding({{0, {0, 0}, "Unspecified"},
                                                  {1, {1, 1}, "1:1 (Square)"},
                                                  {2, {12, 11}, "12:11"},
                                                  {3, {10, 11}, "10:11"},
                                                  {4, {16, 11}, "16:11"},
                                                  {5, {40, 33}, "40:33"},
                                                  {6, {24, 11}, "24:11"},
                                                  {7, {20, 11}, "20:11"},
                                                  {8, {32, 11}, "32:11"},
                                                  {9, {80, 33}, "80:33"},
                                                  {10, {18, 11}, "18:11"},
                                                  {11, {15, 11}, "15:11"},
                                                  {12, {64, 33}, "64:33"},
                                                  {13, {160, 99}, "160:99"},
                                                  {14, {4, 3}, "4:3"},
                                                  {15, {3, 2}, "3:2"},
                                                  {16, {2, 1}, "2:1"},
                                                  {255, {0, 0}, "SarWidth:SarHeight (Custom)"}},
                                                 {0, 0});

class vui_parameters
{
public:
  vui_parameters()  = default;
  ~vui_parameters() = default;
  void parse(reader::SubByteReaderLogging &reader, unsigned payloadSize);

  bool vui_progressive_source_flag{};
  bool vui_interlaced_source_flag{};
  bool vui_non_packed_constraint_flag{};
  bool vui_non_projected_constraint_flag{};
  bool vui_aspect_ratio_info_present_flag{};

  bool     vui_aspect_ratio_constant_flag{};
  unsigned vui_aspect_ratio_idc{};
  unsigned vui_sar_width{};
  unsigned vui_sar_height{};

  bool     vui_overscan_info_present_flag{};
  bool     vui_overscan_appropriate_flag{};
  bool     vui_colour_description_present_flag{};
  unsigned vui_colour_primaries{};
  unsigned vui_transfer_characteristics{};
  unsigned vui_matrix_coeffs{};
  bool     vui_full_range_flag{};
  bool     vui_chroma_loc_info_present_flag{};

  uint64_t vui_chroma_sample_loc_type_frame{};
  uint64_t vui_chroma_sample_loc_type_top_field{};
  uint64_t vui_chroma_sample_loc_type_bottom_field{};
};

} // namespace parser::vvc
