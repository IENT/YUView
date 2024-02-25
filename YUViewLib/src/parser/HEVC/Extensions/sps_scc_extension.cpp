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

#include "sps_scc_extension.h"

#include <parser/common/Functions.h>

namespace parser::hevc
{

using namespace reader;

void sps_scc_extension::parse(SubByteReaderLogging &reader,
                              const unsigned        chroma_format_idc,
                              const unsigned        bitDepthLuma,
                              const unsigned        bitDepthChroma)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "sps_scc_extension()");

  this->sps_curr_pic_ref_enabled_flag = reader.readFlag("sps_curr_pic_ref_enabled_flag");
  this->palette_mode_enabled_flag     = reader.readFlag("palette_mode_enabled_flag");
  if (this->palette_mode_enabled_flag)
  {
    this->palette_max_size                 = reader.readUEV("palette_max_size");
    this->delta_palette_max_predictor_size = reader.readUEV("delta_palette_max_predictor_size");
    this->sps_palette_predictor_initializers_present_flag =
        reader.readFlag("sps_palette_predictor_initializers_present_flag");
    if (this->sps_palette_predictor_initializers_present_flag)
    {
      this->sps_num_palette_predictor_initializers_minus1 =
          reader.readUEV("sps_num_palette_predictor_initializers_minus1");
      const auto numComps = (chroma_format_idc == 0) ? 1 : 3;
      for (int comp = 0; comp < numComps; comp++)
      {
        for (int i = 0; i <= this->sps_num_palette_predictor_initializers_minus1; i++)
        {
          // There is an issue in the HEVC spec here. It is missing the number of bits that are used
          // to code this symbol. From the SCC reference software I was able to deduce that it
          // should be equal to the number for bit_depth signaled in the SPS for the component.
          const int numBits = (comp == 0) ? bitDepthLuma : bitDepthChroma;

          sps_palette_predictor_initializer[comp][i] =
              reader.readBits(formatArray("sps_palette_predictor_initializer", comp, i), numBits);
        }
      }
    }
  }
  this->motion_vector_resolution_control_idc =
      reader.readBits("motion_vector_resolution_control_idc", 2);
  this->intra_boundary_filtering_disabled_flag =
      reader.readFlag("intra_boundary_filtering_disabled_flag");
}

} // namespace parser::hevc
