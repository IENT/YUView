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

#include "pic_parameter_set_rbsp.h"
#include "seq_parameter_set_rbsp.h"

#include <cmath>

namespace parser::vvc
{

using namespace parser::reader;

void ref_pic_lists::parse(ReaderHelperNew &                       reader,
                          std::shared_ptr<seq_parameter_set_rbsp> sps,
                          std::shared_ptr<pic_parameter_set_rbsp> pps)
{
  ReaderHelperNewSubLevel subLevel(reader, "ref_pic_lists");

  for (unsigned i = 0; i < 2; i++)
  {
    this->delta_poc_msb_cycle_present_flag.push_back({});
    if (sps->sps_num_ref_pic_lists[i] > 0 && (i == 0 || (i == 1 && pps->pps_rpl1_idx_present_flag)))
    {
      this->rpl_sps_flag[i] = reader.readFlag("rpl_sps_flag");
    }
    if (this->rpl_sps_flag[i])
    {
      if (sps->sps_num_ref_pic_lists[i] > 1 &&
          (i == 0 || (i == 1 && pps->pps_rpl1_idx_present_flag)))
      {
        auto nrBits      = std::ceil(std::log2(sps->sps_num_ref_pic_lists[i]));
        this->rpl_idx[i] = reader.readBits("rpl_idx", nrBits);
      }
    }
    else
    {
      this->ref_pic_list_struct_instance.parse(reader, i, sps->sps_num_ref_pic_lists[i], sps);
    }
    this->RplsIdx[i] = this->rpl_sps_flag[i] ? this->rpl_idx[i] : sps->sps_num_ref_pic_lists[i];
    for (unsigned j = 0; j < ref_pic_list_struct_instance.NumLtrpEntries[i][this->RplsIdx[i]]; j++)
    {
      if (ref_pic_list_struct_instance.ltrp_in_header_flag[i][this->RplsIdx[i]])
      {
        auto nrBits      = sps->sps_log2_max_pic_order_cnt_lsb_minus4 + 4;
        this->poc_lsb_lt = reader.readBits("poc_lsb_lt", nrBits);
      }
      this->delta_poc_msb_cycle_present_flag[i].push_back(
          reader.readFlag("delta_poc_msb_cycle_present_flag"));
      if (this->delta_poc_msb_cycle_present_flag[i][j])
      {
        this->delta_poc_msb_cycle_lt[i][j] = reader.readUEV("delta_poc_msb_cycle_lt");
      }
    }
  }
}

} // namespace parser::vvc
