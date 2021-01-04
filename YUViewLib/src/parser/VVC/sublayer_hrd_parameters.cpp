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

#include "sublayer_hrd_parameters.h"

#include "general_timing_hrd_parameters.h"

namespace parser::vvc
{

using namespace parser::reader;

void sublayer_hrd_parameters::parse(SubByteReaderLogging &              reader,
                                    unsigned                       subLayerId,
                                    general_timing_hrd_parameters *general_hrd)
{
  assert(general_hrd != nullptr);
  SubByteReaderLoggingSubLevel subLevel(reader, "sublayer_hrd_parameters");

  for (unsigned j = 0; j <= general_hrd->hrd_cpb_cnt_minus1; j++)
  {
    this->bit_rate_value_minus1[subLayerId][j] = reader.readUEV("bit_rate_value_minus1");
    this->cpb_size_value_minus1[subLayerId][j] = reader.readUEV("cpb_size_value_minus1");
    if (general_hrd->general_du_hrd_params_present_flag)
    {
      this->cpb_size_du_value_minus1[subLayerId][j] = reader.readUEV("cpb_size_du_value_minus1");
      this->bit_rate_du_value_minus1[subLayerId][j] = reader.readUEV("bit_rate_du_value_minus1");
    }
    this->cbr_flag[subLayerId][j] = reader.readFlag("cbr_flag");
  }
}

} // namespace parser::vvc
