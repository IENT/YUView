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

#include "NalUnitVVC.h"
#include "ProfileTierLevel.h"

namespace VVC
{

// The sequence parameter set.
struct SPS : NalUnitVVC
{
  SPS(const NalUnitVVC &nal) : NalUnitVVC(nal) {}
  bool parse(const QByteArray &parameterSetData, TreeItem *root);

  unsigned int sps_seq_parameter_set_id;
  unsigned int sps_video_parameter_set_id;
  unsigned int sps_max_sublayers_minus1;
  unsigned int sps_reserved_zero_4bits;
  bool sps_ptl_dpb_hrd_params_present_flag;

  ProfileTierLevel profileTierLevel;

  bool sps_gdr_enabled_flag;
  unsigned int sps_chroma_format_idc;
  bool sps_separate_colour_plane_flag;
  bool sps_ref_pic_resampling_enabled_flag;
  bool sps_res_change_in_clvs_allowed_flag;

  unsigned int sps_pic_width_max_in_luma_samples;
  unsigned int sps_pic_height_max_in_luma_samples;
  bool sps_conformance_window_flag;

  unsigned int sps_conf_win_left_offset;
  unsigned int sps_conf_win_right_offset;
  unsigned int sps_conf_win_top_offset;
  unsigned int sps_conf_win_bottom_offset;

  unsigned int sps_log2_ctu_size_minus5;
  bool sps_subpic_info_present_flag;
};

}