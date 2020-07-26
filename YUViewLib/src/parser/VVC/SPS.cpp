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
#include "parser/common/ParserMacros.h"
#include "parser/common/ReaderHelper.h"

namespace VVC
{

bool extra_ph_bits_struct_t::parse(ReaderHelper &reader, unsigned int numExtraBtyes)
{
  ReaderSubLevel s(reader, "extra_ph_bits_struct()");
  for(unsigned i = 0; i < (numExtraBtyes * 8 ); i++)
  {
    READFLAG_A(extra_ph_bit_present_flag, i);
  }
  return true;
}

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

  CtbLog2SizeY = sps_log2_ctu_size_minus5 + 5;
  CtbSizeY = 1 << CtbLog2SizeY;

  READFLAG(this->sps_subpic_info_present_flag);
  if (sps_subpic_info_present_flag)
  {
    READUEV(sps_num_subpics_minus1);
    if (sps_num_subpics_minus1 > 0)
    {
      READFLAG(sps_independent_subpics_flag);
    }
    for(unsigned i = 0; sps_num_subpics_minus1 > 0 && i <= sps_num_subpics_minus1; i++)
    {
      if (i > 0 && sps_pic_width_max_in_luma_samples > CtbSizeY)
      {
        auto nrBits = ceil(log2((sps_pic_width_max_in_luma_samples + CtbSizeY - 1) >> CtbLog2SizeY));
        READBITS_A(sps_subpic_ctu_top_left_x, nrBits, i);
      }
      if (i > 0 && sps_pic_height_max_in_luma_samples > CtbSizeY)
      {
        auto nrBits = ceil(log2((sps_pic_height_max_in_luma_samples + CtbSizeY - 1) >> CtbLog2SizeY));
        READBITS_A(sps_subpic_ctu_top_left_y, nrBits, i);
      }
      if (i < sps_num_subpics_minus1 && sps_pic_width_max_in_luma_samples > CtbSizeY)
      {
        auto nrBits = ceil(log2((sps_pic_width_max_in_luma_samples + CtbSizeY - 1) >> CtbLog2SizeY));
        READBITS_A(sps_subpic_width_minus1, nrBits, i);
      }
      if (i < sps_num_subpics_minus1 && sps_pic_height_max_in_luma_samples > CtbSizeY)
      {
        auto nrBits = ceil(log2((sps_pic_height_max_in_luma_samples + CtbSizeY - 1) >> CtbLog2SizeY));
        READBITS_A(sps_subpic_height_minus1, nrBits, i);
      }
      if (!sps_independent_subpics_flag)
      {
        READFLAG_A(sps_subpic_treated_as_pic_flag, i);
        READFLAG_A(sps_loop_filter_across_subpic_enabled_flag, i);
      }
    }
  }

  READUEV(sps_subpic_id_len_minus1);
  READFLAG(sps_subpic_id_mapping_explicitly_signalled_flag);
  if (sps_subpic_id_mapping_explicitly_signalled_flag)
  {
    READFLAG(sps_subpic_id_mapping_present_flag);
    if (sps_subpic_id_mapping_present_flag)
    {
      for (unsigned i = 0; i <= sps_num_subpics_minus1; i++)
      {
        auto nrBits = sps_subpic_id_len_minus1 + 1;
        READBITS_A(sps_subpic_id, nrBits, i);
      }
    }
  }

  READUEV(sps_bit_depth_minus8);
  READFLAG(sps_entropy_coding_sync_enabled_flag);
  READFLAG(sps_entry_point_offsets_present_flag);
  READBITS(sps_log2_max_pic_order_cnt_lsb_minus4, 4);
  READFLAG(sps_poc_msb_cycle_flag);

  if (sps_poc_msb_cycle_flag)
  {
    READUEV(sps_poc_msb_cycle_len_minus1);
  }
  READBITS(sps_num_extra_ph_bits_bytes, 2);
  if (!extra_ph_bits_struct.parse(reader, sps_num_extra_ph_bits_bytes))
    return reader.addErrorMessageChildItem("Error parsing extra_ph_bits_struct");

  // ....
  // TODO: There is more to parse. But we don't need more for now.

  // Calculate some values
  NumExtraPhBits = 0;
  for (unsigned i = 0; i < (sps_num_extra_ph_bits_bytes * 8); i++)
  {
    if (extra_ph_bits_struct.extra_ph_bit_present_flag[i])
      NumExtraPhBits++;
  }

  MaxPicOrderCntLsb = pow(sps_log2_max_pic_order_cnt_lsb_minus4 + 4, 2);

  return false;
}

} // namespace VVC