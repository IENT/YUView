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

namespace parser::vvc
{

using namespace parser::reader;

namespace
{

parser::CodingEnum<NalType> nalTypeCoding({{0, NalType::TRAIL_NUT, "TRAIL_NUT"},
                                           {1, NalType::STSA_NUT, "STSA_NUT"},
                                           {2, NalType::RADL_NUT, "RADL_NUT"},
                                           {3, NalType::RASL_NUT, "RASL_NUT"},
                                           {4, NalType::RSV_VCL_4, "RSV_VCL_4"},
                                           {5, NalType::RSV_VCL_5, "RSV_VCL_5"},
                                           {6, NalType::RSV_VCL_6, "RSV_VCL_6"},
                                           {7, NalType::IDR_W_RADL, "IDR_W_RADL"},
                                           {8, NalType::IDR_N_LP, "IDR_N_LP"},
                                           {9, NalType::CRA_NUT, "CRA_NUT"},
                                           {10, NalType::GDR_NUT, "GDR_NUT"},
                                           {11, NalType::RSV_IRAP_11, "RSV_IRAP_11"},
                                           {12, NalType::OPI_NUT, "OPI_NUT"},
                                           {13, NalType::DCI_NUT, "DCI_NUT"},
                                           {14, NalType::VPS_NUT, "VPS_NUT"},
                                           {15, NalType::SPS_NUT, "SPS_NUT"},
                                           {16, NalType::PPS_NUT, "PPS_NUT"},
                                           {17, NalType::PREFIX_APS_NUT, "PREFIX_APS_NUT"},
                                           {18, NalType::SUFFIX_APS_NUT, "SUFFIX_APS_NUT"},
                                           {19, NalType::PH_NUT, "PH_NUT"},
                                           {20, NalType::AUD_NUT, "AUD_NUT"},
                                           {21, NalType::EOS_NUT, "EOS_NUT"},
                                           {22, NalType::EOB_NUT, "EOB_NUT"},
                                           {23, NalType::PREFIX_SEI_NUT, "PREFIX_SEI_NUT"},
                                           {24, NalType::SUFFIX_SEI_NUT, "SUFFIX_SEI_NUT"},
                                           {25, NalType::FD_NUT, "FD_NUT"},
                                           {26, NalType::RSV_NVCL_26, "RSV_NVCL_26"},
                                           {27, NalType::RSV_NVCL_27, "RSV_NVCL_27"},
                                           {28, NalType::UNSPEC_28, "UNSPEC_28"},
                                           {29, NalType::UNSPEC_29, "UNSPEC_29"},
                                           {30, NalType::UNSPEC_30, "UNSPEC_30"},
                                           {31, NalType::UNSPEC_31, "UNSPEC_31"},
                                           {32, NalType::UNSPECIFIED, "UNSPECIFIED"}},
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

} // namespace parser::vvc
