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

#include "colour_mapping_octants.h"

#include <parser/common/Functions.h>

namespace parser::hevc
{

using namespace reader;

void colour_mapping_octants::parse(SubByteReaderLogging &reader,
                                   const unsigned        cm_octant_depth,
                                   const unsigned        cm_y_part_num_log2,
                                   const unsigned        CMResLSBits,
                                   const unsigned        inpDepth,
                                   const unsigned        idxY,
                                   const unsigned        idxCb,
                                   const unsigned        idxCr,
                                   const unsigned        inpLength)
{
  std::string subLevelName = "colour_mapping_octants(";
  subLevelName += "inpDepth=" + std::to_string(inpDepth);
  subLevelName += ", idxY=" + std::to_string(idxY);
  subLevelName += ", idxCb=" + std::to_string(idxCb);
  subLevelName += ", idxCr=" + std::to_string(idxCr);
  subLevelName += ", inpLength=" + std::to_string(inpLength);
  subLevelName += ")";

  SubByteReaderLoggingSubLevel subLevel(reader, subLevelName);

  // F.7.4.3.3.5 (F-43)
  const auto PartNumY = 1u << cm_y_part_num_log2;

  if (inpDepth < cm_octant_depth)
    this->split_octant_flag = reader.readFlag("split_octant_flag");
  if (this->split_octant_flag)
  {
    for (int k = 0; k < 2; k++)
    {
      for (int m = 0; m < 2; m++)
      {
        for (int n = 0; n < 2; n++)
        {
          this->colourMappingOctants = std::make_unique<array3d<colour_mapping_octants, 2, 2, 2>>();
          (*this->colourMappingOctants)[k][m][n].parse(reader,
                                                       cm_octant_depth,
                                                       cm_y_part_num_log2,
                                                       CMResLSBits,
                                                       inpDepth + 1,
                                                       idxY + PartNumY * k * inpLength / 2,
                                                       idxCb + m * inpLength / 2,
                                                       idxCr + n * inpLength / 2,
                                                       inpLength / 2);
        }
      }
    }
  }
  else
  {
    for (unsigned i = 0; i < PartNumY; i++)
    {
      const auto idxShiftY = idxY + (i << (cm_octant_depth - inpDepth));
      for (unsigned j = 0; j < 4; j++)
      {
        this->coded_res_flag[idxShiftY][idxCb][idxCr][j] =
            reader.readFlag(formatArray("coded_res_flag", idxShiftY, idxCb, idxCr, j));
        if (this->coded_res_flag[idxShiftY][idxCb][idxCr][j])
        {
          for (unsigned c = 0; c < 3; c++)
          {
            this->res_coeff_q[idxShiftY][idxCb][idxCr][j][c] =
                reader.readUEV(formatArray("res_coeff_q", idxShiftY, idxCb, idxCr, j, c));
            if (CMResLSBits == 0)
              this->res_coeff_r[idxShiftY][idxCb][idxCr][j][c] = 0;
            else
              this->res_coeff_r[idxShiftY][idxCb][idxCr][j][c] = reader.readBits(
                  formatArray("res_coeff_r", idxShiftY, idxCb, idxCr, j, c), CMResLSBits);
            if (this->res_coeff_q[idxShiftY][idxCb][idxCr][j][c] ||
                this->res_coeff_r[idxShiftY][idxCb][idxCr][j][c])
              this->res_coeff_s[idxShiftY][idxCb][idxCr][j][c] =
                  reader.readFlag(formatArray("res_coeff_s", idxShiftY, idxCb, idxCr, j, c));
          }
        }
      }
    }
  }
}

} // namespace parser::hevc
