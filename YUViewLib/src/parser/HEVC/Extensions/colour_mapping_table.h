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

#include "colour_mapping_octants.h"
#include "parser/common/SubByteReaderLogging.h"

namespace parser::hevc
{

// F.7.3.2.3.5 General colour mapping table syntax
class colour_mapping_table
{
public:
  colour_mapping_table() {}

  void parse(reader::SubByteReaderLogging &reader);

  unsigned               num_cm_ref_layers_minus1{};
  vector<unsigned>       cm_ref_layer_id;
  unsigned               cm_octant_depth{};
  unsigned               cm_y_part_num_log2{};
  unsigned               luma_bit_depth_cm_input_minus8{};
  unsigned               chroma_bit_depth_cm_input_minus8{};
  unsigned               luma_bit_depth_cm_output_minus8{};
  unsigned               chroma_bit_depth_cm_output_minus8{};
  unsigned               cm_res_quant_bits{};
  unsigned               cm_delta_flc_bits_minus1{};
  int                    cm_adapt_threshold_u_delta{};
  int                    cm_adapt_threshold_v_delta{};
  colour_mapping_octants colourMappingOctants{};
};

} // namespace parser::hevc
