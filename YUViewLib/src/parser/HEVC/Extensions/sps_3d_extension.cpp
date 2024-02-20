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

#include "sps_3d_extension.h"

#include <parser/common/Functions.h>

namespace parser::hevc
{

using namespace reader;

void sps_3d_extension::parse(SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "sps_3d_extension()");

  for (int d = 0; d <= 1; d++)
  {
    this->iv_di_mc_enabled_flag[d]   = reader.readFlag(formatArray("iv_di_mc_enabled_flag", d));
    this->iv_mv_scal_enabled_flag[d] = reader.readFlag(formatArray("iv_mv_scal_enabled_flag", d));
    if (d == 0)
    {
      this->log2_ivmc_sub_pb_size_minus3[d] =
          reader.readUEV(formatArray("log2_ivmc_sub_pb_size_minus3", d));
      this->iv_res_pred_enabled_flag[d] =
          reader.readFlag(formatArray("iv_res_pred_enabled_flag", d));
      this->depth_ref_enabled_flag[d] = reader.readFlag(formatArray("depth_ref_enabled_flag", d));
      this->vsp_mc_enabled_flag[d]    = reader.readFlag(formatArray("vsp_mc_enabled_flag", d));
      this->dbbp_enabled_flag[d]      = reader.readFlag(formatArray("dbbp_enabled_flag", d));
    }
    else
    {
      this->tex_mc_enabled_flag[d] = reader.readFlag(formatArray("tex_mc_enabled_flag", d));
      this->log2_texmc_sub_pb_size_minus3[d] =
          reader.readUEV(formatArray("log2_texmc_sub_pb_size_minus3", d));
      this->intra_contour_enabled_flag[d] =
          reader.readFlag(formatArray("intra_contour_enabled_flag", d));
      this->intra_dc_only_wedge_enabled_flag[d] =
          reader.readFlag(formatArray("intra_dc_only_wedge_enabled_flag", d));
      this->cqt_cu_part_pred_enabled_flag[d] =
          reader.readFlag(formatArray("cqt_cu_part_pred_enabled_flag", d));
      this->inter_dc_only_enabled_flag[d] =
          reader.readFlag(formatArray("inter_dc_only_enabled_flag", d));
      this->skip_intra_enabled_flag[d] = reader.readFlag(formatArray("skip_intra_enabled_flag", d));
    }
  }
}

} // namespace parser::hevc
