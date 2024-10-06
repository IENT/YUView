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

#include <common/EnumMapper.h>
#include <parser/common/SubByteReaderLogging.h>

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

constexpr EnumMapper<NalType, 65>
    NalTypeMapper(std::make_pair(NalType::TRAIL_N, "TRAIL_N"sv),
                  std::make_pair(NalType::TRAIL_R, "TRAIL_R"sv),
                  std::make_pair(NalType::TSA_N, "TSA_N"sv),
                  std::make_pair(NalType::TSA_R, "TSA_R"sv),
                  std::make_pair(NalType::STSA_N, "STSA_N"sv),
                  std::make_pair(NalType::STSA_R, "STSA_R"sv),
                  std::make_pair(NalType::RADL_N, "RADL_N"sv),
                  std::make_pair(NalType::RADL_R, "RADL_R"sv),
                  std::make_pair(NalType::RASL_N, "RASL_N"sv),
                  std::make_pair(NalType::RASL_R, "RASL_R"sv),
                  std::make_pair(NalType::RSV_VCL_N10, "RSV_VCL_N10"sv),
                  std::make_pair(NalType::RSV_VCL_N12, "RSV_VCL_N12"sv),
                  std::make_pair(NalType::RSV_VCL_N14, "RSV_VCL_N14"sv),
                  std::make_pair(NalType::RSV_VCL_R11, "RSV_VCL_R11"sv),
                  std::make_pair(NalType::RSV_VCL_R13, "RSV_VCL_R13"sv),
                  std::make_pair(NalType::RSV_VCL_R15, "RSV_VCL_R15"sv),
                  std::make_pair(NalType::BLA_W_LP, "BLA_W_LP"sv),
                  std::make_pair(NalType::BLA_W_RADL, "BLA_W_RADL"sv),
                  std::make_pair(NalType::BLA_N_LP, "BLA_N_LP"sv),
                  std::make_pair(NalType::IDR_W_RADL, "IDR_W_RADL"sv),
                  std::make_pair(NalType::IDR_N_LP, "IDR_N_LP"sv),
                  std::make_pair(NalType::CRA_NUT, "CRA_NUT"sv),
                  std::make_pair(NalType::RSV_IRAP_VCL22, "RSV_IRAP_VCL22"sv),
                  std::make_pair(NalType::RSV_IRAP_VCL23, "RSV_IRAP_VCL23"sv),
                  std::make_pair(NalType::RSV_VCL24, "RSV_VCL24"sv),
                  std::make_pair(NalType::RSV_VCL25, "RSV_VCL25"sv),
                  std::make_pair(NalType::RSV_VCL26, "RSV_VCL26"sv),
                  std::make_pair(NalType::RSV_VCL27, "RSV_VCL27"sv),
                  std::make_pair(NalType::RSV_VCL28, "RSV_VCL28"sv),
                  std::make_pair(NalType::RSV_VCL29, "RSV_VCL29"sv),
                  std::make_pair(NalType::RSV_VCL30, "RSV_VCL30"sv),
                  std::make_pair(NalType::RSV_VCL31, "RSV_VCL31"sv),
                  std::make_pair(NalType::VPS_NUT, "VPS_NUT"sv),
                  std::make_pair(NalType::SPS_NUT, "SPS_NUT"sv),
                  std::make_pair(NalType::PPS_NUT, "PPS_NUT"sv),
                  std::make_pair(NalType::AUD_NUT, "AUD_NUT"sv),
                  std::make_pair(NalType::EOS_NUT, "EOS_NUT"sv),
                  std::make_pair(NalType::EOB_NUT, "EOB_NUT"sv),
                  std::make_pair(NalType::FD_NUT, "FD_NUT"sv),
                  std::make_pair(NalType::PREFIX_SEI_NUT, "PREFIX_SEI_NUT"sv),
                  std::make_pair(NalType::SUFFIX_SEI_NUT, "SUFFIX_SEI_NUT"sv),
                  std::make_pair(NalType::RSV_NVCL41, "RSV_NVCL41"sv),
                  std::make_pair(NalType::RSV_NVCL42, "RSV_NVCL42"sv),
                  std::make_pair(NalType::RSV_NVCL43, "RSV_NVCL43"sv),
                  std::make_pair(NalType::RSV_NVCL44, "RSV_NVCL44"sv),
                  std::make_pair(NalType::RSV_NVCL45, "RSV_NVCL45"sv),
                  std::make_pair(NalType::RSV_NVCL46, "RSV_NVCL46"sv),
                  std::make_pair(NalType::RSV_NVCL47, "RSV_NVCL47"sv),
                  std::make_pair(NalType::UNSPEC48, "UNSPEC48"sv),
                  std::make_pair(NalType::UNSPEC49, "UNSPEC49"sv),
                  std::make_pair(NalType::UNSPEC50, "UNSPEC50"sv),
                  std::make_pair(NalType::UNSPEC51, "UNSPEC51"sv),
                  std::make_pair(NalType::UNSPEC52, "UNSPEC52"sv),
                  std::make_pair(NalType::UNSPEC53, "UNSPEC53"sv),
                  std::make_pair(NalType::UNSPEC54, "UNSPEC54"sv),
                  std::make_pair(NalType::UNSPEC55, "UNSPEC55"sv),
                  std::make_pair(NalType::UNSPEC56, "UNSPEC56"sv),
                  std::make_pair(NalType::UNSPEC57, "UNSPEC57"sv),
                  std::make_pair(NalType::UNSPEC58, "UNSPEC58"sv),
                  std::make_pair(NalType::UNSPEC69, "UNSPEC69"sv),
                  std::make_pair(NalType::UNSPEC60, "UNSPEC60"sv),
                  std::make_pair(NalType::UNSPEC61, "UNSPEC61"sv),
                  std::make_pair(NalType::UNSPEC62, "UNSPEC62"sv),
                  std::make_pair(NalType::UNSPEC63, "UNSPEC63"sv),
                  std::make_pair(NalType::UNSPECIFIED, "UNSPECIFIED"sv));

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
