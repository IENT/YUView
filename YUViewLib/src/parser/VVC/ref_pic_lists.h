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

#pragma once

#include "NalUnitVVC.h"
#include "parser/common/SubByteReaderLogging.h"
#include "ref_pic_list_struct.h"

namespace parser::vvc
{

class seq_parameter_set_rbsp;
class pic_parameter_set_rbsp;

class ref_pic_lists : public NalRBSP
{
public:
  ref_pic_lists()  = default;
  ~ref_pic_lists() = default;
  void parse(reader::SubByteReaderLogging &          reader,
             std::shared_ptr<seq_parameter_set_rbsp> sps,
             std::shared_ptr<pic_parameter_set_rbsp> pps);

  ref_pic_list_struct getActiveRefPixList(std::shared_ptr<seq_parameter_set_rbsp> sps, unsigned listIndex) const;

  umap_1d<bool>       rpl_sps_flag{};
  umap_1d<int>        rpl_idx{};
  ref_pic_list_struct ref_pic_list_structs[2];
  int                 poc_lsb_lt{};
  vector2d<bool>      delta_poc_msb_cycle_present_flag{};
  umap_2d<unsigned>   delta_poc_msb_cycle_lt{};

  umap_1d<int> RplsIdx;
};

} // namespace parser::vvc
