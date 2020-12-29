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

#include "ref_pic_list_struct.h"

#include "seq_parameter_set_rbsp.h"

namespace parser::vvc
{

using namespace parser::reader;

void ref_pic_list_struct::parse(ReaderHelperNew &       reader,
                                unsigned                listIdx,
                                unsigned                rplsIdx,
                                seq_parameter_set_rbsp *sps)
{
  assert(sps != nullptr);
  ReaderHelperNewSubLevel subLevel(reader, "ref_pic_list_struct");

  this->num_ref_entries[listIdx][rplsIdx] = reader.readUEV("num_ref_entries");
  if (sps->sps_long_term_ref_pics_flag && rplsIdx < sps->sps_num_ref_pic_lists[listIdx] &&
      this->num_ref_entries[listIdx][rplsIdx] > 0)
  {
    this->ltrp_in_header_flag[listIdx][rplsIdx] = reader.readFlag("ltrp_in_header_flag");
  }
  unsigned j = 0;
  for (unsigned i = 0; i < this->num_ref_entries[listIdx][rplsIdx]; i++)
  {
    if (sps->sps_inter_layer_prediction_enabled_flag)
    {
      this->inter_layer_ref_pic_flag[listIdx][rplsIdx][i] =
          reader.readFlag("inter_layer_ref_pic_flag");
    }
    if (!this->inter_layer_ref_pic_flag[listIdx][rplsIdx][i])
    {
      if (sps->sps_long_term_ref_pics_flag)
      {
        this->st_ref_pic_flag[listIdx][rplsIdx][i] = reader.readFlag("st_ref_pic_flag");
      }
      if (!this->st_ref_pic_flag[listIdx][rplsIdx][i])
      {
        this->abs_delta_poc_st[listIdx][rplsIdx][i] = reader.readUEV("abs_delta_poc_st");

        // (149)
        if ((sps->sps_weighted_pred_flag || sps->sps_weighted_bipred_flag) && i != 0)
          this->AbsDeltaPocSt[listIdx][rplsIdx][i] = abs_delta_poc_st[listIdx][rplsIdx][i];
        else
          this->AbsDeltaPocSt[listIdx][rplsIdx][i] = abs_delta_poc_st[listIdx][rplsIdx][i] + 1;

        if (this->AbsDeltaPocSt[listIdx][rplsIdx][i])
        {
          this->strp_entry_sign_flag[listIdx][rplsIdx][i] = reader.readFlag("strp_entry_sign_flag");
        }
      }
      else if (!this->ltrp_in_header_flag[listIdx][rplsIdx])
      {
        auto numBits = sps->sps_log2_max_pic_order_cnt_lsb_minus4 + 4;
        this->rpls_poc_lsb_lt[listIdx][rplsIdx][j++] = reader.readBits("rpls_poc_lsb_lt", numBits);
      }
    }
    else
    {
      this->ilrp_idx[listIdx][rplsIdx][i] = reader.readUEV("ilrp_idx");
    }
  }

  // (150)
  for (unsigned i = 0; i < this->num_ref_entries[listIdx][rplsIdx]; i++)
    if (!this->inter_layer_ref_pic_flag[listIdx][rplsIdx][i] &&
        this->st_ref_pic_flag[listIdx][rplsIdx][i])
      this->DeltaPocValSt[listIdx][rplsIdx][i] =
          (1 - 2 * int(this->strp_entry_sign_flag[listIdx][rplsIdx][i])) *
          this->AbsDeltaPocSt[listIdx][rplsIdx][i];
}

} // namespace parser::vvc
