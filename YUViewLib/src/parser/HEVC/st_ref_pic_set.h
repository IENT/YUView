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

#include "parser/common/SubByteReaderLogging.h"

namespace parser::hevc
{

class slice_segment_header;

// 7.3.7 Short-term reference picture set syntax
class st_ref_pic_set
{
public:
  st_ref_pic_set() {}

  void parse(reader::SubByteReaderLogging &reader, unsigned stRpsIdx, unsigned num_short_term_ref_pic_sets);

  unsigned NumPicTotalCurr(int CurrRpsIdx, const slice_segment_header* slice);

  bool         inter_ref_pic_set_prediction_flag{};
  unsigned     delta_idx_minus1{};
  bool         delta_rps_sign{};
  unsigned     abs_delta_rps_minus1{};
  vector<bool> used_by_curr_pic_flag;
  vector<bool> use_delta_flag;

  unsigned         num_negative_pics{};
  unsigned         num_positive_pics{};
  vector<unsigned> delta_poc_s0_minus1;
  vector<bool>     used_by_curr_pic_s0_flag;
  vector<unsigned> delta_poc_s1_minus1;
  vector<bool>     used_by_curr_pic_s1_flag;

  // Calculated values. These are static. They are used for reference picture set prediction.
  static unsigned NumNegativePics[65];
  static unsigned NumPositivePics[65];
  static int      DeltaPocS0[65][16];
  static int      DeltaPocS1[65][16];
  static bool     UsedByCurrPicS0[65][16];
  static bool     UsedByCurrPicS1[65][16];
  static unsigned NumDeltaPocs[65];
};

} // namespace parser::hevc