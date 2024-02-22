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

#pragma once

#include "parser/common/SubByteReaderLogging.h"

#include "../hrd_parameters.h"

namespace parser::hevc
{

// F.7.3.2.2.4 Sequence parameter set multilayer extension syntax
class vps_vui_bsp_hrd_params
{
public:
  vps_vui_bsp_hrd_params() {}

  void parse(reader::SubByteReaderLogging &reader,
             const unsigned                vps_num_hrd_parameters,
             const unsigned                NumOutputLayerSets,
             const umap_1d<unsigned> &     NumLayersInIdList,
             const umap_1d<unsigned> &     OlsIdxToLsIdx,
             const vector<unsigned> &      MaxSubLayersInLayerSetMinus1);

  unsigned          vps_num_add_hrd_params{};
  umap_1d<bool>     cprms_add_present_flag;
  umap_1d<unsigned> num_sub_layer_hrd_minus1;

  umap_1d<hrd_parameters> hrdParameters;
  umap_1d<unsigned>       num_signalled_partitioning_schemes;
  umap_2d<unsigned>       num_partitions_in_scheme_minus1;
  umap_4d<bool>           layer_included_in_partition_flag;
  umap_3d<unsigned>       num_bsp_schedules_minus1;
  umap_5d<unsigned>       bsp_hrd_idx;
  umap_5d<unsigned>       bsp_sched_idx;
};

} // namespace parser::hevc
