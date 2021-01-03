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

#include "parser/common/ReaderHelperNew.h"
#include "sei_payload.h"

namespace parser::vvc
{

class buffering_period : public sei_payload
{
public:
  buffering_period()  = default;
  ~buffering_period() = default;
  void parse(reader::ReaderHelperNew &reader);

  bool             bp_nal_hrd_params_present_flag{};
  bool             bp_vcl_hrd_params_present_flag{};
  unsigned         bp_cpb_initial_removal_delay_length_minus1{};
  unsigned         bp_cpb_removal_delay_length_minus1{};
  unsigned         bp_dpb_output_delay_length_minus1{};
  bool             bp_du_hrd_params_present_flag{};
  unsigned         bp_du_cpb_removal_delay_increment_length_minus1{};
  unsigned         bp_dpb_output_delay_du_length_minus1{};
  bool             bp_du_cpb_params_in_pic_timing_sei_flag{};
  bool             bp_du_dpb_params_in_pic_timing_sei_flag{};
  bool             bp_concatenation_flag{};
  bool             bp_additional_concatenation_info_present_flag{};
  unsigned         bp_max_initial_removal_delay_for_concatenation{};
  unsigned         bp_cpb_removal_delay_delta_minus1{};
  unsigned         bp_max_sublayers_minus1{};
  bool             bp_cpb_removal_delay_deltas_present_flag{};
  unsigned         bp_num_cpb_removal_delay_deltas_minus1{};
  unsigned         bp_cpb_removal_delay_delta_val{};
  unsigned         bp_cpb_cnt_minus1{};
  bool             bp_sublayer_initial_cpb_removal_delay_present_flag{};
  unsigned         bp_nal_initial_cpb_removal_delay{};
  unsigned         bp_nal_initial_cpb_removal_offset{};
  unsigned         bp_nal_initial_alt_cpb_removal_delay{};
  unsigned         bp_nal_initial_alt_cpb_removal_offset{};
  unsigned         bp_vcl_initial_cpb_removal_delay{};
  unsigned         bp_vcl_initial_cpb_removal_offset{};
  unsigned         bp_vcl_initial_alt_cpb_removal_delay{};
  unsigned         bp_vcl_initial_alt_cpb_removal_offset{};
  bool             bp_sublayer_dpb_output_offsets_present_flag{};
  vector<unsigned> bp_dpb_output_tid_offset{};
  bool             bp_alt_cpb_params_present_flag{};
  bool             bp_use_alt_cpb_params_flag{};
};

} // namespace parser::vvc
