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

#include "subpic_level_info.h"

namespace parser::vvc
{

using namespace parser::reader;

void subpic_level_info::parse(ReaderHelperNew &reader)
{
  ReaderHelperNewSubLevel subLevel(reader, "subpic_level_info");

  this->sli_num_ref_levels_minus1          = reader.readBits("sli_num_ref_levels_minus1", 3);
  this->sli_cbr_constraint_flag            = reader.readFlag("sli_cbr_constraint_flag");
  this->sli_explicit_fraction_present_flag = reader.readFlag("sli_explicit_fraction_present_flag");
  if (this->sli_explicit_fraction_present_flag)
  {
    this->sli_num_subpics_minus1 = reader.readUEV("sli_num_subpics_minus1");
  }
  this->sli_max_sublayers_minus1       = reader.readBits("sli_max_sublayers_minus1", 3);
  this->sli_sublayer_info_present_flag = reader.readFlag("sli_sublayer_info_present_flag");
  while (!reader.byte_aligned())
  {
    reader.readFlag("sli_alignment_zero_bit", Options().withCheckEqualTo(0));
  }
  for (unsigned k = this->sli_sublayer_info_present_flag ? 0 : this->sli_max_sublayers_minus1;
       k <= this->sli_max_sublayers_minus1;
       k++)
  {
    this->sli_non_subpic_layers_fraction.push_back({});
    this->sli_ref_level_idc.push_back({});
    this->sli_ref_level_fraction_minus1.push_back({});
    for (unsigned i = 0; i <= this->sli_num_ref_levels_minus1; i++)
    {
      this->sli_non_subpic_layers_fraction[k].push_back(
          reader.readBits("sli_non_subpic_layers_fraction", 8));
      this->sli_ref_level_idc[k].push_back(reader.readBits("sli_ref_level_idc", 8));
      this->sli_ref_level_fraction_minus1[k].push_back({});
      if (this->sli_explicit_fraction_present_flag)
      {
        for (unsigned j = 0; j <= this->sli_num_subpics_minus1; j++)
        {
          this->sli_ref_level_fraction_minus1[k][i].push_back(
              reader.readBits("sli_ref_level_fraction_minus1", 8));
        }
      }
    }
  }
}

} // namespace parser::vvc
