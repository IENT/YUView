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

#include "colour_mapping_table.h"

#include <parser/common/Functions.h>

namespace parser::hevc
{

using namespace reader;

void colour_mapping_table::parse(SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "colour_mapping_table()");

  this->num_cm_ref_layers_minus1 = reader.readUEV("num_cm_ref_layers_minus1");
  for (unsigned i = 0; i <= this->num_cm_ref_layers_minus1; i++)
    this->cm_ref_layer_id.push_back(reader.readBits(formatArray("cm_ref_layer_id", i), 6));
  this->cm_octant_depth                   = reader.readBits("cm_octant_depth", 2);
  this->cm_y_part_num_log2                = reader.readBits("cm_y_part_num_log2", 2);
  this->luma_bit_depth_cm_input_minus8    = reader.readUEV("luma_bit_depth_cm_input_minus8");
  this->chroma_bit_depth_cm_input_minus8  = reader.readUEV("chroma_bit_depth_cm_input_minus8");
  this->luma_bit_depth_cm_output_minus8   = reader.readUEV("luma_bit_depth_cm_output_minus8");
  this->chroma_bit_depth_cm_output_minus8 = reader.readUEV("chroma_bit_depth_cm_output_minus8");
  this->cm_res_quant_bits                 = reader.readBits("cm_res_quant_bits", 2);
  this->cm_delta_flc_bits_minus1          = reader.readBits("cm_delta_flc_bits_minus1", 2);
  if (this->cm_octant_depth == 1)
  {
    this->cm_adapt_threshold_u_delta = reader.readSEV("cm_adapt_threshold_u_delta");
    this->cm_adapt_threshold_v_delta = reader.readSEV("cm_adapt_threshold_v_delta");
  }

  const auto BitDepthCmInputY  = 8 + this->luma_bit_depth_cm_input_minus8;
  const auto BitDepthCmOutputY = 8 + luma_bit_depth_cm_output_minus8;
  const auto CMResLSBits =
      std::max(0u,
               (10 + BitDepthCmInputY - BitDepthCmOutputY - this->cm_res_quant_bits -
                (this->cm_delta_flc_bits_minus1 + 1)));

  this->colourMappingOctants.parse(reader,
                                   this->cm_octant_depth,
                                   cm_y_part_num_log2,
                                   CMResLSBits,
                                   0,
                                   0,
                                   0,
                                   0,
                                   1 << this->cm_octant_depth);
}

} // namespace parser::hevc
