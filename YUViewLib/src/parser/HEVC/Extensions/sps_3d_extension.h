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

namespace parser::hevc
{

// I.7.3.2.2.5 Sequence parameter set 3D extension syntax
class sps_3d_extension
{
public:
  sps_3d_extension() {}

  void parse(reader::SubByteReaderLogging &reader);

  bool     iv_di_mc_enabled_flag[2]{};
  bool     iv_mv_scal_enabled_flag[2]{};
  unsigned log2_ivmc_sub_pb_size_minus3[2]{};
  bool     iv_res_pred_enabled_flag[2]{};
  bool     depth_ref_enabled_flag[2]{};
  bool     vsp_mc_enabled_flag[2]{};
  bool     dbbp_enabled_flag[2]{};

  bool     tex_mc_enabled_flag[2]{};
  unsigned log2_texmc_sub_pb_size_minus3[2]{};
  bool     intra_contour_enabled_flag[2]{};
  bool     intra_dc_only_wedge_enabled_flag[2]{};
  bool     cqt_cu_part_pred_enabled_flag[2]{};
  bool     inter_dc_only_enabled_flag[2]{};
  bool     skip_intra_enabled_flag[2]{};
};

} // namespace parser::hevc
