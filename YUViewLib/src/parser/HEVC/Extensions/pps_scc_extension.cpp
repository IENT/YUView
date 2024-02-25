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

#include "pps_scc_extension.h"

#include <parser/common/Functions.h>

namespace parser::hevc
{

using namespace reader;

void pps_scc_extension::parse(SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "pps_scc_extension()");

  this->pps_curr_pic_ref_enabled_flag = reader.readFlag("pps_curr_pic_ref_enabled_flag");
  this->residual_adaptive_colour_transform_enabled_flag =
      reader.readFlag("residual_adaptive_colour_transform_enabled_flag");
  if (this->residual_adaptive_colour_transform_enabled_flag)
  {
    this->pps_slice_act_qp_offsets_present_flag =
        reader.readFlag("pps_slice_act_qp_offsets_present_flag");
    this->pps_act_y_qp_offset_plus5  = reader.readSEV("pps_act_y_qp_offset_plus5");
    this->pps_act_cb_qp_offset_plus5 = reader.readSEV("pps_act_cb_qp_offset_plus5");
    this->pps_act_cr_qp_offset_plus3 = reader.readSEV("pps_act_cr_qp_offset_plus3");
  }
  this->pps_palette_predictor_initializers_present_flag =
      reader.readFlag("pps_palette_predictor_initializers_present_flag");
  if (this->pps_palette_predictor_initializers_present_flag)
  {
    this->pps_num_palette_predictor_initializers =
        reader.readUEV("pps_num_palette_predictor_initializers");
    if (pps_num_palette_predictor_initializers > 0)
    {
      this->monochrome_palette_flag     = reader.readFlag("monochrome_palette_flag");
      this->luma_bit_depth_entry_minus8 = reader.readUEV("luma_bit_depth_entry_minus8");
      if (!this->monochrome_palette_flag)
        this->chroma_bit_depth_entry_minus8 = reader.readUEV("chroma_bit_depth_entry_minus8");
      const auto numComps = this->monochrome_palette_flag ? 1u : 3u;
      for (unsigned comp = 0; comp < numComps; comp++)
      {
        this->pps_palette_predictor_initializer.push_back({});
        const auto numBits = (comp == 0) ? this->luma_bit_depth_entry_minus8 + 8
                                         : this->chroma_bit_depth_entry_minus8 + 8;
        for (unsigned i = 0; i < this->pps_num_palette_predictor_initializers; i++)
        {
          pps_palette_predictor_initializer[comp].push_back(
              reader.readBits(formatArray("pps_palette_predictor_initializer", comp, i), numBits));
        }
      }
    }
  }
}

} // namespace parser::hevc
