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

#include "pps_3d_extension.h"

#include <parser/common/Functions.h>

namespace parser::hevc
{

using namespace reader;

void pps_3d_extension::parse(SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "pps_3d_extension()");

  this->dlts_present_flag = reader.readFlag("dlts_present_flag");
  if (this->dlts_present_flag)
  {
    this->pps_depth_layers_minus1 = reader.readBits("pps_depth_layers_minus1", 6);
    this->pps_bit_depth_for_depth_layers_minus8 =
        reader.readBits("pps_bit_depth_for_depth_layers_minus8", 4);

    const auto depthMaxValue = (1u << (pps_bit_depth_for_depth_layers_minus8 + 8)) - 1;

    for (unsigned i = 0; i <= this->pps_depth_layers_minus1; i++)
    {
      this->dlt_flag.push_back(reader.readFlag(formatArray("dlt_flag", i)));
      if (this->dlt_flag.at(i))
      {
        this->dlt_pred_flag.push_back(reader.readFlag(formatArray("dlt_pred_flag", i)));
        if (!this->dlt_pred_flag.at(i))
          this->dlt_val_flags_present_flag.push_back(
              reader.readFlag(formatArray("dlt_val_flags_present_flag", i)));
        else
          this->dlt_val_flags_present_flag.push_back(false);

        if (this->dlt_val_flags_present_flag.at(i))
        {
          dlt_value_flag.push_back({});
          for (unsigned j = 0; j <= depthMaxValue; j++)
            dlt_value_flag[i].push_back(reader.readFlag(formatArray("dlt_value_flag", i, j)));
        }
        else
          this->deltaDlt.parse(reader, this->pps_bit_depth_for_depth_layers_minus8);
      }
    }
  }
}

} // namespace parser::hevc
