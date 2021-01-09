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

#include "sequence_header.h"

#include "parser/common/functions.h"

namespace parser::mpeg2
{

using namespace parser::reader;

void sequence_header::parse(SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "sequence_header");

  this->horizontal_size_value = reader.readBits("horizontal_size_value", 12);
  this->vertical_size_value   = reader.readBits("vertical_size_value", 12);

  this->aspect_ratio_information = reader.readBits(
      "aspect_ratio_information",
      4,
      Options()
          .withMeaningVector(
              {"Forbidden", "SAR 1.0 (Square Sample)", "DAR 3:4", "DAR 9:16", "DAR 1:2.21"})
          .withMeaning("Reserved"));

  this->frame_rate_code = reader.readBits("frame_rate_code",
                                          4,
                                          Options()
                                              .withMeaningVector({"Forbidden",
                                                                  "24000:1001 (23.976...)",
                                                                  "24",
                                                                  "25",
                                                                  "30000:1001 (29.97...)",
                                                                  "30",
                                                                  "50",
                                                                  "60000:1001 (59.94)",
                                                                  "60"})
                                              .withMeaning("Reserved"));

  this->bit_rate_value = reader.readBits(
      "bit_rate_value", 18, Options().withMeaning("The lower 18 bits of bit_rate."));
  this->marker_bit            = reader.readFlag("marker_bit", Options().withCheckEqualTo(1));
  this->vbv_buffer_size_value = reader.readBits(
      "vbv_buffer_size_value", 10, Options().withMeaning("the lower 10 bits of vbv_buffer_size"));
  this->constrained_parameters_flag = reader.readFlag("constrained_parameters_flag");
  this->load_intra_quantiser_matrix = reader.readFlag("load_intra_quantiser_matrix");
  if (this->load_intra_quantiser_matrix)
  {
    for (unsigned i = 0; i < 64; i++)
      this->intra_quantiser_matrix[i] =
          reader.readBits(formatArray("intra_quantiser_matrix", i), 8);
  }
  this->load_non_intra_quantiser_matrix = reader.readFlag("load_non_intra_quantiser_matrix");
  if (this->load_non_intra_quantiser_matrix)
  {
    for (unsigned i = 0; i < 64; i++)
      this->non_intra_quantiser_matrix[i] =
          reader.readBits(formatArray("non_intra_quantiser_matrix", i), 8);
  }
}

} // namespace parser::mpeg2
