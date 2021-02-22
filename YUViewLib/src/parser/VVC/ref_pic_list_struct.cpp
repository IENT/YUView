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

#include "parser/common/functions.h"
#include "seq_parameter_set_rbsp.h"

namespace parser::vvc
{

using namespace parser::reader;

void ref_pic_list_struct::parse(SubByteReaderLogging &                  reader,
                                unsigned                                listIdx,
                                unsigned                                rplsIdx,
                                std::shared_ptr<seq_parameter_set_rbsp> sps)
{
  assert(sps != nullptr);
  SubByteReaderLoggingSubLevel subLevel(reader,
                                        formatArray("ref_pic_list_struct", listIdx, rplsIdx));

  this->num_ref_entries = reader.readUEV("num_ref_entries");
  if (sps->sps_long_term_ref_pics_flag && rplsIdx < sps->sps_num_ref_pic_lists[listIdx] &&
      this->num_ref_entries > 0)
  {
    this->ltrp_in_header_flag = reader.readFlag("ltrp_in_header_flag");
  }
  unsigned j = 0;
  for (unsigned i = 0; i < this->num_ref_entries; i++)
  {
    if (sps->sps_inter_layer_prediction_enabled_flag)
    {
      this->inter_layer_ref_pic_flag[i] =
          reader.readFlag(formatArray("inter_layer_ref_pic_flag", i));
    }
    if (!this->inter_layer_ref_pic_flag[i])
    {
      if (sps->sps_long_term_ref_pics_flag)
      {
        this->st_ref_pic_flag[i] = reader.readFlag(formatArray("st_ref_pic_flag", i));
      }
      if (this->getStRefPicFlag(i))
      {
        this->abs_delta_poc_st[i] = reader.readUEV(formatArray("abs_delta_poc_st", i));

        // (149)
        if ((sps->sps_weighted_pred_flag || sps->sps_weighted_bipred_flag) && i != 0)
          this->AbsDeltaPocSt[i] = abs_delta_poc_st[i];
        else
          this->AbsDeltaPocSt[i] = abs_delta_poc_st[i] + 1;

        if (this->AbsDeltaPocSt[i])
        {
          this->strp_entry_sign_flag[i] = reader.readFlag(formatArray("strp_entry_sign_flag", i));
        }
      }
      else if (!this->ltrp_in_header_flag)
      {
        auto numBits               = sps->sps_log2_max_pic_order_cnt_lsb_minus4 + 4;
        this->rpls_poc_lsb_lt[j++] = reader.readBits(formatArray("rpls_poc_lsb_lt", i), numBits);
      }
    }
    else
    {
      this->ilrp_idx[i] = reader.readUEV(formatArray("ilrp_idx", i));
    }
  }

  // (148)
  this->NumLtrpEntries = 0;
  for (unsigned i = 0; i < this->num_ref_entries; i++)
    if (!this->inter_layer_ref_pic_flag[i] && !this->getStRefPicFlag(i))
      this->NumLtrpEntries++;

  // (150)
  for (unsigned i = 0; i < this->num_ref_entries; i++)
    if (!this->inter_layer_ref_pic_flag[i] && this->getStRefPicFlag(i))
      this->DeltaPocValSt[i] =
          (1 - 2 * int(this->strp_entry_sign_flag[i])) * this->AbsDeltaPocSt[i];
}

bool ref_pic_list_struct::getStRefPicFlag(unsigned i)
{
  // The default value of a non existent st_ref_pic_flag is true
  if (!this->inter_layer_ref_pic_flag[i] && this->st_ref_pic_flag.count(i) == 0)
    this->st_ref_pic_flag[i] = true;

  return this->st_ref_pic_flag[i];
}

} // namespace parser::vvc
