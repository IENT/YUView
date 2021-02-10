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

#include "ref_pic_lists.h"

#include "parser/common/functions.h"
#include "pic_parameter_set_rbsp.h"
#include "seq_parameter_set_rbsp.h"

#include <cmath>

namespace parser::vvc
{

using namespace parser::reader;

void ref_pic_lists::parse(SubByteReaderLogging &                  reader,
                          std::shared_ptr<seq_parameter_set_rbsp> sps,
                          std::shared_ptr<pic_parameter_set_rbsp> pps)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "ref_pic_lists");

  for (unsigned i = 0; i < 2; i++)
  {
    this->delta_poc_msb_cycle_present_flag.push_back({});
    if (sps->sps_num_ref_pic_lists[i] > 0 && (i == 0 || (i == 1 && pps->pps_rpl1_idx_present_flag)))
    {
      this->rpl_sps_flag[i] = reader.readFlag(formatArray("rpl_sps_flag", i));
    }
    else
    {
      // 7.40.10 If rpl_sps_flag is not present
      if (sps->sps_num_ref_pic_lists[i] == 0)
      {
        this->rpl_sps_flag[i] = 0;
      }
      else if (!pps->pps_rpl1_idx_present_flag && i == 1)
      {
        this->rpl_sps_flag[1] = rpl_sps_flag[0];
      }
    }
    if (this->rpl_sps_flag[i])
    {
      if (sps->sps_num_ref_pic_lists[i] > 1 &&
          (i == 0 || (i == 1 && pps->pps_rpl1_idx_present_flag)))
      {
        auto nrBits      = std::ceil(std::log2(sps->sps_num_ref_pic_lists[i]));
        this->rpl_idx[i] = reader.readBits(formatArray("rpl_idx", i), nrBits);
      }
      else if (i == 1 && !pps->pps_rpl1_idx_present_flag)
      {
        this->rpl_idx[i] = this->rpl_idx[0];
      }
    }
    else
    {
      this->ref_pic_list_structs[i].parse(reader, i, sps->sps_num_ref_pic_lists[i], sps);
    }

    this->RplsIdx[i] = this->rpl_sps_flag[i] ? this->rpl_idx[i] : sps->sps_num_ref_pic_lists[i];
    reader.logCalculatedValue(formatArray("RplsIdx", i), this->RplsIdx[i]);

    auto rpl = this->getActiveRefPixList(sps, i);

    for (unsigned j = 0; j < rpl.NumLtrpEntries; j++)
    {
      if (rpl.ltrp_in_header_flag)
      {
        auto nrBits      = sps->sps_log2_max_pic_order_cnt_lsb_minus4 + 4;
        this->poc_lsb_lt = reader.readBits(formatArray("poc_lsb_lt", i), nrBits);
      }
      this->delta_poc_msb_cycle_present_flag[i].push_back(
          reader.readFlag(formatArray("delta_poc_msb_cycle_present_flag", i)));
      if (this->delta_poc_msb_cycle_present_flag[i][j])
      {
        this->delta_poc_msb_cycle_lt[i][j] =
            reader.readUEV(formatArray("delta_poc_msb_cycle_lt", i, j));
      }
    }
  }
}

ref_pic_list_struct ref_pic_lists::getActiveRefPixList(std::shared_ptr<seq_parameter_set_rbsp> sps,
                                                       unsigned listIndex) const
{
  return (this->rpl_sps_flag.at(listIndex)
              ? sps->ref_pic_list_structs[listIndex][this->rpl_idx.at(listIndex)]
              : this->ref_pic_list_structs[listIndex]);
}

} // namespace parser::vvc
