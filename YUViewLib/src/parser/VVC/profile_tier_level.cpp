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

#include "profile_tier_level.h"

namespace parser::vvc
{

using namespace parser::reader;

void profile_tier_level::parse(SubByteReaderLogging &reader,
                               bool             profileTierPresentFlag,
                               unsigned         MaxNumSubLayersMinus1)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "profile_tier_level");

  if (profileTierPresentFlag)
  {
    this->general_profile_idc = reader.readBits("general_profile_idc", 7);
    this->general_tier_flag   = reader.readFlag("general_tier_flag");
  }
  this->general_level_idc              = reader.readBits("general_level_idc", 8);
  this->ptl_frame_only_constraint_flag = reader.readFlag("ptl_frame_only_constraint_flag");
  this->ptl_multilayer_enabled_flag    = reader.readFlag("ptl_multilayer_enabled_flag");
  if (profileTierPresentFlag)
  {
    this->general_constraints_info_instance.parse(reader);
  }
  for (int i = MaxNumSubLayersMinus1 - 1; i >= 0; i--)
  {
    this->ptl_sublayer_level_present_flag.push_back(
        reader.readFlag("ptl_sublayer_level_present_flag"));
  }
  while (!reader.byte_aligned())
  {
    this->ptl_reserved_zero_bit = reader.readFlag("ptl_reserved_zero_bit");
  }
  for (int i = MaxNumSubLayersMinus1 - 1; i >= 0; i--)
  {
    if (this->ptl_sublayer_level_present_flag[i])
    {
      this->sublayer_level_idc.push_back(reader.readBits("sublayer_level_idc", 8));
    }
  }
  if (profileTierPresentFlag)
  {
    this->ptl_num_sub_profiles = reader.readBits("ptl_num_sub_profiles", 8);
    for (unsigned i = 0; i < this->ptl_num_sub_profiles; i++)
    {
      this->general_sub_profile_idc.push_back(reader.readBits("general_sub_profile_idc", 32));
    }
  }
}

} // namespace parser::vvc
