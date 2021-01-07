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

#include "sequence_extension.h"

#include "parser/common/functions.h"

namespace parser::mpeg2
{

using namespace parser::reader;

void sequence_extension::parse(SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "sequence_extension");

  this->profile_and_level_indication = reader.readBits("profile_and_level_indication", 8);

  this->profile_identification = (this->profile_and_level_indication >> 4) & 0x07;
  reader.logCalculatedValue(
      "profile_identification",
      this->profile_identification,
      Options()
          .withMeaningVector(
              {"Reserved", "High", "Spatially Scalable", "SNR Scalable", "Main", "Simple"})
          .withMeaning("Reserved"));

  this->level_identification = this->profile_and_level_indication & 0x03;
  reader.logCalculatedValue(
      "level_identification",
      this->level_identification,
      Options().withMeaningMap(
          {{0b0100, "High"}, {0b0110, "High 1440"}, {0b1000, "Main"}, {0b1010, "Low"}}));

  this->progressive_sequence = reader.readFlag(
      "progressive_sequence",
      Options().withMeaningMap(
          {{0,
            "the coded video sequence may contain both frame-pictures and field-pictures, and "
            "frame-picture may be progressive or interlaced frames."},
           {1, "the coded video sequence contains only progressive frame-pictures"}}));

  this->chroma_format = reader.readBits(
      "chroma_format",
      2,
      Options().withMeaningMap({{0, "Reserved"}, {1, "4:2:0"}, {2, "4:2:2"}, {3, "4:4:4"}}));
  this->horizontal_size_extension =
      reader.readBits("horizontal_size_extension",
                      2,
                      Options().withMeaning("most significant bits from horizontal_size"));
  this->vertical_size_extension =
      reader.readBits("vertical_size_extension",
                      2,
                      Options().withMeaning("most significant bits from vertical_size"));
  this->bit_rate_extension = reader.readBits(
      "bit_rate_extension", 12, Options().withMeaning("12 most significant bits from bit_rate"));
  this->marker_bit = reader.readFlag(
      "marker_bit",
      Options().withCheckEqualTo(
          1, "The marker_bit shall be set to 1 to prevent emulation of start codes."));
  this->vbv_buffer_size_extension =
      reader.readBits("vbv_buffer_size_extension",
                      8,
                      Options().withMeaning("most significant bits from vbv_buffer_size"));
  this->low_delay = reader.readFlag(
      "low_delay",
      Options().withMeaningMap(
          {{0,
            "sequence may contain B-pictures, the frame re-ordering delay is present in the VBV "
            "description and the bitstream shall not contain big pictures"},
           {1,
            "sequence does not contain any B-pictures, the frame e-ordering delay is not present "
            "in "
            "the VBV description and the bitstream may contain 'big pictures'"}}));
  this->frame_rate_extension_n = reader.readBits("frame_rate_extension_n", 2);
  this->frame_rate_extension_d = reader.readBits("frame_rate_extension_d", 5);
}

} // namespace parser::mpeg2
