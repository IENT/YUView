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

#include "parser/common/NalUnit.h"

namespace VVC
{

enum NalUnitType
{
  TRAIL_NUT, STSA_NUT, RADL_NUT, RASL_NUT, RSV_VCL_4, RSV_VCL_5, RSV_VCL_6, IDR_W_RADL, IDR_N_LP, CRA_NUT, 
  GDR_NUT, RSV_IRAP_11, RSV_IRAP_12, DCI_NUT, VPS_NUT, SPS_NUT, PPS_NUT, PREFIX_APS_NUT, SUFFIX_APS_NUT, PH_NUT,
  AUD_NUT, EOS_NUT, EOB_NUT, PREFIX_SEI_NUT, SUFFIX_SEI_NUT, FD_NUT, RSV_NVCL_26, RSV_NVCL_27, UNSPEC_28, UNSPEC_29,
  UNSPEC_30, UNSPEC_31, UNSPECIFIED
};

/* The basic VVC NAL unit. Additionally to the basic NAL unit, it knows the VVC nal unit types.
  */
struct NalUnitVVC : NalUnit
{
  NalUnitVVC(QUint64Pair filePosStartEnd, int nal_idx) : NalUnit(filePosStartEnd, nal_idx) {}
  NalUnitVVC(QSharedPointer<NalUnitVVC> nal_src) : NalUnit(nal_src->filePosStartEnd, nal_src->nal_idx) { nal_unit_type_id = nal_src->nal_unit_type_id; nuh_layer_id = nal_src->nuh_layer_id; nuh_temporal_id_plus1 = nal_src->nuh_temporal_id_plus1; }
  virtual ~NalUnitVVC() {}

  virtual QByteArray getNALHeader() const override;
  virtual bool isParameterSet() const override { return false; }  // We don't know yet
  bool parse_nal_unit_header(const QByteArray &parameterSetData, TreeItem *root) override;

  bool isAUDelimiter() { return nal_unit_type_id == 20; }

  // The information of the NAL unit header
  unsigned int nuh_layer_id;
  unsigned int nuh_temporal_id_plus1;
  NalUnitType nal_unit_type;
};

} // Namespace VVC