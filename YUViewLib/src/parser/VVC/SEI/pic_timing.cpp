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

#include "buffering_period.h"

#include <cmath>

namespace parser::vvc
{

using namespace parser::reader;

void pic_timing::parse(SubByteReaderLogging &            reader,
                       unsigned                          nalTemporalID,
                       std::shared_ptr<buffering_period> bp)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "pic_timing");

  {
    auto nrBits                       = bp->bp_cpb_removal_delay_length_minus1 + 1;
    this->pt_cpb_removal_delay_minus1 = reader.readBits("pt_cpb_removal_delay_minus1", nrBits);
  }
  auto TemporalId = nalTemporalID;
  for (unsigned i = TemporalId; i < bp->bp_max_sublayers_minus1; i++)
  {
    this->pt_sublayer_delays_present_flag.push_back(
        reader.readFlag("pt_sublayer_delays_present_flag"));
    if (this->pt_sublayer_delays_present_flag[i])
    {
      if (bp->bp_cpb_removal_delay_deltas_present_flag)
      {
        this->pt_cpb_removal_delay_delta_enabled_flag.push_back(
            reader.readFlag("pt_cpb_removal_delay_delta_enabled_flag"));
      }
      if (this->pt_cpb_removal_delay_delta_enabled_flag[i])
      {
        if (bp->bp_num_cpb_removal_delay_deltas_minus1 > 0)
        {
          auto nrBits = std::ceil(std::log2(bp->bp_num_cpb_removal_delay_deltas_minus1 + 1));
          this->pt_cpb_removal_delay_delta_idx =
              reader.readBits("pt_cpb_removal_delay_delta_idx", nrBits);
        }
      }
      else
      {
        auto nrBits                       = bp->bp_cpb_removal_delay_length_minus1 + 1;
        this->pt_cpb_removal_delay_minus1 = reader.readBits("pt_cpb_removal_delay_minus1", nrBits);
      }
    }
  }
  {
    auto nrBits               = bp->bp_dpb_output_delay_length_minus1 + 1;
    this->pt_dpb_output_delay = reader.readBits("pt_dpb_output_delay", nrBits);
  }

  if (bp->bp_alt_cpb_params_present_flag)
  {
    this->pt_cpb_alt_timing_info_present_flag =
        reader.readFlag("pt_cpb_alt_timing_info_present_flag");
    if (this->pt_cpb_alt_timing_info_present_flag)
    {
      if (bp->bp_nal_hrd_params_present_flag)
      {
        for (unsigned i = (bp->bp_sublayer_initial_cpb_removal_delay_present_flag
                               ? 0
                               : bp->bp_max_sublayers_minus1);
             i <= bp->bp_max_sublayers_minus1;
             i++)
        {
          for (unsigned j = 0; j < bp->bp_cpb_cnt_minus1 + 1; j++)
          {
            auto nrBits = bp->bp_cpb_initial_removal_delay_length_minus1 + 1;
            this->pt_nal_cpb_alt_initial_removal_delay_delta =
                reader.readBits("pt_nal_cpb_alt_initial_removal_delay_delta", nrBits);
            this->pt_nal_cpb_alt_initial_removal_offset_delta =
                reader.readBits("pt_nal_cpb_alt_initial_removal_offset_delta", nrBits);
          }
          {
            auto nrBits                   = bp->bp_cpb_removal_delay_length_minus1 + 1;
            this->pt_nal_cpb_delay_offset = reader.readBits("pt_nal_cpb_delay_offset", nrBits);
          }
          {
            auto nrBits                   = bp->bp_dpb_output_delay_length_minus1 + 1;
            this->pt_nal_dpb_delay_offset = reader.readBits("pt_nal_dpb_delay_offset", nrBits);
          }
        }
      }
      if (bp->bp_vcl_hrd_params_present_flag)
      {
        for (unsigned i = (bp->bp_sublayer_initial_cpb_removal_delay_present_flag
                               ? 0
                               : bp->bp_max_sublayers_minus1);
             i <= bp->bp_max_sublayers_minus1;
             i++)
        {
          for (unsigned j = 0; j < bp->bp_cpb_cnt_minus1 + 1; j++)
          {
            auto nrBits = bp->bp_cpb_initial_removal_delay_length_minus1 + 1;
            this->pt_vcl_cpb_alt_initial_removal_delay_delta =
                reader.readBits("pt_vcl_cpb_alt_initial_removal_delay_delta", nrBits);
            this->pt_vcl_cpb_alt_initial_removal_offset_delta =
                reader.readBits("pt_vcl_cpb_alt_initial_removal_offset_delta", nrBits);
          }
          {
            auto nrBits                   = bp->bp_cpb_removal_delay_length_minus1 + 1;
            this->pt_vcl_cpb_delay_offset = reader.readBits("pt_vcl_cpb_delay_offset", nrBits);
          }
          {
            auto nrBits                   = bp->bp_dpb_output_delay_length_minus1 + 1;
            this->pt_vcl_dpb_delay_offset = reader.readBits("pt_vcl_dpb_delay_offset", nrBits);
          }
        }
      }
    }
  }
  if (bp->bp_du_hrd_params_present_flag && bp->bp_du_dpb_params_in_pic_timing_sei_flag)
  {
    auto nrBits                  = bp->bp_dpb_output_delay_du_length_minus1 + 1;
    this->pt_dpb_output_du_delay = reader.readBits("pt_dpb_output_du_delay", nrBits);
  }
  if (bp->bp_du_hrd_params_present_flag && bp->bp_du_cpb_params_in_pic_timing_sei_flag)
  {
    this->pt_num_decoding_units_minus1 = reader.readUEV("pt_num_decoding_units_minus1");
    if (this->pt_num_decoding_units_minus1 > 0)
    {
      this->pt_du_common_cpb_removal_delay_flag =
          reader.readFlag("pt_du_common_cpb_removal_delay_flag");
      if (this->pt_du_common_cpb_removal_delay_flag)
      {
        for (unsigned i = TemporalId; i <= bp->bp_max_sublayers_minus1; i++)
        {
          if (this->pt_sublayer_delays_present_flag[i])
          {
            auto nrBits = bp->bp_du_cpb_removal_delay_increment_length_minus1 + 1;
            this->pt_du_common_cpb_removal_delay_increment_minus1 =
                reader.readBits("pt_du_common_cpb_removal_delay_increment_minus1", nrBits);
          }
        }
      }
      for (unsigned i = 0; i <= pt_num_decoding_units_minus1; i++)
      {
        this->pt_num_nalus_in_du_minus1.push_back(reader.readUEV("pt_num_nalus_in_du_minus1"));
        if (!this->pt_du_common_cpb_removal_delay_flag && i < pt_num_decoding_units_minus1)
        {
          for (unsigned j = TemporalId; j <= bp->bp_max_sublayers_minus1; j++)
          {
            if (this->pt_sublayer_delays_present_flag[j])
            {
              auto nrBits = bp->bp_du_cpb_removal_delay_increment_length_minus1 + 1;
              this->pt_du_cpb_removal_delay_increment_minus1 =
                  reader.readBits("pt_du_cpb_removal_delay_increment_minus1", nrBits);
            }
          }
        }
      }
    }
  }
  if (bp->bp_additional_concatenation_info_present_flag)
  {
    this->pt_delay_for_concatenation_ensured_flag =
        reader.readFlag("pt_delay_for_concatenation_ensured_flag");
  }
  this->pt_display_elemental_periods_minus1 =
      reader.readBits("pt_display_elemental_periods_minus1", 8);
}

} // namespace parser::vvc
