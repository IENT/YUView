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

#include "general_timing_hrd_parameters.h"

namespace parser::vvc
{

using namespace parser::reader;

void general_timing_hrd_parameters::parse(SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "general_timing_hrd_parameters");

  this->num_units_in_tick = reader.readBits("num_units_in_tick", 32, Options().withCheckGreater(0));
  this->time_scale        = reader.readBits("time_scale", 32, Options().withCheckGreater(0));
  this->general_nal_hrd_params_present_flag =
      reader.readFlag("general_nal_hrd_params_present_flag");
  this->general_vcl_hrd_params_present_flag =
      reader.readFlag("general_vcl_hrd_params_present_flag");
  if (this->general_nal_hrd_params_present_flag || this->general_vcl_hrd_params_present_flag)
  {
    this->general_same_pic_timing_in_all_ols_flag =
        reader.readFlag("general_same_pic_timing_in_all_ols_flag");
    this->general_du_hrd_params_present_flag =
        reader.readFlag("general_du_hrd_params_present_flag");
    if (this->general_du_hrd_params_present_flag)
    {
      this->tick_divisor_minus2 = reader.readBits("tick_divisor_minus2", 8);
    }
    this->bit_rate_scale = reader.readBits("bit_rate_scale", 4);
    this->cpb_size_scale = reader.readBits("cpb_size_scale", 4);
    if (this->general_du_hrd_params_present_flag)
    {
      this->cpb_size_du_scale = reader.readBits("cpb_size_du_scale", 4);
    }
    this->hrd_cpb_cnt_minus1 =
        reader.readUEV("hrd_cpb_cnt_minus1", Options().withCheckRange({0, 31}));
  }
}

} // namespace parser::vvc
