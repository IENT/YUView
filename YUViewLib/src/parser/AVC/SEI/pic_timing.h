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
#include "sei_message.h"

namespace parser::avc
{

class seq_parameter_set_rbsp;

class pic_timing : public sei_payload
{
public:
  pic_timing() = default;

  SEIParsingResult parse(reader::SubByteReaderLogging &          reader,
                         bool                                    reparse,
                         SPSMap &                                spsMap,
                         std::shared_ptr<seq_parameter_set_rbsp> associatedSPS) override;

  unsigned cpb_removal_delay{};
  unsigned dpb_output_delay{};

  unsigned pic_struct{};
  bool     clock_timestamp_flag[3]{};
  unsigned ct_type[3]{};
  bool     nuit_field_based_flag[3]{};
  unsigned counting_type[3]{};
  bool     full_timestamp_flag[3]{};
  bool     discontinuity_flag[3]{};
  bool     cnt_dropped_flag[3]{};
  unsigned n_frames[3]{};
  unsigned seconds_value[3]{};
  unsigned minutes_value[3]{};
  unsigned hours_value[3]{};
  bool     seconds_flag[3]{};
  bool     minutes_flag[3]{};
  bool     hours_flag[3]{};
  unsigned time_offset[3]{};

private:
  reader::SubByteReaderLoggingSubLevel subLevel;
};

} // namespace parser::avc
