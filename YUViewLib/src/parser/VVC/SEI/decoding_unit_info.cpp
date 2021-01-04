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

#include "decoding_unit_info.h"

#include "buffering_period.h"

namespace parser::vvc
{

using namespace parser::reader;

void decoding_unit_info::parse(SubByteReaderLogging &                 reader,
                               unsigned                          nalTemporalID,
                               std::shared_ptr<buffering_period> bp)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "decoding_unit_info");

  this->dui_decoding_unit_idx = reader.readUEV("dui_decoding_unit_idx");
  if (!bp->bp_du_cpb_params_in_pic_timing_sei_flag)
  {
    auto TemporalId = nalTemporalID;
    for (unsigned i = TemporalId; i <= bp->bp_max_sublayers_minus1; i++)
    {
      if (i < bp->bp_max_sublayers_minus1)
      {
        this->dui_sublayer_delays_present_flag.push_back(
            reader.readFlag("dui_sublayer_delays_present_flag"));
      }
      if (this->dui_sublayer_delays_present_flag[i])
      {
        auto nrBits = bp->bp_du_cpb_removal_delay_increment_length_minus1 + 1;
        this->dui_du_cpb_removal_delay_increment =
            reader.readBits("dui_du_cpb_removal_delay_increment", nrBits);
      }
    }
  }
  if (!bp->bp_du_dpb_params_in_pic_timing_sei_flag)
  {
    this->dui_dpb_output_du_delay_present_flag =
        reader.readFlag("dui_dpb_output_du_delay_present_flag");
  }
  if (this->dui_dpb_output_du_delay_present_flag)
  {
    auto nrBits = bp->bp_dpb_output_delay_du_length_minus1 + 1;
    this->dui_dpb_output_du_delay = reader.readBits("dui_dpb_output_du_delay", nrBits);
  }
}

} // namespace parser::vvc
