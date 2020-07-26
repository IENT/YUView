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

#include "PictureHeader.h"
#include "parser/common/parserMacros.h"
#include "parser/common/ReaderHelper.h"

namespace VVC
{

bool PictureHeader::parse(const QByteArray &parameterSetData, const SPS::SPSMap &active_SPS_list, const PPS::PPSMap &active_PPS_list, bool NoOutputBeforeRecoveryFlag, TreeItem *root)
{
  nalPayload = parameterSetData;

  ReaderHelper reader(parameterSetData, root, "picture_header_structure()");

  READFLAG(ph_gdr_or_irap_pic_flag);
  if (ph_gdr_or_irap_pic_flag)
  {
    READFLAG(ph_gdr_pic_flag);
  }
  READFLAG(ph_inter_slice_allowed_flag);
  if (ph_inter_slice_allowed_flag)
  {
    READFLAG(ph_intra_slice_allowed_flag);
  }
  READFLAG(ph_non_ref_pic_flag);
  READUEV(ph_pic_parameter_set_id);

  // Get the active PPS
  if (!active_PPS_list.contains(ph_pic_parameter_set_id))
    return reader.addErrorMessageChildItem("The signaled PPS was not found in the bitstream.");
  auto actPPS = active_PPS_list.value(ph_pic_parameter_set_id);

  // Get the active SPS
  if (!active_SPS_list.contains(actPPS->pps_seq_parameter_set_id))
    return reader.addErrorMessageChildItem("The signaled SPS was not found in the bitstream.");
  auto actSPS = active_SPS_list.value(actPPS->pps_seq_parameter_set_id);

  auto nrBits = actSPS->sps_log2_max_pic_order_cnt_lsb_minus4 + 4;
  READBITS(ph_pic_order_cnt_lsb, nrBits);

  if (ph_gdr_or_irap_pic_flag)
  {
    READFLAG(ph_no_output_of_prior_pics_flag);
  }
  if (ph_gdr_pic_flag)
  {
    READUEV(ph_recovery_poc_cnt);
  }
  for (unsigned i = 0; i < actSPS->NumExtraPhBits; i++)
  {
    READFLAG_A(ph_extra_bit, i);
  }
  if (actSPS->sps_poc_msb_cycle_flag)
  {
    READFLAG(ph_poc_msb_cycle_present_flag);
    if (ph_poc_msb_cycle_present_flag)
    {
      auto nrBits = actSPS->sps_poc_msb_cycle_len_minus1 + 1;
      READBITS(ph_poc_msb_cycle_val, nrBits);
    }
  }

  // ....
  // TODO: There is more to parse. But we don't need more for now.

  // Calculate POC

  auto isCLVSSPicture = (this->nal_unit_type == GDR_NUT || this->nal_unit_type == RSV_IRAP_11 || this->nal_unit_type == RSV_IRAP_12) && NoOutputBeforeRecoveryFlag;
  if (ph_poc_msb_cycle_present_flag)
  {
    PicOrderCntMsb = ph_poc_msb_cycle_val * actSPS->MaxPicOrderCntLsb;
  }
  else if (isCLVSSPicture)
  {
    PicOrderCntMsb = 0;
  }
  else
  {
    // if ((ph_pic_order_cnt_lsb < prevPicOrderCntLsb) && ((prevPicOrderCntLsb - ph_pic_order_cnt_lsb) >= (actSPS->MaxPicOrderCntLsb / 2)))
    //   PicOrderCntMsb = prevPicOrderCntMsb + actSPS->MaxPicOrderCntLsb;
    // else if ((ph_pic_order_cnt_lsb > prevPicOrderCntLsb) && ((ph_pic_order_cnt_lsb - prevPicOrderCntLsb) > (actSPS->MaxPicOrderCntLsb / 2)))
    //   PicOrderCntMsb = prevPicOrderCntMsb - actSPS->MaxPicOrderCntLsb;
    // else
    //   PicOrderCntMsb = prevPicOrderCntMsb;

  }
  PicOrderCntVal = PicOrderCntMsb + ph_pic_order_cnt_lsb;
}

} // namespace VVC

