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

namespace parser::vvc
{

class seq_parameter_set_rbsp;
class pic_parameter_set_rbsp;
class slice_layer_rbsp;
class ref_pic_lists;

class pred_weight_table : public NalRBSP
{
public:
  pred_weight_table()  = default;
  ~pred_weight_table() = default;
  void parse(reader::SubByteReaderLogging &               reader,
             std::shared_ptr<seq_parameter_set_rbsp> sps,
             std::shared_ptr<pic_parameter_set_rbsp> pps,
             std::shared_ptr<slice_layer_rbsp>       sl,
             std::shared_ptr<ref_pic_lists>          rpl);

  unsigned      luma_log2_weight_denom{};
  int           delta_chroma_log2_weight_denom{};
  unsigned      num_l0_weights{};
  vector<bool>  luma_weight_l0_flag{};
  vector<bool>  chroma_weight_l0_flag{};
  vector<int>   delta_luma_weight_l0{};
  vector<int>   luma_offset_l0{};
  umap_2d<int>  delta_chroma_weight_l0{};
  vector2d<int> delta_chroma_offset_l0{};
  unsigned      num_l1_weights{};
  vector<bool>  luma_weight_l1_flag{};
  vector<bool>  chroma_weight_l1_flag{};
  vector<int>   delta_luma_weight_l1{};
  vector<int>   luma_offset_l1{};
  vector2d<int> delta_chroma_weight_l1{};
  vector2d<int> delta_chroma_offset_l1{};
};

} // namespace parser::vvc
