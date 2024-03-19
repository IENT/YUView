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

#include "three_dimensional_reference_displays_info.h"

#include <parser/common/Functions.h>

namespace parser::hevc
{

using namespace reader;

SEIParsingResult three_dimensional_reference_displays_info::parse(
    reader::SubByteReaderLogging &          reader,
    bool                                    reparse,
    VPSMap &                                vpsMap,
    SPSMap &                                spsMap,
    std::shared_ptr<seq_parameter_set_rbsp> associatedSPS)
{
  (void)reparse;
  (void)vpsMap;
  (void)spsMap;
  (void)associatedSPS;

  SubByteReaderLoggingSubLevel subLevel(reader, "three_dimensional_reference_displays_info");

  this->prec_ref_display_width    = reader.readUEV("prec_ref_display_width");
  this->ref_viewing_distance_flag = reader.readFlag("ref_viewing_distance_flag");
  if (this->ref_viewing_distance_flag)
    this->prec_ref_viewing_dist = reader.readUEV("prec_ref_viewing_dist");
  this->num_ref_displays_minus1 = reader.readUEV("num_ref_displays_minus1");
  for (unsigned i = 0; i <= num_ref_displays_minus1; ++i)
  {
    this->left_view_id.push_back(reader.readUEV(formatArray("left_view_id", i)));
    this->right_view_id.push_back(reader.readUEV(formatArray("right_view_id", i)));
    this->exponent_ref_display_width.push_back(
        reader.readBits(formatArray("exponent_ref_display_width", i), 6));

    const auto refDispWidthBits =
        this->exponent_ref_display_width.at(i) == 0
            ? std::max(0ull, this->prec_ref_display_width - 30)
            : std::max(0ull,
                       this->exponent_ref_display_width.at(i) + this->prec_ref_display_width - 31);
    this->mantissa_ref_display_width.push_back(
        reader.readBits(formatArray("mantissa_ref_display_width", i), refDispWidthBits));

    if (this->ref_viewing_distance_flag)
    {
      this->exponent_ref_viewing_distance.push_back(
          reader.readBits(formatArray("exponent_ref_viewing_distance", i), 6));

      const auto refViewDistBits = exponent_ref_viewing_distance.at(i) == 0
                                       ? std::max(0ull, this->prec_ref_viewing_dist - 30)
                                       : std::max(0ull,
                                                  this->exponent_ref_viewing_distance.at(i) +
                                                      this->prec_ref_viewing_dist - 31);
      this->mantissa_ref_viewing_distance.push_back(
          reader.readBits(formatArray("mantissa_ref_viewing_distance", i), refViewDistBits));
    }
    this->additional_shift_present_flag.push_back(
        reader.readFlag(formatArray("additional_shift_present_flag", i)));
    if (this->additional_shift_present_flag.at(i))
      this->num_sample_shift_plus512[i] =
          reader.readBits(formatArray("num_sample_shift_plus512", i), 10);
  }

  this->three_dimensional_reference_displays_extension_flag =
      reader.readFlag("three_dimensional_reference_displays_extension_flag");

  return SEIParsingResult::OK;
}

} // namespace parser::hevc