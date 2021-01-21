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

parser::CodingEnum<NalType> nalTypeCoding({{0, NalType::TRAIL_N, "TRAIL_N"},
                                           {1, NalType::TRAIL_R, "TRAIL_R"},
                                           {2, NalType::TSA_N, "TSA_N"},
                                           {3, NalType::TSA_R, "TSA_R"},
                                           {4, NalType::STSA_N, "STSA_N"},
                                           {5, NalType::STSA_R, "STSA_R"},
                                           {6, NalType::RADL_N, "RADL_N"},
                                           {7, NalType::RADL_R, "RADL_R"},
                                           {8, NalType::RASL_N, "RASL_N"},
                                           {9, NalType::RASL_R, "RASL_R"},
                                           {10, NalType::RSV_VCL_N10, "RSV_VCL_N10"},
                                           {12, NalType::RSV_VCL_N12, "RSV_VCL_N12"},
                                           {14, NalType::RSV_VCL_N14, "RSV_VCL_N14"},
                                           {11, NalType::RSV_VCL_R11, "RSV_VCL_R11"},
                                           {13, NalType::RSV_VCL_R13, "RSV_VCL_R13"},
                                           {15, NalType::RSV_VCL_R15, "RSV_VCL_R15"},
                                           {16, NalType::BLA_W_LP, "BLA_W_LP"},
                                           {17, NalType::BLA_W_RADL, "BLA_W_RADL"},
                                           {18, NalType::BLA_N_LP, "BLA_N_LP"},
                                           {19, NalType::IDR_W_RADL, "IDR_W_RADL"},
                                           {20, NalType::IDR_N_LP, "IDR_N_LP"},
                                           {21, NalType::CRA_NUT, "CRA_NUT"},
                                           {22, NalType::RSV_IRAP_VCL22, "RSV_IRAP_VCL22"},
                                           {23, NalType::RSV_IRAP_VCL23, "RSV_IRAP_VCL23"},
                                           {24, NalType::RSV_VCL24, "RSV_VCL24"},
                                           {25, NalType::RSV_VCL25, "RSV_VCL25"},
                                           {26, NalType::RSV_VCL26, "RSV_VCL26"},
                                           {27, NalType::RSV_VCL27, "RSV_VCL27"},
                                           {28, NalType::RSV_VCL28, "RSV_VCL28"},
                                           {29, NalType::RSV_VCL29, "RSV_VCL29"},
                                           {30, NalType::RSV_VCL30, "RSV_VCL30"},
                                           {31, NalType::RSV_VCL31, "RSV_VCL31"},
                                           {32, NalType::VPS_NUT, "VPS_NUT"},
                                           {33, NalType::SPS_NUT, "SPS_NUT"},
                                           {34, NalType::PPS_NUT, "PPS_NUT"},
                                           {35, NalType::AUD_NUT, "AUD_NUT"},
                                           {36, NalType::EOS_NUT, "EOS_NUT"},
                                           {37, NalType::EOB_NUT, "EOB_NUT"},
                                           {38, NalType::FD_NUT, "FD_NUT"},
                                           {39, NalType::PREFIX_SEI_NUT, "PREFIX_SEI_NUT"},
                                           {40, NalType::SUFFIX_SEI_NUT, "SUFFIX_SEI_NUT"},
                                           {41, NalType::RSV_NVCL41, "RSV_NVCL41"},
                                           {42, NalType::RSV_NVCL42, "RSV_NVCL42"},
                                           {43, NalType::RSV_NVCL43, "RSV_NVCL43"},
                                           {44, NalType::RSV_NVCL44, "RSV_NVCL44"},
                                           {45, NalType::RSV_NVCL45, "RSV_NVCL45"},
                                           {46, NalType::RSV_NVCL46, "RSV_NVCL46"},
                                           {47, NalType::RSV_NVCL47, "RSV_NVCL47"},
                                           {48, NalType::UNSPEC48, "UNSPEC48"},
                                           {49, NalType::UNSPEC49, "UNSPEC49"},
                                           {50, NalType::UNSPEC50, "UNSPEC50"},
                                           {51, NalType::UNSPEC51, "UNSPEC51"},
                                           {52, NalType::UNSPEC52, "UNSPEC52"},
                                           {53, NalType::UNSPEC53, "UNSPEC53"},
                                           {54, NalType::UNSPEC54, "UNSPEC54"},
                                           {55, NalType::UNSPEC55, "UNSPEC55"},
                                           {56, NalType::UNSPEC56, "UNSPEC56"},
                                           {57, NalType::UNSPEC57, "UNSPEC57"},
                                           {58, NalType::UNSPEC58, "UNSPEC58"},
                                           {59, NalType::UNSPEC69, "UNSPEC69"},
                                           {60, NalType::UNSPEC60, "UNSPEC60"},
                                           {61, NalType::UNSPEC61, "UNSPEC61"},
                                           {62, NalType::UNSPEC62, "UNSPEC62"},
                                           {63, NalType::UNSPEC63, "UNSPEC63"},
                                           {64, NalType::UNSPECIFIED, "UNSPECIFIED"}},
                                          NalType::UNSPECIFIED);

}

void nal_unit_header::parse(SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "nal_unit_header");

  reader.readFlag("forbidden_zero_bit", Options().withCheckEqualTo(0));
  reader.readFlag("nuh_reserved_zero_bit", Options().withCheckEqualTo(0));
  this->nuh_layer_id = reader.readBits("nuh_layer_id", 6, Options().withCheckRange({0, 55}));

  this->nalUnitTypeID = reader.readBits(
      "nal_unit_type",
      5,
      Options().withMeaningVector({"Coded slice of a trailing picture or subpicture",
                                   "Coded slice of an STSA picture or subpicture",
                                   "Coded slice of a RADL picture or subpicture",
                                   "Coded slice of a RASL picture or subpicture",
                                   "Reserved non-IRAP VCL NAL unit types",
                                   "Reserved non-IRAP VCL NAL unit types",
                                   "Reserved non-IRAP VCL NAL unit types",
                                   "Coded slice of an IDR picture or subpicture",
                                   "Coded slice of an IDR picture or subpicture",
                                   "Coded slice of a CRA picture or subpicture",
                                   "Coded slice of a GDR picture or subpicture",
                                   "Reserved IRAP VCL NAL unit type",
                                   "Operating point information",
                                   "Decoding capability information",
                                   "Video parameter set",
                                   "Sequence parameter set",
                                   "Picture parameter set",
                                   "Adaptation parameter set",
                                   "Adaptation parameter set",
                                   "Picture header",
                                   "AU delimiter",
                                   "End of sequence",
                                   "End of bitstream",
                                   "Supplemental enhancement information",
                                   "Supplemental enhancement information",
                                   "Filler data",
                                   "Reserved non-VCL NAL unit types",
                                   "Reserved non-VCL NAL unit types",
                                   "Unspecified non-VCL NAL unit types",
                                   "Unspecified non-VCL NAL unit types",
                                   "Unspecified non-VCL NAL unit types",
                                   "Unspecified non-VCL NAL unit types"}));
  this->nal_unit_type = nalTypeCoding.getValue(this->nalUnitTypeID);

  this->nuh_temporal_id_plus1 = reader.readBits("nuh_temporal_id_plus1", 3);
}

QByteArray nal_unit_header::getNALHeader() const
{
  QByteArray ret;
  ret.append(char(this->nuh_layer_id));
  ret.append(char((this->nalUnitTypeID << 3) + this->nuh_temporal_id_plus1));
  return ret;
}

} // namespace parser::hevc
