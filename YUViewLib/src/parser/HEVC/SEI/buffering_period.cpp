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

#include "../hrd_parameters.h"
#include "../seq_parameter_set_rbsp.h"
#include "parser/common/functions.h"


namespace parser::hevc
{

using namespace reader;

SEIParsingResult buffering_period::parse(SubByteReaderLogging &          reader,
                                         bool                                    reparse,
                                         VPSMap &                                vpsMap,
                                         SPSMap &                                spsMap,
                                         std::shared_ptr<seq_parameter_set_rbsp> associatedSPS)
{
  (void)vpsMap;
  (void)associatedSPS;

  if (!reparse)
    this->subLevel = SubByteReaderLoggingSubLevel(reader, "buffering_period()");

  this->bp_seq_parameter_set_id = reader.readUEV("bp_seq_parameter_set_id");

  if (!reparse)
  {
    if (spsMap.count(this->bp_seq_parameter_set_id) == 0)
      return SEIParsingResult::WAIT_FOR_PARAMETER_SETS;
  }
  else
  {
    if (spsMap.count(this->bp_seq_parameter_set_id) == 0)
      throw std::logic_error("SPS with the given bp_seq_parameter_set_id not found");
  }
  auto sps = spsMap.at(this->bp_seq_parameter_set_id);

  // Get the hrd parameters. TODO: It this really always correct?
  hrd_parameters hrd;
  if (sps->vui_parameters_present_flag && sps->vuiParameters.vui_hrd_parameters_present_flag)
    hrd = sps->vuiParameters.hrdParameters;
  else
    throw std::logic_error("Could not found the needed HRD parameters.");

  if (!hrd.sub_pic_hrd_params_present_flag)
    this->irap_cpb_params_present_flag = reader.readFlag("irap_cpb_params_present_flag");
  if (this->irap_cpb_params_present_flag)
  {
    int nrBits             = hrd.au_cpb_removal_delay_length_minus1 + 1;
    this->cpb_delay_offset = reader.readBits("cpb_delay_offset", nrBits);
    nrBits                 = hrd.dpb_output_delay_length_minus1 + 1;
    this->dpb_delay_offset = reader.readBits("dpb_delay_offset", nrBits);
  }
  this->concatenation_flag = reader.readFlag("concatenation_flag");
  auto nrBits              = hrd.au_cpb_removal_delay_length_minus1 + 1;
  this->au_cpb_removal_delay_delta_minus1 =
      reader.readBits("au_cpb_removal_delay_delta_minus1", nrBits);

  auto NalHrdBpPresentFlag = hrd.nal_hrd_parameters_present_flag;
  // TODO: Really not sure if this is correct
  auto CpbCnt = hrd.cpb_cnt_minus1[0] + 1;
  if (NalHrdBpPresentFlag)
  {
    for (unsigned i = 0; i < CpbCnt; i++)
    {
      const auto nrBits = hrd.initial_cpb_removal_delay_length_minus1 + 1;
      this->nal_initial_cpb_removal_delay.push_back(
          reader.readBits(formatArray("nal_initial_cpb_removal_delay", i), nrBits));
      this->nal_initial_cpb_removal_offset.push_back(
          reader.readBits(formatArray("nal_initial_cpb_removal_offset", i), nrBits));
      if (hrd.sub_pic_hrd_params_present_flag || this->irap_cpb_params_present_flag)
      {
        this->nal_initial_alt_cpb_removal_delay.push_back(
            reader.readBits(formatArray("nal_initial_alt_cpb_removal_delay", i), nrBits));
        this->nal_initial_alt_cpb_removal_offset.push_back(
            reader.readBits(formatArray("nal_initial_alt_cpb_removal_offset", i), nrBits));
      }
    }
  }
  auto VclHrdBpPresentFlag = hrd.vcl_hrd_parameters_present_flag;
  if (VclHrdBpPresentFlag)
  {
    for (unsigned i = 0; i < CpbCnt; i++)
    {
      const int nrBits = hrd.initial_cpb_removal_delay_length_minus1 + 1;
      this->vcl_initial_cpb_removal_delay.push_back(
          reader.readBits(formatArray("vcl_initial_cpb_removal_delay", i), nrBits));
      this->vcl_initial_cpb_removal_offset.push_back(
          reader.readBits(formatArray("vcl_initial_cpb_removal_offset", i), nrBits));
      if (hrd.sub_pic_hrd_params_present_flag || this->irap_cpb_params_present_flag)
      {
        this->vcl_initial_alt_cpb_removal_delay.push_back(
            reader.readBits(formatArray("vcl_initial_alt_cpb_removal_delay", i), nrBits));
        this->vcl_initial_alt_cpb_removal_offset.push_back(
            reader.readBits(formatArray("vcl_initial_alt_cpb_removal_offset", i), nrBits));
      }
    }
  }
  if (reader.payload_extension_present())
    this->use_alt_cpb_params_flag = reader.readFlag("use_alt_cpb_params_flag");

  return SEIParsingResult::OK;
}

} // namespace parser::hevc