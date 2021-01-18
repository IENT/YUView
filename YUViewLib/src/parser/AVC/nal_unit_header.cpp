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

namespace parser::avc
{

using namespace parser::reader;

void nal_unit_header::parse(SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "nal_unit_header");

  auto forbidden_zero_bit = reader.readFlag("forbidden_zero_bit", Options().withCheckEqualTo(0));
  this->nal_ref_idc       = reader.readBits("nal_ref_idc", 2);

  this->nalUnitTypeID = reader.readBits(
      "nal_unit_type",
      5,
      Options().withMeaningVector(
          {"Unspecified",
           "Coded slice of a non-IDR picture slice_layer_without_partitioning_rbsp()",
           "Coded slice data partition A slice_data_partition_a_layer_rbsp( )",
           "Coded slice data partition B slice_data_partition_b_layer_rbsp( )",
           "Coded slice data partition C slice_data_partition_c_layer_rbsp( )",
           "Coded slice of an IDR picture slice_layer_without_partitioning_rbsp( )",
           "Supplemental enhancement information (SEI) sei_rbsp( )",
           "Sequence parameter set seq_parameter_set_rbsp( )",
           "Picture parameter set pic_parameter_set_rbsp( )",
           "Access unit delimiter access_unit_delimiter_rbsp( )",
           "End of sequence end_of_seq_rbsp( )",
           "End of stream end_of_stream_rbsp( )",
           "Filler data filler_data_rbsp( )",
           "Sequence parameter set extension seq_parameter_set_extension_rbsp( )",
           "Prefix NAL unit prefix_nal_unit_rbsp( )",
           "Subset sequence parameter set subset_seq_parameter_set_rbsp( )",
           "Depth parameter set depth_parameter_set_rbsp( )",
           "Reserved",
           "Reserved",
           "Coded slice of an auxiliary coded picture without partitioning "
           "slice_layer_without_partitioning_rbsp( )",
           "Coded slice extension slice_layer_extension_rbsp( )",
           "Coded slice extension for a depth view component or a 3D-AVC texture view component "
           "slice_layer_extension_rbsp( )",
           "Reserved",
           "Reserved",
           "Unspecified"}));

  nal_unit_type = nalTypeCoding.getValue(this->nalUnitTypeID);
}

bool nal_unit_header::isSlice(NalType nalType) const
{
  return (this->nalType == NalType::CODED_SLICE_NON_IDR ||
          this->nalType == NalType::CODED_SLICE_DATA_PARTITION_A ||
          this->nalType == NalType::CODED_SLICE_DATA_PARTITION_B ||
          this->nalType == NalType::CODED_SLICE_DATA_PARTITION_C ||
          this->nalType == NalType::CODED_SLICE_IDR);
}

} // namespace parser::avc
