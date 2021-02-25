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

#include "pic_parameter_set_rbsp.h"
#include "ref_pic_list_struct.h"
#include "seq_parameter_set_rbsp.h"
#include "slice_layer_rbsp.h"

namespace parser::vvc
{

using namespace parser::reader;

void pred_weight_table::parse(SubByteReaderLogging &                  reader,
                              std::shared_ptr<seq_parameter_set_rbsp> sps,
                              std::shared_ptr<pic_parameter_set_rbsp> pps,
                              std::shared_ptr<slice_layer_rbsp>       sl,
                              std::shared_ptr<ref_pic_lists>          rpl)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "pred_weight_table");

  this->luma_log2_weight_denom =
      reader.readUEV("luma_log2_weight_denom", Options().withCheckRange({0, 7}));
  if (sps->sps_chroma_format_idc != 0)
  {
    this->delta_chroma_log2_weight_denom = reader.readSEV("delta_chroma_log2_weight_denom");
  }
  if (pps->pps_wp_info_in_ph_flag)
  {
    this->num_l0_weights = reader.readUEV("num_l0_weights");
  }
  unsigned NumWeightsL0;
  if (pps->pps_wp_info_in_ph_flag)
    NumWeightsL0 = num_l0_weights;
  else
    NumWeightsL0 = sl->slice_header_instance.NumRefIdxActive[0];
  for (unsigned i = 0; i < NumWeightsL0; i++)
  {
    this->luma_weight_l0_flag.push_back(reader.readFlag("luma_weight_l0_flag"));
  }
  if (sps->sps_chroma_format_idc != 0)
  {
    for (unsigned i = 0; i < NumWeightsL0; i++)
    {
      this->chroma_weight_l0_flag.push_back(reader.readFlag("chroma_weight_l0_flag"));
    }
  }
  for (unsigned i = 0; i < NumWeightsL0; i++)
  {
    if (this->luma_weight_l0_flag[i])
    {
      this->delta_luma_weight_l0.push_back(reader.readSEV("delta_luma_weight_l0"));
      this->luma_offset_l0.push_back(reader.readSEV("luma_offset_l0"));
    }
    if (this->chroma_weight_l0_flag[i])
    {
      for (unsigned j = 0; j < 2; j++)
      {
        this->delta_chroma_weight_l0[i][j] = reader.readSEV("delta_chroma_weight_l0");
        this->delta_chroma_offset_l0[i][j] = reader.readSEV("delta_chroma_offset_l0");
      }
    }
  }
  if (pps->pps_weighted_bipred_flag && pps->pps_wp_info_in_ph_flag &&
      rpl->getActiveRefPixList(sps, 1).num_ref_entries > 0)
  {
    this->num_l1_weights =
        reader.readUEV("num_l1_weights",
                       Options().withCheckRange(
                           {0, std::min(15u, rpl->getActiveRefPixList(sps, 1).num_ref_entries)}));
  }

  // (144)
  unsigned NumWeightsL1;
  if (!pps->pps_weighted_bipred_flag ||
      (pps->pps_wp_info_in_ph_flag && rpl->getActiveRefPixList(sps, 1).num_ref_entries == 0))
    NumWeightsL1 = 0;
  else if (pps->pps_wp_info_in_ph_flag)
    NumWeightsL1 = this->num_l1_weights;
  else
    NumWeightsL1 = sl->slice_header_instance.NumRefIdxActive[1];

  for (unsigned i = 0; i < NumWeightsL1; i++)
  {
    this->luma_weight_l1_flag.push_back(reader.readFlag("luma_weight_l1_flag"));
  }
  if (sps->sps_chroma_format_idc != 0)
  {
    for (unsigned i = 0; i < NumWeightsL1; i++)
    {
      this->chroma_weight_l1_flag.push_back(reader.readFlag("chroma_weight_l1_flag"));
    }
  }
  for (unsigned i = 0; i < NumWeightsL1; i++)
  {
    if (this->luma_weight_l1_flag[i])
    {
      this->delta_luma_weight_l1.push_back(reader.readSEV("delta_luma_weight_l1"));
      this->luma_offset_l1.push_back(reader.readSEV("luma_offset_l1"));
    }
    this->delta_chroma_weight_l1.push_back({});
    this->delta_chroma_offset_l1.push_back({});
    if (this->chroma_weight_l1_flag[i])
    {
      for (unsigned j = 0; j < 2; j++)
      {
        this->delta_chroma_weight_l1[i].push_back(reader.readSEV("delta_chroma_weight_l1"));
        this->delta_chroma_offset_l1[i].push_back(reader.readSEV("delta_chroma_offset_l1"));
      }
    }
  }
}

} // namespace parser::vvc
