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

#include "buffering_period.h"

#include "../commonMaps.h"
#include "../seq_parameter_set_rbsp.h"
#include "../typedef.h"
#include <parser/common/functions.h>

namespace parser::avc

{
using namespace reader;

SEIParsingResult buffering_period::parse(reader::SubByteReaderLogging &          reader,
                                         bool                                    reparse,
                                         SPSMap &                                spsMap,
                                         std::shared_ptr<seq_parameter_set_rbsp> associatedSPS)
{
  (void)associatedSPS;

  std::unique_ptr<SubByteReaderLoggingSubLevel> subLevel;
  if (reparse)
    reader.stashAndReplaceCurrentTreeItem(this->reparseTreeItem);
  else
    subLevel.reset(new SubByteReaderLoggingSubLevel(reader, "buffering_period()"));

  if (!reparse)
  {
    this->seq_parameter_set_id = reader.readUEV("seq_parameter_set_id");
    if (spsMap.count(this->seq_parameter_set_id) == 0)
    {
      this->reparseTreeItem = reader.getCurrentItemTree();
      return SEIParsingResult::WAIT_FOR_PARAMETER_SETS;
    }
  }
  else
  {
    if (spsMap.count(this->seq_parameter_set_id) == 0)
      throw std::logic_error("SPS with the given seq_parameter_set_id not found");
  }
  auto refSPS = spsMap.at(this->seq_parameter_set_id);

  if (refSPS->seqParameterSetData.vuiParameters.nal_hrd_parameters_present_flag)
  {
    auto cpb_cnt_minus1 = refSPS->seqParameterSetData.vuiParameters.nalHrdParameters.cpb_cnt_minus1;
    for (unsigned SchedSelIdx = 0; SchedSelIdx <= cpb_cnt_minus1; SchedSelIdx++)
    {
      int nrBits = refSPS->seqParameterSetData.vuiParameters.nalHrdParameters
                       .initial_cpb_removal_delay_length_minus1 +
                   1;
      this->initial_cpb_removal_delay.push_back(
          reader.readBits(formatArray("initial_cpb_removal_delay", SchedSelIdx), nrBits));
      this->initial_cpb_removal_delay_offset.push_back(
          reader.readBits(formatArray("initial_cpb_removal_delay_offset", SchedSelIdx), nrBits));
    }
  }
  bool VclHrdBpPresentFlag =
      refSPS->seqParameterSetData.vuiParameters.vcl_hrd_parameters_present_flag;
  if (VclHrdBpPresentFlag)
  {
    auto cpb_cnt_minus1 = refSPS->seqParameterSetData.vuiParameters.vclHrdParameters.cpb_cnt_minus1;
    for (unsigned SchedSelIdx = 0; SchedSelIdx <= cpb_cnt_minus1; SchedSelIdx++)
    {
      this->initial_cpb_removal_delay.push_back(
          reader.readBits(formatArray("initial_cpb_removal_delay", SchedSelIdx), 5));
      this->initial_cpb_removal_delay_offset.push_back(
          reader.readBits(formatArray("initial_cpb_removal_delay_offset", SchedSelIdx), 5));
    }
  }

  reader.popTreeItem();
  return SEIParsingResult::OK;
}

} // namespace parser::avc