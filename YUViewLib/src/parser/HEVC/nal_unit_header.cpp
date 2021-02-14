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

#include "nal_unit_header.h"
#include "parser/common/CodingEnum.h"

namespace parser::hevc
{

using namespace parser::reader;

namespace
{

parser::CodingEnum<NalType> nalTypeCoding(
    {{0,
      NalType::TRAIL_N,
      "TRAIL_N",
      "Coded slice segment of a non-TSA, non-STSA trailing picture"},
     {1,
      NalType::TRAIL_R,
      "TRAIL_R",
      "Coded slice segment of a non-TSA, non-STSA trailing picture"},
     {2, NalType::TSA_N, "TSA_N", "Coded slice segment of a TSA picture"},
     {3, NalType::TSA_R, "TSA_R", "Coded slice segment of a TSA picture"},
     {4, NalType::STSA_N, "STSA_N", "Coded slice segment of an STSA picture"},
     {5, NalType::STSA_R, "STSA_R", "Coded slice segment of an STSA picture"},
     {6, NalType::RADL_N, "RADL_N", "Coded slice segment of a RADL picture"},
     {7, NalType::RADL_R, "RADL_R", "Coded slice segment of a RADL picture"},
     {8, NalType::RASL_N, "RASL_N", "Coded slice segment of a RASL picture"},
     {9, NalType::RASL_R, "RASL_R", "Coded slice segment of a RASL picture"},
     {10, NalType::RSV_VCL_N10, "RSV_VCL_N10", "Reserved non-IRAP SLNR VCL NAL unit types"},
     {12, NalType::RSV_VCL_N12, "RSV_VCL_N12", "Reserved non-IRAP SLNR VCL NAL unit types"},
     {14, NalType::RSV_VCL_N14, "RSV_VCL_N14", "Reserved non-IRAP SLNR VCL NAL unit types"},
     {11,
      NalType::RSV_VCL_R11,
      "RSV_VCL_R11",
      "Reserved non-IRAP sub-layer reference VCL NAL unit types"},
     {13,
      NalType::RSV_VCL_R13,
      "RSV_VCL_R13",
      "Reserved non-IRAP sub-layer reference VCL NAL unit types"},
     {15,
      NalType::RSV_VCL_R15,
      "RSV_VCL_R15",
      "Reserved non-IRAP sub-layer reference VCL NAL unit types"},
     {16, NalType::BLA_W_LP, "BLA_W_LP", "Coded slice segment of a BLA picture"},
     {17, NalType::BLA_W_RADL, "BLA_W_RADL", "Coded slice segment of a BLA picture"},
     {18, NalType::BLA_N_LP, "BLA_N_LP", "Coded slice segment of a BLA picture"},
     {19, NalType::IDR_W_RADL, "IDR_W_RADL", "Coded slice segment of an IDR picture"},
     {20, NalType::IDR_N_LP, "IDR_N_LP", "Coded slice segment of an IDR picture"},
     {21, NalType::CRA_NUT, "CRA_NUT", "Coded slice segment of a CRA picture"},
     {22, NalType::RSV_IRAP_VCL22, "RSV_IRAP_VCL22", "Reserved IRAP VCL NAL unit types"},
     {23, NalType::RSV_IRAP_VCL23, "RSV_IRAP_VCL23", "Reserved IRAP VCL NAL unit types"},
     {24, NalType::RSV_VCL24, "RSV_VCL24", "Reserved non-IRAP VCL NAL unit types"},
     {25, NalType::RSV_VCL25, "RSV_VCL25", "Reserved non-IRAP VCL NAL unit types"},
     {26, NalType::RSV_VCL26, "RSV_VCL26", "Reserved non-IRAP VCL NAL unit types"},
     {27, NalType::RSV_VCL27, "RSV_VCL27", "Reserved non-IRAP VCL NAL unit types"},
     {28, NalType::RSV_VCL28, "RSV_VCL28", "Reserved non-IRAP VCL NAL unit types"},
     {29, NalType::RSV_VCL29, "RSV_VCL29", "Reserved non-IRAP VCL NAL unit types"},
     {30, NalType::RSV_VCL30, "RSV_VCL30", "Reserved non-IRAP VCL NAL unit types"},
     {31, NalType::RSV_VCL31, "RSV_VCL31", "Reserved non-IRAP VCL NAL unit types"},
     {32, NalType::VPS_NUT, "VPS_NUT", "Video parameter set"},
     {33, NalType::SPS_NUT, "SPS_NUT", "Sequence parameter set"},
     {34, NalType::PPS_NUT, "PPS_NUT", "Picture parameter set"},
     {35, NalType::AUD_NUT, "AUD_NUT", "Access unit delimiter"},
     {36, NalType::EOS_NUT, "EOS_NUT", "End of sequence"},
     {37, NalType::EOB_NUT, "EOB_NUT", "End of bitstream"},
     {38, NalType::FD_NUT, "FD_NUT", "Filler data"},
     {39, NalType::PREFIX_SEI_NUT, "PREFIX_SEI_NUT", "Supplemental enhancement information"},
     {40, NalType::SUFFIX_SEI_NUT, "SUFFIX_SEI_NUT", "Supplemental enhancement information"},
     {41, NalType::RSV_NVCL41, "RSV_NVCL41", "Reserved"},
     {42, NalType::RSV_NVCL42, "RSV_NVCL42", "Reserved"},
     {43, NalType::RSV_NVCL43, "RSV_NVCL43", "Reserved"},
     {44, NalType::RSV_NVCL44, "RSV_NVCL44", "Reserved"},
     {45, NalType::RSV_NVCL45, "RSV_NVCL45", "Reserved"},
     {46, NalType::RSV_NVCL46, "RSV_NVCL46", "Reserved"},
     {47, NalType::RSV_NVCL47, "RSV_NVCL47", "Reserved"},
     {48, NalType::UNSPEC48, "UNSPEC48", "Unspecified"},
     {49, NalType::UNSPEC49, "UNSPEC49", "Unspecified"},
     {50, NalType::UNSPEC50, "UNSPEC50", "Unspecified"},
     {51, NalType::UNSPEC51, "UNSPEC51", "Unspecified"},
     {52, NalType::UNSPEC52, "UNSPEC52", "Unspecified"},
     {53, NalType::UNSPEC53, "UNSPEC53", "Unspecified"},
     {54, NalType::UNSPEC54, "UNSPEC54", "Unspecified"},
     {55, NalType::UNSPEC55, "UNSPEC55", "Unspecified"},
     {56, NalType::UNSPEC56, "UNSPEC56", "Unspecified"},
     {57, NalType::UNSPEC57, "UNSPEC57", "Unspecified"},
     {58, NalType::UNSPEC58, "UNSPEC58", "Unspecified"},
     {59, NalType::UNSPEC69, "UNSPEC69", "Unspecified"},
     {60, NalType::UNSPEC60, "UNSPEC60", "Unspecified"},
     {61, NalType::UNSPEC61, "UNSPEC61", "Unspecified"},
     {62, NalType::UNSPEC62, "UNSPEC62", "Unspecified"},
     {63, NalType::UNSPEC63, "UNSPEC63", "Unspecified"},
     {64, NalType::UNSPECIFIED, "UNSPECIFIED", "Unspecified"}},
    NalType::UNSPECIFIED);

}

void nal_unit_header::parse(SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "nal_unit_header");

  reader.readFlag("forbidden_zero_bit", Options().withCheckEqualTo(0));
  this->nalUnitTypeID =
      reader.readBits("nal_unit_type", 6, Options().withMeaningMap(nalTypeCoding.getMeaningMap()));
  this->nuh_layer_id = reader.readBits("nuh_layer_id", 6, Options().withCheckRange({0, 55}));
  this->nuh_temporal_id_plus1 = reader.readBits("nuh_temporal_id_plus1", 3);

  this->nal_unit_type = nalTypeCoding.getValue(this->nalUnitTypeID);
}

bool nal_unit_header::isIRAP() const
{
  return (this->nal_unit_type == hevc::NalType::BLA_W_LP ||
          this->nal_unit_type == hevc::NalType::BLA_W_RADL ||
          this->nal_unit_type == hevc::NalType::BLA_N_LP ||
          this->nal_unit_type == hevc::NalType::IDR_W_RADL ||
          this->nal_unit_type == hevc::NalType::IDR_N_LP ||
          this->nal_unit_type == hevc::NalType::CRA_NUT ||
          this->nal_unit_type == hevc::NalType::RSV_IRAP_VCL22 ||
          this->nal_unit_type == hevc::NalType::RSV_IRAP_VCL23);
}

bool nal_unit_header::isSLNR() const
{
  return (this->nal_unit_type == hevc::NalType::TRAIL_N ||
          this->nal_unit_type == hevc::NalType::TSA_N ||
          this->nal_unit_type == hevc::NalType::STSA_N ||
          this->nal_unit_type == hevc::NalType::RADL_N ||
          this->nal_unit_type == hevc::NalType::RASL_N ||
          this->nal_unit_type == hevc::NalType::RSV_VCL_N10 ||
          this->nal_unit_type == hevc::NalType::RSV_VCL_N12 ||
          this->nal_unit_type == hevc::NalType::RSV_VCL_N14);
}

bool nal_unit_header::isRADL() const
{
  return (this->nal_unit_type == hevc::NalType::RADL_N ||
          this->nal_unit_type == hevc::NalType::RADL_R);
}

bool nal_unit_header::isRASL() const
{
  return (this->nal_unit_type == hevc::NalType::RASL_N ||
          this->nal_unit_type == hevc::NalType::RASL_R);
}

bool nal_unit_header::isSlice() const
{
  return (
      this->nal_unit_type == hevc::NalType::IDR_W_RADL ||
      this->nal_unit_type == hevc::NalType::IDR_N_LP ||
      this->nal_unit_type == hevc::NalType::CRA_NUT ||
      this->nal_unit_type == hevc::NalType::BLA_W_LP ||
      this->nal_unit_type == hevc::NalType::BLA_W_RADL ||
      this->nal_unit_type == hevc::NalType::BLA_N_LP ||
      this->nal_unit_type == hevc::NalType::TRAIL_N ||
      this->nal_unit_type == hevc::NalType::TRAIL_R ||
      this->nal_unit_type == hevc::NalType::TSA_N || this->nal_unit_type == hevc::NalType::TSA_R ||
      this->nal_unit_type == hevc::NalType::STSA_N ||
      this->nal_unit_type == hevc::NalType::STSA_R ||
      this->nal_unit_type == hevc::NalType::RADL_N ||
      this->nal_unit_type == hevc::NalType::RADL_R ||
      this->nal_unit_type == hevc::NalType::RASL_N || this->nal_unit_type == hevc::NalType::RASL_R);
}

} // namespace parser::hevc
