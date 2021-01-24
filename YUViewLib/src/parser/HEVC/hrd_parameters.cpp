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

#include "hrd_parameters.h"

#include "parser/common/functions.h"

namespace parser::hevc
{

using namespace reader;

void hrd_parameters::parse(SubByteReaderLogging &reader,
                           bool                  commonInfPresentFlag,
                           unsigned              maxNumSubLayersMinus1)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "hrd_parameters()");

  if (commonInfPresentFlag)
  {
    this->nal_hrd_parameters_present_flag = reader.readFlag("nal_hrd_parameters_present_flag");
    this->vcl_hrd_parameters_present_flag = reader.readFlag("vcl_hrd_parameters_present_flag");

    if (nal_hrd_parameters_present_flag || vcl_hrd_parameters_present_flag)
    {
      this->sub_pic_hrd_params_present_flag = reader.readFlag(
          "sub_pic_hrd_params_present_flag",
          Options().withMeaning("Can the HRD operate at access unit or sub-picture level (true) or "
                                "only at access unit level (false)"));
      if (sub_pic_hrd_params_present_flag)
      {
        this->tick_divisor_minus2 = reader.readBits(
            "tick_divisor_minus2",
            8,
            Options().withMeaning("Specifies the clock sub-tick. A clock sub-tick is the minimum "
                                  "interval of time that can be represented in the coded data when "
                                  "sub_pic_hrd_params_present_flag is equal to 1"));
        this->du_cpb_removal_delay_increment_length_minus1 = reader.readBits(
            "du_cpb_removal_delay_increment_length_minus1",
            5,
            Options().withMeaning("Specifies the length, in bits, of some coded symbols"));
        this->sub_pic_cpb_params_in_pic_timing_sei_flag =
            reader.readFlag("sub_pic_cpb_params_in_pic_timing_sei_flag");
        this->dpb_output_delay_du_length_minus1 =
            reader.readBits("dpb_output_delay_du_length_minus1", 5);
      }
      this->bit_rate_scale = reader.readBits(
          "bit_rate_scale",
          4,
          Options().withMeaning("(together with bit_rate_value_minus1[i]) specifies the maximum "
                                "input bit rate of the i-th CPB."));
      this->cpb_size_scale = reader.readBits(
          "cpb_size_scale",
          4,
          Options().withMeaning("(together with cpb_size_value_minus1[i]) specifies the CPB size "
                                "of the i-th CPB when the CPB operates at the access unit level."));
      if (sub_pic_hrd_params_present_flag)
        this->cpb_size_du_scale = reader.readBits(
            "cpb_size_du_scale",
            4,
            Options().withMeaning(
                "(together with cpb_size_du_value_minus1[ i ]) specifies the CPB size of the i-th "
                "CPB when the CPB operates at sub-picture level."));
      this->initial_cpb_removal_delay_length_minus1 = reader.readBits(
          "initial_cpb_removal_delay_length_minus1",
          5,
          Options().withMeaning("Specifies the length, in bits, of some coded symbols"));
      this->au_cpb_removal_delay_length_minus1 = reader.readBits(
          "au_cpb_removal_delay_length_minus1",
          5,
          Options().withMeaning("Specifies the length, in bits, of some coded symbols"));
      this->dpb_output_delay_length_minus1 = reader.readBits(
          "dpb_output_delay_length_minus1",
          5,
          Options().withMeaning("Specifies the length, in bits, of some coded symbols"));
    }
  }

  this->SubPicHrdFlag = (this->SubPicHrdPreferredFlag && this->sub_pic_hrd_params_present_flag);
  reader.logCalculatedValue("SubPicHrdPreferredFlag", this->SubPicHrdPreferredFlag);
  reader.logCalculatedValue("SubPicHrdFlag", this->SubPicHrdFlag);

  if (maxNumSubLayersMinus1 >= 8)
    throw std::logic_error("The value of maxNumSubLayersMinus1 must be in the range of 0 to 7");

  for (int i = 0; i <= maxNumSubLayersMinus1; i++)
  {
    this->fixed_pic_rate_general_flag[i] = reader.readFlag(
        formatArray("fixed_pic_rate_general_flag", i),
        Options().withMeaning(
            "Equal to 1 indicates that, when HighestTid is equal to i, the temporal distance "
            "between the HRD output times of consecutive pictures in output order is constrained "
            "as specified by the following syntax elements."));
    if (!this->fixed_pic_rate_general_flag[i])
      this->fixed_pic_rate_within_cvs_flag[i] = reader.readFlag(
          formatArray("fixed_pic_rate_within_cvs_flag", i),
          Options().withMeaning(
              "equal to 1 indicates that, when HighestTid is equal to i, the temporal distance "
              "between the HRD output times of consecutive pictures in output order is constrained "
              "as specified by the following syntax elements."));
    else
      this->fixed_pic_rate_within_cvs_flag[i] = 1;
    if (this->fixed_pic_rate_within_cvs_flag[i])
    {
      this->elemental_duration_in_tc_minus1[i] = reader.readUEV(
          formatArray("elemental_duration_in_tc_minus1", i),
          Options()
              .withMeaning("Specifies, when HighestTid is equal to i, the temporal distance, in "
                           "clock ticks, between the elemental units that specify the HRD output "
                           "times of consecutive pictures in output order")
              .withCheckRange({0, 2047}));
      this->low_delay_hrd_flag[i] = false;
    }
    else
      this->low_delay_hrd_flag[i] =
          reader.readFlag(formatArray("low_delay_hrd_flag", i),
                          Options().withMeaning("Specifies the HRD operational mode"));
    this->cpb_cnt_minus1[i] = 0;
    if (!this->low_delay_hrd_flag[i])
    {
      this->cpb_cnt_minus1[i] = reader.readUEV(
          formatArray("cpb_cnt_minus1", i),
          Options()
              .withMeaning("Specifies the number of alternative CPB specifications in the "
                           "bitstream of the CVS when HighestTid is equal to i.")
              .withCheckRange({0, 31}));
    }

    if (this->nal_hrd_parameters_present_flag)
      this->nal_sub_hrd[i].parse(reader,
                                 cpb_cnt_minus1[i],
                                 sub_pic_hrd_params_present_flag,
                                 SubPicHrdFlag,
                                 bit_rate_scale,
                                 cpb_size_scale,
                                 cpb_size_du_scale);
    if (this->vcl_hrd_parameters_present_flag)
      vcl_sub_hrd[i].parse(reader,
                           cpb_cnt_minus1[i],
                           sub_pic_hrd_params_present_flag,
                           SubPicHrdFlag,
                           bit_rate_scale,
                           cpb_size_scale,
                           cpb_size_du_scale);
  }
}

} // namespace parser::hevc