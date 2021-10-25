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

#include "picture_coding_extension.h"

#include "parser/common/SubByteReaderLogging.h"
#include <parser/common/functions.h>

namespace parser::mpeg2
{

using namespace parser::reader;

void picture_coding_extension::parse(reader::SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "picture_coding_extension");

  this->f_code[0][0] = reader.readBits(formatArray("f_code", 0, 0), 4); // forward horizontal
  this->f_code[0][1] = reader.readBits(formatArray("f_code", 0, 1), 4); // forward vertical
  this->f_code[1][0] = reader.readBits(formatArray("f_code", 1, 0), 4); // backward horizontal
  this->f_code[1][1] = reader.readBits(formatArray("f_code", 1, 1), 4); // backward vertical

  this->intra_dc_precision = reader.readBits(
      "intra_dc_precision",
      2,
      Options().withMeaningVector(
          {"Precision 8 bit", "Precision 9 bit", "Precision 10 bit", "Precision 11 bit"}));
  this->picture_structure = reader.readBits(
      "picture_structure",
      2,
      Options().withMeaningVector({"Reserved", "Top Field", "Bottom Field", "Frame picture"}));

  this->top_field_first            = reader.readFlag("top_field_first");
  this->frame_pred_frame_dct       = reader.readFlag("frame_pred_frame_dct");
  this->concealment_motion_vectors = reader.readFlag("concealment_motion_vectors");
  this->q_scale_type               = reader.readFlag("q_scale_type");
  this->intra_vlc_format           = reader.readFlag("intra_vlc_format");
  this->alternate_scan             = reader.readFlag("alternate_scan");
  this->repeat_first_field         = reader.readFlag("repeat_first_field");
  this->chroma_420_type            = reader.readFlag("chroma_420_type");
  this->progressive_frame          = reader.readFlag("progressive_frame");
  this->composite_display_flag     = reader.readFlag("composite_display_flag");
  if (composite_display_flag)
  {
    this->v_axis            = reader.readFlag("v_axis");
    this->field_sequence    = reader.readBits("field_sequence", 3);
    this->sub_carrier       = reader.readFlag("sub_carrier");
    this->burst_amplitude   = reader.readBits("burst_amplitude", 7);
    this->sub_carrier_phase = reader.readBits("sub_carrier_phase", 8);
  }
}

} // namespace parser::mpeg2
