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

#include "vps_vui_bsp_hrd_params.h"

#include <parser/common/Functions.h>

namespace parser::hevc
{

using namespace reader;

void vps_vui_bsp_hrd_params::parse(SubByteReaderLogging &   reader,
                                   const unsigned           vps_num_hrd_parameters,
                                   const unsigned           NumOutputLayerSets,
                                   const umap_1d<unsigned> &NumLayersInIdList,
                                   const umap_1d<unsigned> &OlsIdxToLsIdx,
                                   const vector<unsigned> & MaxSubLayersInLayerSetMinus1)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "vps_vui_bsp_hrd_params()");

  this->vps_num_add_hrd_params = reader.readUEV("vps_num_add_hrd_params");
  for (unsigned i = vps_num_hrd_parameters;
       i < vps_num_hrd_parameters + this->vps_num_add_hrd_params;
       i++)
  {
    if (i > 0)
      this->cprms_add_present_flag[i] = reader.readFlag(formatArray("cprms_add_present_flag", i));
    this->num_sub_layer_hrd_minus1[i] = reader.readUEV(formatArray("cprms_add_present_flag", i));
    hrdParameters[i].parse(
        reader, this->cprms_add_present_flag[i], this->num_sub_layer_hrd_minus1[i]);
  }

  if (vps_num_hrd_parameters + this->vps_num_add_hrd_params > 0)
    for (unsigned h = 1; h < NumOutputLayerSets; h++)
    {
      this->num_signalled_partitioning_schemes[h] =
          reader.readUEV(formatArray("cprms_add_present_flag", h));
      for (unsigned j = 1; j < num_signalled_partitioning_schemes[h] + 1; j++)
      {
        this->num_partitions_in_scheme_minus1[h][j] =
            reader.readUEV(formatArray("num_partitions_in_scheme_minus1", h, j));
        for (unsigned k = 0; k <= this->num_partitions_in_scheme_minus1[h][j]; k++)
          for (unsigned r = 0; r < NumLayersInIdList.at(OlsIdxToLsIdx.at(h)); r++)
            layer_included_in_partition_flag[h][j][k][r] =
                reader.readFlag(formatArray("layer_included_in_partition_flag", h, j, k, r));
      }
      for (unsigned i = 0; i < this->num_signalled_partitioning_schemes[h] + 1; i++)
        for (unsigned t = 0; t <= MaxSubLayersInLayerSetMinus1[OlsIdxToLsIdx.at(h)]; t++)
        {
          this->num_bsp_schedules_minus1[h][i][t] =
              reader.readUEV(formatArray("num_bsp_schedules_minus1", h, i, t));
          for (unsigned j = 0; j <= this->num_bsp_schedules_minus1[h][i][t]; j++)
            for (unsigned k = 0; k <= this->num_partitions_in_scheme_minus1[h][i]; k++)
            {
              if (vps_num_hrd_parameters + this->vps_num_add_hrd_params > 1)
              {
                const auto nrBits =
                    std::ceil(std::log2(vps_num_hrd_parameters + this->vps_num_add_hrd_params));
                this->bsp_hrd_idx[h][i][t][j][k] =
                    reader.readBits(formatArray("bsp_sched_idx", h, i, t, j, k), nrBits);
              }
              this->bsp_sched_idx[h][i][t][j][k] =
                  reader.readUEV(formatArray("bsp_sched_idx", h, i, t, j, k));
            }
        }
    }
}

} // namespace parser::hevc
