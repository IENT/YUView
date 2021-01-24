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
#include "sub_layer_hrd_parameters.h"

namespace parser::hevc
{

// E.2.2 HRD parameters syntax
class hrd_parameters
{
public:
  hrd_parameters() {}

  void parse(reader::SubByteReaderLogging &reader,
             bool                          commonInfPresentFlag,
             unsigned                      maxNumSubLayersMinus1);

  bool nal_hrd_parameters_present_flag{};
  bool vcl_hrd_parameters_present_flag{};
  bool sub_pic_hrd_params_present_flag{};

  unsigned tick_divisor_minus2;
  unsigned du_cpb_removal_delay_increment_length_minus1{};
  bool     sub_pic_cpb_params_in_pic_timing_sei_flag{};
  unsigned dpb_output_delay_du_length_minus1{};

  unsigned bit_rate_scale{};
  unsigned cpb_size_scale{};
  unsigned cpb_size_du_scale{};
  unsigned initial_cpb_removal_delay_length_minus1{23};
  unsigned au_cpb_removal_delay_length_minus1{23};
  unsigned dpb_output_delay_length_minus1{23};

  bool SubPicHrdPreferredFlag{};
  bool SubPicHrdFlag{};

  bool     fixed_pic_rate_general_flag[8]{};
  bool     fixed_pic_rate_within_cvs_flag[8]{};
  unsigned elemental_duration_in_tc_minus1[8]{};
  bool     low_delay_hrd_flag[8]{};
  unsigned cpb_cnt_minus1[8]{};

  sub_layer_hrd_parameters nal_sub_hrd[8];
  sub_layer_hrd_parameters vcl_sub_hrd[8];
};

} // namespace parser::hevc