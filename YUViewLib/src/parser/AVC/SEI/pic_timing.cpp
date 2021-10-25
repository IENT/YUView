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
#include <parser/common/functions.h>

namespace parser::avc

{
using namespace reader;

SEIParsingResult pic_timing::parse(SubByteReaderLogging &                  reader,
                                   bool                                    reparse,
                                   SPSMap &                                spsMap,
                                   std::shared_ptr<seq_parameter_set_rbsp> associatedSPS)
{
  (void)spsMap;

  if (!associatedSPS)
  {
    if (reparse)
      throw std::logic_error("associatedSPS is nullptr");
    else
      return SEIParsingResult::WAIT_FOR_PARAMETER_SETS;
  }

  auto subLevel = SubByteReaderLoggingSubLevel(reader, "pic_timing()");

  // ... is dependent on the content of the sequence parameter set that is active for the primary
  // coded picture associated with the picture timing SEI message.
  auto CpbDpbDelaysPresentFlag =
      associatedSPS->seqParameterSetData.vuiParameters.nal_hrd_parameters_present_flag ||
      associatedSPS->seqParameterSetData.vuiParameters.vcl_hrd_parameters_present_flag;
  if (CpbDpbDelaysPresentFlag)
  {
    unsigned nrBits_removal_delay = 0;
    unsigned nrBits_output_delay  = 0;
    auto     NalHrdBpPresentFlag =
        associatedSPS->seqParameterSetData.vuiParameters.nal_hrd_parameters_present_flag;
    if (NalHrdBpPresentFlag)
    {
      nrBits_removal_delay = associatedSPS->seqParameterSetData.vuiParameters.nalHrdParameters
                                 .cpb_removal_delay_length_minus1 +
                             1;
      nrBits_output_delay = associatedSPS->seqParameterSetData.vuiParameters.nalHrdParameters
                                .dpb_output_delay_length_minus1 +
                            1;
    }
    auto VclHrdBpPresentFlag =
        associatedSPS->seqParameterSetData.vuiParameters.vcl_hrd_parameters_present_flag;
    if (VclHrdBpPresentFlag)
    {
      nrBits_removal_delay = associatedSPS->seqParameterSetData.vuiParameters.vclHrdParameters
                                 .cpb_removal_delay_length_minus1 +
                             1;
      nrBits_output_delay = associatedSPS->seqParameterSetData.vuiParameters.vclHrdParameters
                                .dpb_output_delay_length_minus1 +
                            1;
    }

    if (NalHrdBpPresentFlag || VclHrdBpPresentFlag)
    {
      this->cpb_removal_delay = reader.readBits("cpb_removal_delay", nrBits_removal_delay);
      this->dpb_output_delay  = reader.readBits("dpb_output_delay", nrBits_output_delay);
    }
  }

  if (associatedSPS->seqParameterSetData.vuiParameters.pic_struct_present_flag)
  {
    this->pic_struct =
        reader.readBits("pic_struct",
                        4,
                        Options().withMeaningVector(
                            {"(progressive) frame",
                             "top field",
                             "bottom field",
                             "top field, bottom field, in that order",
                             "bottom field, top field, in that order",
                             "top field, bottom field, top field repeated, in that order",
                             "bottom field, top field, bottom field repeated, in that order",
                             "frame doubling",
                             "frame tripling",
                             "reserved"}));
    unsigned NumClockTS = 0;
    if (this->pic_struct < 3)
      NumClockTS = 1;
    else if (this->pic_struct < 5 || this->pic_struct == 7)
      NumClockTS = 2;
    else if (this->pic_struct < 9)
      NumClockTS = 3;
    for (unsigned i = 0; i < NumClockTS; i++)
    {
      this->clock_timestamp_flag[i] = reader.readFlag(formatArray("clock_timestamp_flag", i));
      if (clock_timestamp_flag[i])
      {
        this->ct_type[i] = reader.readBits(
            formatArray("ct_type", i),
            2,
            Options().withMeaningVector({"progressive", "interlaced", "unknown", "reserved"}));
        this->nuit_field_based_flag[i] = reader.readFlag(formatArray("nuit_field_based_flag", i));
        this->counting_type[i]         = reader.readBits(
            formatArray("counting_type", i),
            5,
            Options()
                .withMeaningVector(
                    {"no dropping of n_frames count values and no use of time_offset",
                     "no dropping of n_frames count values",
                     "dropping of individual zero values of n_frames count",
                     "dropping of individual MaxFPS - 1 values of n_frames count",
                     "dropping of the two lowest (value 0 and 1) n_frames counts when "
                     "seconds_value is equal to 0 and minutes_value is not an integer multiple of "
                     "10",
                     "dropping of unspecified individual n_frames count values",
                     "dropping of unspecified numbers of unspecified n_frames count values"})
                .withMeaning("reserved"));
        this->full_timestamp_flag[i] = reader.readFlag(formatArray("full_timestamp_flag", i));
        this->discontinuity_flag[i]  = reader.readFlag(formatArray("discontinuity_flag", i));
        this->cnt_dropped_flag[i]    = reader.readFlag(formatArray("cnt_dropped_flag", i));
        this->n_frames[i]            = reader.readBits(formatArray("n_frames", i), 8);
        if (this->full_timestamp_flag[i])
        {
          this->seconds_value[i] = reader.readBits(formatArray("seconds_value", i), 6); /* 0..59 */
          this->minutes_value[i] = reader.readBits(formatArray("minutes_value", i), 6); /* 0..59 */
          this->hours_value[i]   = reader.readBits(formatArray("hours_value", i), 5);   /* 0..23 */
        }
        else
        {
          this->seconds_flag[i] = reader.readFlag(formatArray("seconds_flag", i));
          if (this->seconds_flag[i])
          {
            this->seconds_value[i] =
                reader.readBits(formatArray("seconds_value", i), 6); /* 0..59 */
            this->minutes_flag[i] = reader.readFlag(formatArray("minutes_flag", i));
            if (this->minutes_flag[i])
            {
              this->minutes_value[i] =
                  reader.readBits(formatArray("minutes_value", i), 6); /* 0..59 */
              this->hours_flag[i] = reader.readFlag(formatArray("hours_flag", i));
              if (hours_flag[i])
                this->hours_value[i] =
                    reader.readBits(formatArray("hours_value", i), 5); /* 0..23 */
            }
          }
        }
        if (associatedSPS->seqParameterSetData.vuiParameters.nalHrdParameters.time_offset_length >
            0)
        {
          auto nrBits =
              associatedSPS->seqParameterSetData.vuiParameters.nalHrdParameters.time_offset_length;
          this->time_offset[i] = reader.readBits(formatArray("time_offset", i), nrBits);
        }
      }
    }
  }

  return SEIParsingResult::OK;
}

} // namespace parser::avc