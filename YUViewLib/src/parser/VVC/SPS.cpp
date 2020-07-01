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

#include "SPS.h"
#include "parser/common/parserMacros.h"
#include "parser/common/ReaderHelper.h"

namespace VVC
{

bool SPS::parse(const QByteArray &parameterSetData, TreeItem *root)
{
  nalPayload = parameterSetData;

  ReaderHelper reader(parameterSetData, root, "seq_parameter_set_rbsp()");

  READBITS(this->sps_seq_parameter_set_id,4);
  READBITS(this->sps_video_parameter_set_id,4);
  READBITS(this->sps_max_sublayers_minus1,3);
  READBITS(this->sps_reserved_zero_4bits,4);
  READFLAG(this->sps_ptl_dpb_hrd_params_present_flag);

  if(this->sps_ptl_dpb_hrd_params_present_flag)
  {
    if (!this->profileTierLevel.parse(reader, true, this->sps_max_sublayers_minus1))
      return false;
  }
  
  READFLAG(this->sps_gdr_enabled_flag);
  READBITS(this->sps_chroma_format_idc, 2);
  if (this->sps_chroma_format_idc == 3)
    READFLAG(this->sps_separate_colour_plane_flag);
  READFLAG(this->sps_ref_pic_resampling_enabled_flag);
  if(this->sps_ref_pic_resampling_enabled_flag)
    READFLAG(this->sps_res_change_in_clvs_allowed_flag);
  READUEV(this->sps_pic_width_max_in_luma_samples);
  READUEV(this->sps_pic_height_max_in_luma_samples);
  READFLAG(this->sps_conformance_window_flag);
  if (sps_conformance_window_flag)
  {
    READUEV(this->sps_conf_win_left_offset);
    READUEV(this->sps_conf_win_right_offset);
    READUEV(this->sps_conf_win_top_offset);
    READUEV(this->sps_conf_win_bottom_offset);
  }
  READBITS(this->sps_log2_ctu_size_minus5, 2);
  READFLAG(this->sps_subpic_info_present_flag);

  // ....
  // TODO: There is more to parse
}

} // namespace VVC