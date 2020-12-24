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

namespace parser::vvc
{

namespace
{

const std::map<unsigned, NalType> idToNalTypeMap = {
    {0, NalType::TRAIL_NUT},       {1, NalType::STSA_NUT},   {2, NalType::RADL_NUT},
    {3, NalType::RASL_NUT},        {4, NalType::RSV_VCL_4},  {5, NalType::RSV_VCL_5},
    {6, NalType::RSV_VCL_6},       {7, NalType::IDR_W_RADL}, {8, NalType::IDR_N_LP},
    {9, NalType::CRA_NUT},         {10, NalType::GDR_NUT},   {11, NalType::RSV_IRAP_11},
    {12, NalType::OPI_NUT},        {13, NalType::DCI_NUT},   {14, NalType::VPS_NUT},
    {15, NalType::SPS_NUT},        {16, NalType::PPS_NUT},   {17, NalType::PREFIX_APS_NUT},
    {18, NalType::SUFFIX_APS_NUT}, {19, NalType::PH_NUT},    {20, NalType::AUD_NUT},
    {21, NalType::EOS_NUT},        {22, NalType::EOB_NUT},   {23, NalType::PREFIX_SEI_NUT},
    {24, NalType::SUFFIX_SEI_NUT}, {25, NalType::FD_NUT},    {26, NalType::RSV_NVCL_26},
    {27, NalType::RSV_NVCL_27},    {28, NalType::UNSPEC_28}, {29, NalType::UNSPEC_29},
    {30, NalType::UNSPEC_30},      {31, NalType::UNSPEC_31}};

}

void nal_unit_header::parse(ReaderHelperNew &reader)
{
  this->forbidden_zero_bit    = reader.readFlag("forbidden_zero_bit");
  this->nuh_reserved_zero_bit = reader.readFlag("nuh_reserved_zero_bit");
  this->nuh_layer_id          = reader.readBits("nuh_layer_id", 6);

  std::map<int, std::string> nal_unit_type_meaning;
  nal_unit_type_meaning[0]  = "Coded slice of a trailing picture or subpicture";
  nal_unit_type_meaning[1]  = "Coded slice of an STSA picture or subpicture";
  nal_unit_type_meaning[2]  = "Coded slice of a RADL picture or subpicture";
  nal_unit_type_meaning[3]  = "Coded slice of a RASL picture or subpicture";
  nal_unit_type_meaning[4]  = "Reserved non-IRAP VCL NAL unit types";
  nal_unit_type_meaning[5]  = "Reserved non-IRAP VCL NAL unit types";
  nal_unit_type_meaning[6]  = "Reserved non-IRAP VCL NAL unit types";
  nal_unit_type_meaning[7]  = "Coded slice of an IDR picture or subpicture";
  nal_unit_type_meaning[8]  = "Coded slice of an IDR picture or subpicture";
  nal_unit_type_meaning[9]  = "Coded slice of a CRA picture or subpicture";
  nal_unit_type_meaning[10] = "Coded slice of a GDR picture or subpicture";
  nal_unit_type_meaning[11] = "Reserved IRAP VCL NAL unit type";
  nal_unit_type_meaning[12] = "Operating point information";
  nal_unit_type_meaning[13] = "Decoding capability information";
  nal_unit_type_meaning[14] = "Video parameter set";
  nal_unit_type_meaning[15] = "Sequence parameter set";
  nal_unit_type_meaning[16] = "Picture parameter set";
  nal_unit_type_meaning[17] = "Adaptation parameter set";
  nal_unit_type_meaning[18] = "Adaptation parameter set";
  nal_unit_type_meaning[19] = "Picture header";
  nal_unit_type_meaning[20] = "AU delimiter";
  nal_unit_type_meaning[21] = "End of sequence";
  nal_unit_type_meaning[22] = "End of bitstream";
  nal_unit_type_meaning[23] = "Supplemental enhancement information";
  nal_unit_type_meaning[24] = "Supplemental enhancement information";
  nal_unit_type_meaning[25] = "Filler data";
  nal_unit_type_meaning[26] = "Reserved non-VCL NAL unit types";
  nal_unit_type_meaning[27] = "Reserved non-VCL NAL unit types";
  nal_unit_type_meaning[28] = "Unspecified non-VCL NAL unit types";
  nal_unit_type_meaning[29] = "Unspecified non-VCL NAL unit types";
  nal_unit_type_meaning[30] = "Unspecified non-VCL NAL unit types";
  nal_unit_type_meaning[31] = "Unspecified non-VCL NAL unit types";
  this->nalUnitTypeID       = reader.readBits(
      "nal_unit_type", 5, ReaderHelperNew::Options{.meaningMap = nal_unit_type_meaning});
  this->nal_unit_type = idToNalTypeMap.at(this->nalUnitTypeID);

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
