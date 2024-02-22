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

#include "delta_dlt.h"

#include <parser/common/Functions.h>

#include <cmath>

namespace parser::hevc
{

using namespace reader;

void delta_dlt::parse(SubByteReaderLogging &reader,
                      const unsigned        pps_bit_depth_for_depth_layers_minus8)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "delta_dlt()");

  const auto numBits      = pps_bit_depth_for_depth_layers_minus8 + 8;
  this->num_val_delta_dlt = reader.readBits("num_val_delta_dlt", numBits);
  if (this->num_val_delta_dlt > 0)
  {
    if (num_val_delta_dlt > 1)
      this->max_diff = reader.readBits("max_diff", numBits);
    if (this->num_val_delta_dlt > 2 && this->max_diff > 0)
    {
      const auto numBitsDiff = std::ceil(std::log2(this->max_diff + 1));
      this->min_diff_minus1  = reader.readBits("min_diff_minus1", numBitsDiff);
    }
    this->delta_dlt_val0 = reader.readBits("delta_dlt_val0", numBits);
    if (this->max_diff > (this->min_diff_minus1 + 1))
    {
      const auto minDiff     = (this->min_diff_minus1 + 1);
      const auto numBitsDiff = std::ceil(std::log2(this->max_diff - minDiff + 1));
      for (unsigned k = 1; k < num_val_delta_dlt; k++)
        this->delta_val_diff_minus_min.push_back(
            reader.readBits(formatArray("delta_val_diff_minus_min", k), numBitsDiff));
    }
  }
}

} // namespace parser::hevc
