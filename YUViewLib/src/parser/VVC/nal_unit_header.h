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

namespace parser::vvc
{

enum class NalType
{
  TRAIL_NUT,
  STSA_NUT,
  RADL_NUT,
  RASL_NUT,
  RSV_VCL_4,
  RSV_VCL_5,
  RSV_VCL_6,
  IDR_W_RADL,
  IDR_N_LP,
  CRA_NUT,
  GDR_NUT,
  RSV_IRAP_11,
  OPI_NUT,
  DCI_NUT,
  VPS_NUT,
  SPS_NUT,
  PPS_NUT,
  PREFIX_APS_NUT,
  SUFFIX_APS_NUT,
  PH_NUT,
  AUD_NUT,
  EOS_NUT,
  EOB_NUT,
  PREFIX_SEI_NUT,
  SUFFIX_SEI_NUT,
  FD_NUT,
  RSV_NVCL_26,
  RSV_NVCL_27,
  UNSPEC_28,
  UNSPEC_29,
  UNSPEC_30,
  UNSPEC_31,
  UNSPECIFIED
};

class nal_unit_header
{
public:
  nal_unit_header()  = default;
  ~nal_unit_header() = default;
  void parse(reader::SubByteReaderLogging &reader);

  bool isSlice() const
  {
    return this->nal_unit_type == NalType::TRAIL_NUT || this->nal_unit_type == NalType::STSA_NUT ||
           this->nal_unit_type == NalType::RADL_NUT || this->nal_unit_type == NalType::RASL_NUT ||
           this->nal_unit_type == NalType::IDR_W_RADL || this->nal_unit_type == NalType::IDR_N_LP ||
           this->nal_unit_type == NalType::CRA_NUT || this->nal_unit_type == NalType::GDR_NUT;
  }

  unsigned nuh_layer_id;
  unsigned nuh_temporal_id_plus1;

  NalType  nal_unit_type;
  unsigned nalUnitTypeID;
};

} // namespace parser::vvc
