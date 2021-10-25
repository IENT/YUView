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
#include <parser/common/functions.h>

namespace parser::hevc
{

using namespace reader;

void profile_tier_level::parse(SubByteReaderLogging &reader,
                               bool                  profilePresentFlag,
                               unsigned              maxNumSubLayersMinus1)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "profile_tier_level()");

  // Profile tier level
  if (profilePresentFlag)
  {
    this->general_profile_space = reader.readBits("general_profile_space", 2);
    this->general_tier_flag     = reader.readFlag("general_tier_flag");
    this->general_profile_idc   = reader.readBits("general_profile_idc", 5);

    for (unsigned j = 0; j < 32; j++)
      this->general_profile_compatibility_flag[j] =
          reader.readFlag(formatArray("general_profile_compatibility_flag", j));
    this->general_progressive_source_flag = reader.readFlag("general_progressive_source_flag");
    this->general_interlaced_source_flag  = reader.readFlag("general_interlaced_source_flag");
    this->general_non_packed_constraint_flag =
        reader.readFlag("general_non_packed_constraint_flag");
    this->general_frame_only_constraint_flag =
        reader.readFlag("general_frame_only_constraint_flag");

    if (this->general_profile_idc == 4 || this->general_profile_compatibility_flag[4] ||
        this->general_profile_idc == 5 || this->general_profile_compatibility_flag[5] ||
        this->general_profile_idc == 6 || this->general_profile_compatibility_flag[6] ||
        this->general_profile_idc == 7 || this->general_profile_compatibility_flag[7])
    {
      this->general_max_12bit_constraint_flag =
          reader.readFlag("general_max_12bit_constraint_flag");
      this->general_max_10bit_constraint_flag =
          reader.readFlag("general_max_10bit_constraint_flag");
      this->general_max_8bit_constraint_flag = reader.readFlag("general_max_8bit_constraint_flag");
      this->general_max_422chroma_constraint_flag =
          reader.readFlag("general_max_422chroma_constraint_flag");
      this->general_max_420chroma_constraint_flag =
          reader.readFlag("general_max_420chroma_constraint_flag");
      this->general_max_monochrome_constraint_flag =
          reader.readFlag("general_max_monochrome_constraint_flag");
      this->general_intra_constraint_flag = reader.readFlag("general_intra_constraint_flag");
      this->general_one_picture_only_constraint_flag =
          reader.readFlag("general_one_picture_only_constraint_flag");
      this->general_lower_bit_rate_constraint_flag =
          reader.readFlag("general_lower_bit_rate_constraint_flag");

      reader.readBits("general_reserved_zero_bits", 34, Options().withCheckEqualTo(0));
    }
    else
      reader.readBits("general_reserved_zero_bits", 43, Options().withCheckEqualTo(0));

    if ((this->general_profile_idc >= 1 && this->general_profile_idc <= 5) ||
        this->general_profile_compatibility_flag[1] ||
        this->general_profile_compatibility_flag[2] ||
        this->general_profile_compatibility_flag[3] ||
        this->general_profile_compatibility_flag[4] || this->general_profile_compatibility_flag[5])
      this->general_inbld_flag = reader.readFlag("general_inbld_flag");
    else
      reader.readFlag("general_reserved_zero_bit", Options().withCheckEqualTo(0));
  }

  auto general_level_idc_meaning = [](int64_t idc) {
    if (idc % 30 == 0)
      return "Level " + std::to_string(idc / 30);
    else
    {
      auto remainder = idc % 30;
      return "Level " + std::to_string(idc / 30) + "." + std::to_string(remainder);
    }
  };
  this->general_level_idc = reader.readBits(
      "general_level_idc", 8, Options().withMeaningFunction(general_level_idc_meaning));

  for (unsigned i = 0; i < maxNumSubLayersMinus1; i++)
  {
    this->sub_layer_profile_present_flag[i] =
        reader.readFlag(formatArray("sub_layer_profile_present_flag", i));
    this->sub_layer_level_present_flag[i] =
        reader.readFlag(formatArray("sub_layer_level_present_flag", i));
  }

  if (maxNumSubLayersMinus1 > 0)
    for (unsigned i = maxNumSubLayersMinus1; i < 8; i++)
      reader.readBits(formatArray("reserved_zero_2bits", i), 2, Options().withCheckEqualTo(0));

  for (unsigned i = 0; i < maxNumSubLayersMinus1; i++)
  {
    if (this->sub_layer_profile_present_flag[i])
    {
      this->sub_layer_profile_space[i] =
          reader.readBits(formatArray("sub_layer_profile_space", i), 2);
      this->sub_layer_tier_flag[i] = reader.readFlag(formatArray("sub_layer_tier_flag", i));

      this->sub_layer_profile_idc[i] = reader.readBits(formatArray("sub_layer_profile_idc", i), 5);

      for (unsigned j = 0; j < 32; j++)
        this->sub_layer_profile_compatibility_flag[i][j] =
            reader.readFlag(formatArray("sub_layer_profile_compatibility_flag", i, j));

      this->sub_layer_progressive_source_flag[i] =
          reader.readFlag(formatArray("sub_layer_progressive_source_flag", i));
      this->sub_layer_interlaced_source_flag[i] =
          reader.readFlag(formatArray("sub_layer_interlaced_source_flag", i));
      this->sub_layer_non_packed_constraint_flag[i] =
          reader.readFlag(formatArray("sub_layer_non_packed_constraint_flag", i));
      this->sub_layer_frame_only_constraint_flag[i] =
          reader.readFlag(formatArray("sub_layer_frame_only_constraint_flag", i));

      if (sub_layer_profile_idc[i] == 4 || sub_layer_profile_compatibility_flag[i][4] ||
          sub_layer_profile_idc[i] == 5 || sub_layer_profile_compatibility_flag[i][5] ||
          sub_layer_profile_idc[i] == 6 || sub_layer_profile_compatibility_flag[i][6] ||
          sub_layer_profile_idc[i] == 7 || sub_layer_profile_compatibility_flag[i][7])
      {
        this->sub_layer_max_12bit_constraint_flag[i] =
            reader.readFlag(formatArray("sub_layer_max_12bit_constraint_flag", i));
        this->sub_layer_max_10bit_constraint_flag[i] =
            reader.readFlag(formatArray("sub_layer_max_10bit_constraint_flag", i));
        this->sub_layer_max_8bit_constraint_flag[i] =
            reader.readFlag(formatArray("sub_layer_max_8bit_constraint_flag", i));
        this->sub_layer_max_422chroma_constraint_flag[i] =
            reader.readFlag(formatArray("sub_layer_max_422chroma_constraint_flag", i));
        this->sub_layer_max_420chroma_constraint_flag[i] =
            reader.readFlag(formatArray("sub_layer_max_420chroma_constraint_flag", i));
        this->sub_layer_max_monochrome_constraint_flag[i] =
            reader.readFlag(formatArray("sub_layer_max_monochrome_constraint_flag", i));
        this->sub_layer_intra_constraint_flag[i] =
            reader.readFlag(formatArray("sub_layer_intra_constraint_flag", i));
        this->sub_layer_one_picture_only_constraint_flag[i] =
            reader.readFlag(formatArray("sub_layer_one_picture_only_constraint_flag", i));
        this->sub_layer_lower_bit_rate_constraint_flag[i] =
            reader.readFlag(formatArray("sub_layer_lower_bit_rate_constraint_flag", i));

        reader.readBits("sub_layer_reserved_zero_bits", 34, Options().withCheckEqualTo(0));
      }
      else
        reader.readBits("sub_layer_reserved_zero_bits", 43, Options().withCheckEqualTo(0));

      if ((this->sub_layer_profile_idc[i] >= 1 && this->sub_layer_profile_idc[i] <= 5) ||
          sub_layer_profile_compatibility_flag[1] || sub_layer_profile_compatibility_flag[2] ||
          sub_layer_profile_compatibility_flag[3] || sub_layer_profile_compatibility_flag[4] ||
          sub_layer_profile_compatibility_flag[5])
        this->sub_layer_inbld_flag[i] = reader.readFlag(formatArray("sub_layer_inbld_flag", i));
      else
        this->sub_layer_reserved_zero_bit[i] =
            reader.readFlag(formatArray("sub_layer_reserved_zero_bit", i));
    }

    if (this->sub_layer_level_present_flag[i])
      this->sub_layer_level_idc[i] = reader.readBits(formatArray("sub_layer_level_idc", i), 8);
  }
}

} // namespace parser::hevc