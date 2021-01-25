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

parser::CodingEnum<NalType> nalTypeCoding(
    {{0, NalType::TRAIL_NUT, "TRAIL_NUT", "Coded slice of a trailing picture or subpicture"},
     {1, NalType::STSA_NUT, "STSA_NUT", "Coded slice of an STSA picture or subpicture"},
     {2, NalType::RADL_NUT, "RADL_NUT", "Coded slice of a RADL picture or subpicture"},
     {3, NalType::RASL_NUT, "RASL_NUT", "Coded slice of a RASL picture or subpicture"},
     {4, NalType::RSV_VCL_4, "RSV_VCL_4", "Reserved non-IRAP VCL NAL unit types"},
     {5, NalType::RSV_VCL_5, "RSV_VCL_5", "Reserved non-IRAP VCL NAL unit types"},
     {6, NalType::RSV_VCL_6, "RSV_VCL_6", "Reserved non-IRAP VCL NAL unit types"},
     {7, NalType::IDR_W_RADL, "IDR_W_RADL", "Coded slice of an IDR picture or subpicture"},
     {8, NalType::IDR_N_LP, "IDR_N_LP", "Coded slice of an IDR picture or subpicture"},
     {9, NalType::CRA_NUT, "CRA_NUT", "Coded slice of a CRA picture or subpicture"},
     {10, NalType::GDR_NUT, "GDR_NUT", "Coded slice of a GDR picture or subpicture"},
     {11, NalType::RSV_IRAP_11, "RSV_IRAP_11", "Reserved IRAP VCL NAL unit type"},
     {12, NalType::OPI_NUT, "OPI_NUT", "Operating point information"},
     {13, NalType::DCI_NUT, "DCI_NUT", "Decoding capability information"},
     {14, NalType::VPS_NUT, "VPS_NUT", "Video parameter set"},
     {15, NalType::SPS_NUT, "SPS_NUT", "Sequence parameter set"},
     {16, NalType::PPS_NUT, "PPS_NUT", "Picture parameter set"},
     {17, NalType::PREFIX_APS_NUT, "PREFIX_APS_NUT", "Adaptation parameter set"},
     {18, NalType::SUFFIX_APS_NUT, "SUFFIX_APS_NUT", "Adaptation parameter set"},
     {19, NalType::PH_NUT, "PH_NUT", "Picture header"},
     {20, NalType::AUD_NUT, "AUD_NUT", "AU delimiter"},
     {21, NalType::EOS_NUT, "EOS_NUT", "End of sequence"},
     {22, NalType::EOB_NUT, "EOB_NUT", "End of bitstream"},
     {23, NalType::PREFIX_SEI_NUT, "PREFIX_SEI_NUT", "Supplemental enhancement information"},
     {24, NalType::SUFFIX_SEI_NUT, "SUFFIX_SEI_NUT", "Supplemental enhancement information"},
     {25, NalType::FD_NUT, "FD_NUT", "Filler data"},
     {26, NalType::RSV_NVCL_26, "RSV_NVCL_26", "Reserved non-VCL NAL unit types"},
     {27, NalType::RSV_NVCL_27, "RSV_NVCL_27", "Reserved non-VCL NAL unit types"},
     {28, NalType::UNSPEC_28, "UNSPEC_28", "Unspecified non-VCL NAL unit types"},
     {29, NalType::UNSPEC_29, "UNSPEC_29", "Unspecified non-VCL NAL unit types"},
     {30, NalType::UNSPEC_30, "UNSPEC_30", "Unspecified non-VCL NAL unit types"},
     {31, NalType::UNSPEC_31, "UNSPEC_31", "Unspecified non-VCL NAL unit types"},
     {32, NalType::UNSPECIFIED, "UNSPECIFIED"}},
    NalType::UNSPECIFIED);

}

void nal_unit_header::parse(SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "nal_unit_header");

  reader.readFlag("forbidden_zero_bit", Options().withCheckEqualTo(0));
  reader.readFlag("nuh_reserved_zero_bit", Options().withCheckEqualTo(0));
  this->nuh_layer_id = reader.readBits("nuh_layer_id", 6, Options().withCheckRange({0, 55}));

  this->nalUnitTypeID =
      reader.readBits("nal_unit_type", 5, Options().withMeaningMap(nalTypeCoding.getMeaningMap()));
  this->nal_unit_type = nalTypeCoding.getValue(this->nalUnitTypeID);

  this->nuh_temporal_id_plus1 = reader.readBits("nuh_temporal_id_plus1", 3);
}

} // namespace parser::vvc
