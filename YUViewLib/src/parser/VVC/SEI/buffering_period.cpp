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

namespace parser::vvc
{

using namespace parser::reader;

void buffering_period::parse(ReaderHelperNew &reader)
{
  ReaderHelperNewSubLevel subLevel(reader, "buffering_period");

  this->bp_nal_hrd_params_present_flag = reader.readFlag("bp_nal_hrd_params_present_flag");
  this->bp_vcl_hrd_params_present_flag = reader.readFlag("bp_vcl_hrd_params_present_flag");
  this->bp_cpb_initial_removal_delay_length_minus1 =
      reader.readBits("bp_cpb_initial_removal_delay_length_minus1", 5);
  this->bp_cpb_removal_delay_length_minus1 =
      reader.readBits("bp_cpb_removal_delay_length_minus1", 5);
  this->bp_dpb_output_delay_length_minus1 = reader.readBits("bp_dpb_output_delay_length_minus1", 5);
  this->bp_du_hrd_params_present_flag     = reader.readFlag("bp_du_hrd_params_present_flag");
  if (this->bp_du_hrd_params_present_flag)
  {
    this->bp_du_cpb_removal_delay_increment_length_minus1 =
        reader.readBits("bp_du_cpb_removal_delay_increment_length_minus1", 5);
    this->bp_dpb_output_delay_du_length_minus1 =
        reader.readBits("bp_dpb_output_delay_du_length_minus1", 5);
    this->bp_du_cpb_params_in_pic_timing_sei_flag =
        reader.readFlag("bp_du_cpb_params_in_pic_timing_sei_flag");
    this->bp_du_dpb_params_in_pic_timing_sei_flag =
        reader.readFlag("bp_du_dpb_params_in_pic_timing_sei_flag");
  }
  this->bp_concatenation_flag = reader.readFlag("bp_concatenation_flag");
  this->bp_additional_concatenation_info_present_flag =
      reader.readFlag("bp_additional_concatenation_info_present_flag");
  if (this->bp_additional_concatenation_info_present_flag)
  {
    auto nrBits = this->bp_cpb_initial_removal_delay_length_minus1 + 1;
    this->bp_max_initial_removal_delay_for_concatenation =
        reader.readBits("bp_max_initial_removal_delay_for_concatenation", nrBits);
  }
  {
    auto nrBits = this->bp_cpb_removal_delay_length_minus1 + 1;
    this->bp_cpb_removal_delay_delta_minus1 =
        reader.readBits("bp_cpb_removal_delay_delta_minus1", nrBits);
  }
  this->bp_max_sublayers_minus1 = reader.readBits("bp_max_sublayers_minus1", 3);
  if (this->bp_max_sublayers_minus1 > 0)
  {
    this->bp_cpb_removal_delay_deltas_present_flag =
        reader.readFlag("bp_cpb_removal_delay_deltas_present_flag");
  }
  if (this->bp_cpb_removal_delay_deltas_present_flag)
  {
    this->bp_num_cpb_removal_delay_deltas_minus1 =
        reader.readUEV("bp_num_cpb_removal_delay_deltas_minus1");
    for (unsigned i = 0; i <= bp_num_cpb_removal_delay_deltas_minus1; i++)
    {
      auto nrBits = this->bp_cpb_removal_delay_length_minus1 + 1;
      this->bp_cpb_removal_delay_delta_val =
          reader.readBits("bp_cpb_removal_delay_delta_val", nrBits);
    }
  }
  this->bp_cpb_cnt_minus1 = reader.readUEV("bp_cpb_cnt_minus1");
  if (this->bp_max_sublayers_minus1 > 0)
  {
    this->bp_sublayer_initial_cpb_removal_delay_present_flag =
        reader.readFlag("bp_sublayer_initial_cpb_removal_delay_present_flag");
  }
  for (unsigned i =
           (bp_sublayer_initial_cpb_removal_delay_present_flag ? 0 : bp_max_sublayers_minus1);
       i <= bp_max_sublayers_minus1;
       i++)
  {
    if (this->bp_nal_hrd_params_present_flag)
    {
      for (unsigned j = 0; j < bp_cpb_cnt_minus1 + 1; j++)
      {
        auto nrBits = bp_cpb_initial_removal_delay_length_minus1 + 1;

        this->bp_nal_initial_cpb_removal_delay =
            reader.readBits("bp_nal_initial_cpb_removal_delay", nrBits);
        this->bp_nal_initial_cpb_removal_offset =
            reader.readBits("bp_nal_initial_cpb_removal_offset", nrBits);
        if (this->bp_du_hrd_params_present_flag)
        {
          this->bp_nal_initial_alt_cpb_removal_delay =
              reader.readBits("bp_nal_initial_alt_cpb_removal_delay", nrBits);
          this->bp_nal_initial_alt_cpb_removal_offset =
              reader.readBits("bp_nal_initial_alt_cpb_removal_offset", nrBits);
        }
      }
    }
    if (this->bp_vcl_hrd_params_present_flag)
    {
      for (unsigned j = 0; j < bp_cpb_cnt_minus1 + 1; j++)
      {
        auto nrBits = this->bp_cpb_initial_removal_delay_length_minus1 + 1;
        this->bp_vcl_initial_cpb_removal_delay =
            reader.readBits("bp_vcl_initial_cpb_removal_delay", nrBits);
        this->bp_vcl_initial_cpb_removal_offset =
            reader.readBits("bp_vcl_initial_cpb_removal_offset", nrBits);
        if (this->bp_du_hrd_params_present_flag)
        {
          this->bp_vcl_initial_alt_cpb_removal_delay =
              reader.readBits("bp_vcl_initial_alt_cpb_removal_delay", nrBits);
          this->bp_vcl_initial_alt_cpb_removal_offset =
              reader.readBits("bp_vcl_initial_alt_cpb_removal_offset", nrBits);
        }
      }
    }
  }
  if (this->bp_max_sublayers_minus1 > 0)
  {
    this->bp_sublayer_dpb_output_offsets_present_flag =
        reader.readFlag("bp_sublayer_dpb_output_offsets_present_flag");
  }
  if (this->bp_sublayer_dpb_output_offsets_present_flag)
  {
    for (unsigned i = 0; i < bp_max_sublayers_minus1; i++)
    {
      this->bp_dpb_output_tid_offset.push_back(reader.readUEV("bp_dpb_output_tid_offset"));
    }
  }
  this->bp_alt_cpb_params_present_flag = reader.readFlag("bp_alt_cpb_params_present_flag");
  if (this->bp_alt_cpb_params_present_flag)
  {
    this->bp_use_alt_cpb_params_flag = reader.readFlag("bp_use_alt_cpb_params_flag");
  }
}

} // namespace parser::vvc
