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

#include "pred_weight_table.h"

#include <parser/common/Functions.h>
#include "typedef.h"


namespace parser::avc

{
using namespace reader;

void pred_weight_table::parse(reader::SubByteReaderLogging &reader,
                              SliceType                     slice_type,
                              int                           ChromaArrayType,
                              unsigned                      num_ref_idx_l0_active_minus1,
                              unsigned                      num_ref_idx_l1_active_minus1)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "pred_weight_table()");

  this->luma_log2_weight_denom = reader.readUEV("luma_log2_weight_denom");
  if (ChromaArrayType != 0)
    this->chroma_log2_weight_denom = reader.readUEV("chroma_log2_weight_denom");
  for (unsigned i = 0; i <= num_ref_idx_l0_active_minus1; i++)
  {
    auto luma_weight_l0_flag = reader.readFlag("luma_weight_l0_flag");
    luma_weight_l0_flag_list.push_back(luma_weight_l0_flag);
    if (luma_weight_l0_flag)
    {
      this->luma_weight_l0.push_back(reader.readSEV(formatArray("luma_weight_l0", i)));
      this->luma_offset_l0.push_back(reader.readSEV(formatArray("luma_offset_l0", i)));
    }
    if (ChromaArrayType != 0)
    {
      auto chroma_weight_l0_flag = reader.readFlag("chroma_weight_l0_flag");
      chroma_weight_l0_flag_list.push_back(chroma_weight_l0_flag);
      if (chroma_weight_l0_flag)
      {
        for (unsigned j = 0; j < 2; j++)
        {
          this->chroma_weight_l0[j].push_back(
              reader.readSEV(formatArray("chroma_weight_l0", j, i)));
          this->chroma_offset_l0[j].push_back(
              reader.readSEV(formatArray("chroma_offset_l0", j, i)));
        }
      }
    }
  }
  if (slice_type == SliceType::SLICE_B)
  {
    for (unsigned i = 0; i <= num_ref_idx_l1_active_minus1; i++)
    {
      auto luma_weight_l1_flag = reader.readFlag("luma_weight_l1_flag");
      luma_weight_l1_flag_list.push_back(luma_weight_l1_flag);
      if (luma_weight_l1_flag)
      {
        this->luma_weight_l1.push_back(reader.readSEV(formatArray("luma_weight_l1", i)));
        this->luma_offset_l1.push_back(reader.readSEV(formatArray("luma_offset_l1", i)));
      }
      if (ChromaArrayType != 0)
      {
        auto chroma_weight_l1_flag = reader.readFlag("chroma_weight_l1_flag");
        chroma_weight_l1_flag_list.push_back(chroma_weight_l1_flag);
        if (chroma_weight_l1_flag)
        {
          for (unsigned j = 0; j < 2; j++)
          {
            this->chroma_weight_l1[i].push_back(
                reader.readSEV(formatArray("chroma_weight_l1", j, i)));
            this->chroma_offset_l1[i].push_back(
                reader.readSEV(formatArray("chroma_offset_l1", j, i)));
          }
        }
      }
    }
  }
}

} // namespace parser::avc