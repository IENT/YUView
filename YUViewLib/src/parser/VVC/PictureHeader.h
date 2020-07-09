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

#include "NalUnit.h"
#include "PPS.h"
#include "SPS.h"

namespace VVC
{

// The sequence parameter set.
struct PictureHeader : NalUnit
{
  PictureHeader(const NalUnit &nal) : NalUnit(nal) {}
  bool parse(const QByteArray &parameterSetData, const SPS::SPSMap &active_SPS_list, const PPS::PPSMap &active_PPS_list, bool NoOutputBeforeRecoveryFlag, TreeItem *root);

  bool ph_gdr_or_irap_pic_flag;
  bool ph_gdr_pic_flag;
  bool ph_inter_slice_allowed_flag;
  bool ph_intra_slice_allowed_flag;
  bool ph_non_ref_pic_flag;
  unsigned int ph_pic_parameter_set_id;
  unsigned int ph_pic_order_cnt_lsb;
  bool ph_no_output_of_prior_pics_flag;
  unsigned int ph_recovery_poc_cnt;
  QList<bool> ph_extra_bit;
  bool ph_poc_msb_cycle_present_flag {false};
  unsigned int ph_poc_msb_cycle_val;

  unsigned int PicOrderCntMsb;
  unsigned int PicOrderCntVal;
};

}