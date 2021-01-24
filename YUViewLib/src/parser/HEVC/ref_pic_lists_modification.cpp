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

#include "ref_pic_lists_modification.h"

#include "parser/common/functions.h"
#include "slice_segment_header.h"

#include <cmath>

namespace parser::hevc
{

using namespace reader;

void ref_pic_lists_modification::parse(SubByteReaderLogging &      reader,
                                       unsigned                    NumPicTotalCurr,
                                       const slice_segment_header *slice)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "ref_pic_lists_modification()");

  int nrBits = std::ceil(std::log2(NumPicTotalCurr));

  this->ref_pic_list_modification_flag_l0 = reader.readFlag("ref_pic_list_modification_flag_l0");
  if (this->ref_pic_list_modification_flag_l0)
  {
    for (unsigned int i = 0; i <= slice->num_ref_idx_l0_active_minus1; i++)
      this->list_entry_l0.push_back(reader.readBits(formatArray("list_entry_l0", i), nrBits));
  }

  if (slice->slice_type == SliceType::B)
  {
    this->ref_pic_list_modification_flag_l1 = reader.readFlag("ref_pic_list_modification_flag_l1");
    if (ref_pic_list_modification_flag_l1)
      for (unsigned int i = 0; i <= slice->num_ref_idx_l1_active_minus1; i++)
        this->list_entry_l1.push_back(reader.readBits(formatArray("list_entry_l1", i), nrBits));
  }
}

} // namespace parser::hevc