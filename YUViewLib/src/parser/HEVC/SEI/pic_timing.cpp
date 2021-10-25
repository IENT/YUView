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

#include "pic_timing.h"

#include "../seq_parameter_set_rbsp.h"
#include "../video_parameter_set_rbsp.h"
#include <parser/common/Functions.h>

namespace parser::hevc
{

using namespace reader;

SEIParsingResult pic_timing::parse(SubByteReaderLogging &                  reader,
                                   bool                                    reparse,
                                   VPSMap &                                vpsMap,
                                   SPSMap &                                spsMap,
                                   std::shared_ptr<seq_parameter_set_rbsp> associatedSPS)
{
  (void)spsMap;

  if (!reparse)
  {
    if (!associatedSPS)
      return SEIParsingResult::WAIT_FOR_PARAMETER_SETS;
    if (vpsMap.count(associatedSPS->sps_video_parameter_set_id) == 0)
      return SEIParsingResult::WAIT_FOR_PARAMETER_SETS;
  }
  else
  {
    if (!associatedSPS)
      throw std::logic_error("No associated SPS given.");
    if (vpsMap.count(associatedSPS->sps_video_parameter_set_id) == 0)
      throw std::logic_error("VPS with the given sps_video_parameter_set_id not found");
  }
  auto vps = vpsMap.at(associatedSPS->sps_video_parameter_set_id);
  auto sps = associatedSPS;

  auto subLevel = SubByteReaderLoggingSubLevel(reader, "pic_timing()");

  if (sps->vuiParameters.frame_field_info_present_flag)
  {
    this->pic_struct = reader.readBits(
        "pic_struct",
        4,
        Options()
            .withMeaningVector({"(progressive) Frame",
                                "Top field",
                                "Bottom field",
                                "Top field, bottom field, in that order",
                                "Bottom field, top field, in that order",
                                "Top field, bottom field, top field repeated, in that order",
                                "Bottom field, top field, bottom field repeated, in that order",
                                "Frame doubling",
                                "Frame tripling",
                                "Top field paired with previous bottom field in output order",
                                "Bottom field paired with previous top field in output order",
                                "Top field paired with next bottom field in output order",
                                "Bottom field paired with next top field in output order"})
            .withMeaning("Reserved for future use by ITU-T | ISO/IEC"));
    this->source_scan_type = reader.readBits(
        "source_scan_type",
        2,
        Options().withMeaningVector({"interlaced",
                                     "progressive",
                                     "unknown or unspecified",
                                     "reserved for future use by ITU-T | ISO/IEC"}));
    this->duplicate_flag = reader.readFlag("duplicate_flag");
  }

  // Get the hrd parameters. TODO: It this really always correct?
  hrd_parameters hrd;
  if (sps->vui_parameters_present_flag && sps->vuiParameters.vui_hrd_parameters_present_flag)
    hrd = sps->vuiParameters.hrdParameters;
  else if (vps->vps_timing_info_present_flag && vps->vps_num_hrd_parameters > 0)
    // What if there are multiple sets?
    hrd = vps->vps_hrd_parameters[0];

  // true if nal_hrd_parameters_present_flag or vcl_hrd_parameters_present_flag is present in the
  // bitstream and is equal to 1.
  auto CpbDpbDelaysPresentFlag =
      (hrd.nal_hrd_parameters_present_flag || hrd.vcl_hrd_parameters_present_flag);
  if (CpbDpbDelaysPresentFlag)
  {
    auto nr_bits                      = hrd.au_cpb_removal_delay_length_minus1 + 1;
    this->au_cpb_removal_delay_minus1 = reader.readBits("au_cpb_removal_delay_minus1", nr_bits);
    nr_bits                           = hrd.dpb_output_delay_length_minus1 + 1;
    this->pic_dpb_output_delay        = reader.readBits("pic_dpb_output_delay", nr_bits);
    if (hrd.sub_pic_hrd_params_present_flag)
    {
      nr_bits                       = hrd.dpb_output_delay_du_length_minus1 + 1;
      this->pic_dpb_output_du_delay = reader.readBits("pic_dpb_output_du_delay", nr_bits);
    }
    if (hrd.sub_pic_hrd_params_present_flag && hrd.sub_pic_cpb_params_in_pic_timing_sei_flag)
    {
      this->num_decoding_units_minus1 = reader.readUEV(
          "num_decoding_units_minus1", Options().withCheckRange({0, sps->PicSizeInCtbsY - 1}));
      this->du_common_cpb_removal_delay_flag = reader.readFlag("du_common_cpb_removal_delay_flag");
      if (this->du_common_cpb_removal_delay_flag)
      {
        nr_bits = hrd.du_cpb_removal_delay_increment_length_minus1 + 1;
        this->du_common_cpb_removal_delay_increment_minus1 =
            reader.readBits("du_common_cpb_removal_delay_increment_minus1", nr_bits);
      }
      for (unsigned i = 0; i <= this->num_decoding_units_minus1; i++)
      {
        this->num_nalus_in_du_minus1.push_back(
            reader.readUEV(formatArray("num_nalus_in_du_minus1", i)));
        if (!this->du_common_cpb_removal_delay_flag && i < this->num_decoding_units_minus1)
        {
          nr_bits = hrd.du_cpb_removal_delay_increment_length_minus1 + 1;
          this->du_cpb_removal_delay_increment_minus1.push_back(
              reader.readBits(formatArray("du_cpb_removal_delay_increment_minus1", i), nr_bits));
        }
      }
    }
  }

  return SEIParsingResult::OK;
}

} // namespace parser::hevc