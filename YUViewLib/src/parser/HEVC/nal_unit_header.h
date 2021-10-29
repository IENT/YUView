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

#include "common/EnumMapper.h"
#include "parser/common/SubByteReaderLogging.h"

namespace parser::hevc
{

enum class NalType
{
  TRAIL_N,
  TRAIL_R,
  TSA_N,
  TSA_R,
  STSA_N,
  STSA_R,
  RADL_N,
  RADL_R,
  RASL_N,
  RASL_R,
  RSV_VCL_N10,
  RSV_VCL_N12,
  RSV_VCL_N14,
  RSV_VCL_R11,
  RSV_VCL_R13,
  RSV_VCL_R15,
  BLA_W_LP,
  BLA_W_RADL,
  BLA_N_LP,
  IDR_W_RADL,
  IDR_N_LP,
  CRA_NUT,
  RSV_IRAP_VCL22,
  RSV_IRAP_VCL23,
  RSV_VCL24,
  RSV_VCL25,
  RSV_VCL26,
  RSV_VCL27,
  RSV_VCL28,
  RSV_VCL29,
  RSV_VCL30,
  RSV_VCL31,
  VPS_NUT,
  SPS_NUT,
  PPS_NUT,
  AUD_NUT,
  EOS_NUT,
  EOB_NUT,
  FD_NUT,
  PREFIX_SEI_NUT,
  SUFFIX_SEI_NUT,
  RSV_NVCL41,
  RSV_NVCL42,
  RSV_NVCL43,
  RSV_NVCL44,
  RSV_NVCL45,
  RSV_NVCL46,
  RSV_NVCL47,
  UNSPEC48,
  UNSPEC49,
  UNSPEC50,
  UNSPEC51,
  UNSPEC52,
  UNSPEC53,
  UNSPEC54,
  UNSPEC55,
  UNSPEC56,
  UNSPEC57,
  UNSPEC58,
  UNSPEC69,
  UNSPEC60,
  UNSPEC61,
  UNSPEC62,
  UNSPEC63,
  UNSPECIFIED
};

const EnumMapper<NalType> NalTypeMapper({{NalType::TRAIL_N, "TRAIL_N"},
                                         {NalType::TRAIL_R, "TRAIL_R"},
                                         {NalType::TSA_N, "TSA_N"},
                                         {NalType::TSA_R, "TSA_R"},
                                         {NalType::STSA_N, "STSA_N"},
                                         {NalType::STSA_R, "STSA_R"},
                                         {NalType::RADL_N, "RADL_N"},
                                         {NalType::RADL_R, "RADL_R"},
                                         {NalType::RASL_N, "RASL_N"},
                                         {NalType::RASL_R, "RASL_R"},
                                         {NalType::RSV_VCL_N10, "RSV_VCL_N10"},
                                         {NalType::RSV_VCL_N12, "RSV_VCL_N12"},
                                         {NalType::RSV_VCL_N14, "RSV_VCL_N14"},
                                         {NalType::RSV_VCL_R11, "RSV_VCL_R11"},
                                         {NalType::RSV_VCL_R13, "RSV_VCL_R13"},
                                         {NalType::RSV_VCL_R15, "RSV_VCL_R15"},
                                         {NalType::BLA_W_LP, "BLA_W_LP"},
                                         {NalType::BLA_W_RADL, "BLA_W_RADL"},
                                         {NalType::BLA_N_LP, "BLA_N_LP"},
                                         {NalType::IDR_W_RADL, "IDR_W_RADL"},
                                         {NalType::IDR_N_LP, "IDR_N_LP"},
                                         {NalType::CRA_NUT, "CRA_NUT"},
                                         {NalType::RSV_IRAP_VCL22, "RSV_IRAP_VCL22"},
                                         {NalType::RSV_IRAP_VCL23, "RSV_IRAP_VCL23"},
                                         {NalType::RSV_VCL24, "RSV_VCL24"},
                                         {NalType::RSV_VCL25, "RSV_VCL25"},
                                         {NalType::RSV_VCL26, "RSV_VCL26"},
                                         {NalType::RSV_VCL27, "RSV_VCL27"},
                                         {NalType::RSV_VCL28, "RSV_VCL28"},
                                         {NalType::RSV_VCL29, "RSV_VCL29"},
                                         {NalType::RSV_VCL30, "RSV_VCL30"},
                                         {NalType::RSV_VCL31, "RSV_VCL31"},
                                         {NalType::VPS_NUT, "VPS_NUT"},
                                         {NalType::SPS_NUT, "SPS_NUT"},
                                         {NalType::PPS_NUT, "PPS_NUT"},
                                         {NalType::AUD_NUT, "AUD_NUT"},
                                         {NalType::EOS_NUT, "EOS_NUT"},
                                         {NalType::EOB_NUT, "EOB_NUT"},
                                         {NalType::FD_NUT, "FD_NUT"},
                                         {NalType::PREFIX_SEI_NUT, "PREFIX_SEI_NUT"},
                                         {NalType::SUFFIX_SEI_NUT, "SUFFIX_SEI_NUT"},
                                         {NalType::RSV_NVCL41, "RSV_NVCL41"},
                                         {NalType::RSV_NVCL42, "RSV_NVCL42"},
                                         {NalType::RSV_NVCL43, "RSV_NVCL43"},
                                         {NalType::RSV_NVCL44, "RSV_NVCL44"},
                                         {NalType::RSV_NVCL45, "RSV_NVCL45"},
                                         {NalType::RSV_NVCL46, "RSV_NVCL46"},
                                         {NalType::RSV_NVCL47, "RSV_NVCL47"},
                                         {NalType::UNSPEC48, "UNSPEC48"},
                                         {NalType::UNSPEC49, "UNSPEC49"},
                                         {NalType::UNSPEC50, "UNSPEC50"},
                                         {NalType::UNSPEC51, "UNSPEC51"},
                                         {NalType::UNSPEC52, "UNSPEC52"},
                                         {NalType::UNSPEC53, "UNSPEC53"},
                                         {NalType::UNSPEC54, "UNSPEC54"},
                                         {NalType::UNSPEC55, "UNSPEC55"},
                                         {NalType::UNSPEC56, "UNSPEC56"},
                                         {NalType::UNSPEC57, "UNSPEC57"},
                                         {NalType::UNSPEC58, "UNSPEC58"},
                                         {NalType::UNSPEC69, "UNSPEC69"},
                                         {NalType::UNSPEC60, "UNSPEC60"},
                                         {NalType::UNSPEC61, "UNSPEC61"},
                                         {NalType::UNSPEC62, "UNSPEC62"},
                                         {NalType::UNSPEC63, "UNSPEC63"},
                                         {NalType::UNSPECIFIED, "UNSPECIFIED"}});

class nal_unit_header
{
public:
  nal_unit_header()  = default;
  ~nal_unit_header() = default;
  void parse(reader::SubByteReaderLogging &reader);

  bool isIRAP() const;
  bool isSLNR() const;
  bool isRADL() const;
  bool isRASL() const;
  bool isSlice() const;

  unsigned nuh_layer_id;
  unsigned nuh_temporal_id_plus1;

  NalType  nal_unit_type;
  unsigned nalUnitTypeID;
};

} // namespace parser::hevc
