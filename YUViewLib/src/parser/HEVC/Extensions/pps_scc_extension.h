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

namespace parser::hevc
{

// 7.3.2.3.3 Picture parameter set screen content coding extension syntax
class pps_scc_extension
{
public:
  pps_scc_extension() {}

  void parse(reader::SubByteReaderLogging &reader);

  bool               pps_curr_pic_ref_enabled_flag{};
  bool               residual_adaptive_colour_transform_enabled_flag{};
  bool               pps_slice_act_qp_offsets_present_flag{};
  int                pps_act_y_qp_offset_plus5{};
  int                pps_act_cb_qp_offset_plus5{};
  int                pps_act_cr_qp_offset_plus3{};
  bool               pps_palette_predictor_initializers_present_flag{};
  unsigned           pps_num_palette_predictor_initializers{};
  bool               monochrome_palette_flag{};
  unsigned           luma_bit_depth_entry_minus8{};
  unsigned           chroma_bit_depth_entry_minus8{};
  vector2d<unsigned> pps_palette_predictor_initializer;
};

} // namespace parser::hevc
