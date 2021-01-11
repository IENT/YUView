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

#include "loop_filter_params.h"

#include "sequence_header_obu.h"
#include "typedef.h"
#include "parser/common/functions.h"

namespace parser::av1
{

using namespace reader;

void loop_filter_params::parse(reader::SubByteReaderLogging &reader, std::shared_ptr<sequence_header_obu> seqHeader, bool CodedLossless, bool allow_intrabc)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "loop_filter_params()");
  
  if (CodedLossless || allow_intrabc)
  {
    this->loop_filter_level[0] = 0;
    this->loop_filter_level[1] = 0;
    this->loop_filter_ref_deltas[INTRA_FRAME] = 1;
    this->loop_filter_ref_deltas[LAST_FRAME] = 0;
    this->loop_filter_ref_deltas[LAST2_FRAME] = 0;
    this->loop_filter_ref_deltas[LAST3_FRAME] = 0;
    this->loop_filter_ref_deltas[BWDREF_FRAME] = 0;
    this->loop_filter_ref_deltas[GOLDEN_FRAME] = -1;
    this->loop_filter_ref_deltas[ALTREF_FRAME] = -1;
    this->loop_filter_ref_deltas[ALTREF2_FRAME] = -1;
    for (unsigned i = 0; i < 2; i++)
      this->loop_filter_mode_deltas[i] = 0;
    return;
  }

  this->loop_filter_level[0] = reader.readBits("loop_filter_level[0]", 6);
  this->loop_filter_level[1] = reader.readBits("loop_filter_level[1]", 6);
  if (seqHeader->color_config.NumPlanes > 1)
  {
    if (loop_filter_level[0] || loop_filter_level[1])
    {
      this->loop_filter_level[2] = reader.readBits("loop_filter_level[2]", 6);
      this->loop_filter_level[3] = reader.readBits("loop_filter_level[3]", 6);
    }
  }
  this->loop_filter_sharpness = reader.readBits("loop_filter_sharpness", 3);
  this->loop_filter_delta_enabled = reader.readFlag("loop_filter_delta_enabled");
  if (this->loop_filter_delta_enabled)
  {
    this->loop_filter_delta_update = reader.readFlag("loop_filter_delta_update");
    if (this->loop_filter_delta_update)
    {
      for (unsigned i = 0; i < TOTAL_REFS_PER_FRAME; i++)
      {
        if (reader.readFlag("update_ref_delta"))
          this->loop_filter_ref_deltas[i] = reader.readSU(formatArray("loop_filter_ref_deltas", i), 1+6);
      }
      for (unsigned i = 0; i < 2; i++)
      {
        if (reader.readFlag("update_mode_delta"))
          this->loop_filter_mode_deltas[i] = reader.readSU(formatArray("loop_filter_mode_deltas", i), 1+6);
      }
    }
  }
}

} // namespace parser::av1
