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

class buffering_period;

class pic_timing : public sei_payload
{
public:
  pic_timing()  = default;
  ~pic_timing() = default;
  void parse(reader::ReaderHelperNew &         reader,
             unsigned                          nalTemporalID,
             std::shared_ptr<buffering_period> bp);

  unsigned         pt_cpb_removal_delay_minus1{};
  vector<bool>     pt_sublayer_delays_present_flag{};
  vector<bool>     pt_cpb_removal_delay_delta_enabled_flag{};
  unsigned         pt_cpb_removal_delay_delta_idx{};
  unsigned         pt_dpb_output_delay{};
  bool             pt_cpb_alt_timing_info_present_flag{};
  unsigned         pt_nal_cpb_alt_initial_removal_delay_delta{};
  unsigned         pt_nal_cpb_alt_initial_removal_offset_delta{};
  unsigned         pt_nal_cpb_delay_offset{};
  unsigned         pt_nal_dpb_delay_offset{};
  unsigned         pt_vcl_cpb_alt_initial_removal_delay_delta{};
  unsigned         pt_vcl_cpb_alt_initial_removal_offset_delta{};
  unsigned         pt_vcl_cpb_delay_offset{};
  unsigned         pt_vcl_dpb_delay_offset{};
  unsigned         pt_dpb_output_du_delay{};
  unsigned         pt_num_decoding_units_minus1{};
  bool             pt_du_common_cpb_removal_delay_flag{};
  unsigned         pt_du_common_cpb_removal_delay_increment_minus1{};
  vector<unsigned> pt_num_nalus_in_du_minus1{};
  unsigned         pt_du_cpb_removal_delay_increment_minus1{};
  bool             pt_delay_for_concatenation_ensured_flag{};
  unsigned         pt_display_elemental_periods_minus1{};
};

} // namespace parser::vvc
