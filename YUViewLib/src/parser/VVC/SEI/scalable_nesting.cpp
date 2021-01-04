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

#include "scalable_nesting.h"

#include "sei_message.h"

namespace parser::vvc
{

using namespace parser::reader;

void scalable_nesting::parse(ReaderHelperNew &                 reader,
                             NalType                           nal_unit_type,
                             unsigned                          nalTemporalID,
                             std::shared_ptr<buffering_period> lastBufferingPeriod)
{
  ReaderHelperNewSubLevel subLevel(reader, "scalable_nesting");

  this->sn_ols_flag    = reader.readFlag("sn_ols_flag");
  this->sn_subpic_flag = reader.readFlag("sn_subpic_flag");
  if (this->sn_ols_flag)
  {
    this->sn_num_olss_minus1 = reader.readUEV("sn_num_olss_minus1");
    for (unsigned i = 0; i <= sn_num_olss_minus1; i++)
    {
      this->sn_ols_idx_delta_minus1.push_back(reader.readUEV("sn_ols_idx_delta_minus1"));
    }
  }
  else
  {
    this->sn_all_layers_flag = reader.readFlag("sn_all_layers_flag");
    if (!this->sn_all_layers_flag)
    {
      this->sn_num_layers_minus1 = reader.readUEV("sn_num_layers_minus1");
      for (unsigned i = 1; i <= sn_num_layers_minus1; i++)
      {
        this->sn_layer_id.push_back(reader.readBits("sn_layer_id", 6));
      }
    }
  }
  if (this->sn_subpic_flag)
  {
    this->sn_num_subpics_minus1   = reader.readUEV("sn_num_subpics_minus1");
    this->sn_subpic_id_len_minus1 = reader.readUEV("sn_subpic_id_len_minus1");
    for (unsigned i = 0; i <= sn_num_subpics_minus1; i++)
    {
      auto nrBits        = this->sn_subpic_id_len_minus1 + 1;
      this->sn_subpic_id = reader.readBits("sn_subpic_id", nrBits);
    }
  }
  this->sn_num_seis_minus1 = reader.readUEV("sn_num_seis_minus1");
  while (!reader.byte_aligned())
  {
    reader.readFlag("sn_zero_bit", Options().withCheckEqualTo(0));
  }
  for (unsigned i = 0; i <= sn_num_seis_minus1; i++)
  {
    auto newSEI = std::make_shared<sei_message>();
    newSEI->parse(reader,
                  nal_unit_type,
                  nalTemporalID,
                  lastBufferingPeriod);

    this->nested_sei_messages.push_back(newSEI);
  }
}

} // namespace parser::vvc
