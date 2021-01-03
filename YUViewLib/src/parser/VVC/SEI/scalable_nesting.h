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
class sei_message;

class scalable_nesting : public sei_payload
{
public:
  scalable_nesting()  = default;
  ~scalable_nesting() = default;
  void parse(reader::ReaderHelperNew &         reader,
             NalType                           nal_unit_type,
             unsigned                          nalTemporalID,
             std::shared_ptr<buffering_period> lastBufferingPeriod);

  bool             sn_ols_flag{};
  bool             sn_subpic_flag{};
  unsigned         sn_num_olss_minus1{};
  vector<unsigned> sn_ols_idx_delta_minus1{};
  bool             sn_all_layers_flag{};
  unsigned         sn_num_layers_minus1{};
  vector<unsigned> sn_layer_id{};
  unsigned         sn_num_subpics_minus1{};
  unsigned         sn_subpic_id_len_minus1{};
  int              sn_subpic_id{};
  unsigned         sn_num_seis_minus1{};

  std::vector<std::shared_ptr<sei_message>> nested_sei_messages;
};

} // namespace parser::vvc
