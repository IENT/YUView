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

#include "dpb_size.h"

#include <parser/common/Functions.h>

namespace parser::hevc
{

using namespace reader;

void dpb_size::parse(SubByteReaderLogging &   reader,
                     const unsigned           NumOutputLayerSets,
                     const umap_1d<unsigned> &OlsIdxToLsIdx,
                     const vector<unsigned> & MaxSubLayersInLayerSetMinus1,
                     const umap_1d<unsigned> &NumLayersInIdList,
                     const umap_2d<bool>      NecessaryLayerFlag,
                     const bool               vps_base_layer_internal_flag,
                     const umap_2d<unsigned> &LayerSetLayerIdList)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "dpb_size()");

  for (unsigned i = 1; i < NumOutputLayerSets; i++)
  {
    const auto currLsIdx = OlsIdxToLsIdx.at(i);
    this->sub_layer_flag_info_present_flag.push_back(
        reader.readFlag(formatArray("sub_layer_flag_info_present_flag", i)));
    for (unsigned j = 0; j <= MaxSubLayersInLayerSetMinus1.at(currLsIdx); j++)
    {
      if (j > 0 && this->sub_layer_flag_info_present_flag[i])
        this->sub_layer_dpb_info_present_flag[i][j] =
            reader.readFlag(formatArray("sub_layer_dpb_info_present_flag", i, j));
      if (this->sub_layer_dpb_info_present_flag[i][j])
      {
        for (unsigned k = 0; k < NumLayersInIdList.at(currLsIdx); k++)
          if (NecessaryLayerFlag.at(i).at(k) &&
              (vps_base_layer_internal_flag || (LayerSetLayerIdList.at(currLsIdx).at(k) != 0)))
            this->max_vps_dec_pic_buffering_minus1[i][k][j] =
                reader.readUEV(formatArray("max_vps_dec_pic_buffering_minus1", i, k, j));
        this->max_vps_num_reorder_pics[i][j] =
            reader.readUEV(formatArray("max_vps_num_reorder_pics", i, j));
        this->max_vps_latency_increase_plus1[i][j] =
            reader.readUEV(formatArray("max_vps_latency_increase_plus1", i, j));
      }
    }
  }
}

} // namespace parser::hevc
