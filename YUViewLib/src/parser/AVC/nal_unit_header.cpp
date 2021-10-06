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

namespace
{

parser::CodingEnum<NalType> nalTypeCoding(
    {{0, NalType::UNSPECIFIED, "UNSPECIFIED", "Unspecified"},
     {1,
      NalType::CODED_SLICE_NON_IDR,
      "CODED_SLICE_NON_IDR",
      "Coded slice of a non-IDR picture slice_layer_without_partitioning_rbsp()"},
     {2,
      NalType::CODED_SLICE_DATA_PARTITION_A,
      "CODED_SLICE_DATA_PARTITION_A",
      "Coded slice data partition A slice_data_partition_a_layer_rbsp( )"},
     {3,
      NalType::CODED_SLICE_DATA_PARTITION_B,
      "CODED_SLICE_DATA_PARTITION_B",
      "Coded slice data partition B slice_data_partition_b_layer_rbsp( )"},
     {4,
      NalType::CODED_SLICE_DATA_PARTITION_C,
      "CODED_SLICE_DATA_PARTITION_C",
      "Coded slice data partition C slice_data_partition_c_layer_rbsp( )"},
     {5,
      NalType::CODED_SLICE_IDR,
      "CODED_SLICE_IDR",
      "Coded slice of an IDR picture slice_layer_without_partitioning_rbsp( )"},
     {6, NalType::SEI, "SEI", "Supplemental enhancement information (SEI) sei_rbsp( )"},
     {7, NalType::SPS, "SPS", "Sequence parameter set seq_parameter_set_rbsp( )"},
     {8, NalType::PPS, "PPS", "Picture parameter set pic_parameter_set_rbsp( )"},
     {9, NalType::AUD, "AUD", "Access unit delimiter access_unit_delimiter_rbsp( )"},
     {10, NalType::END_OF_SEQUENCE, "END_OF_SEQUENCE", "End of sequence end_of_seq_rbsp( )"},
     {11, NalType::END_OF_STREAM, "END_OF_STREAM", "End of stream end_of_stream_rbsp( )"},
     {12, NalType::FILLER, "FILLER", "Filler data filler_data_rbsp( )"},
     {13,
      NalType::SPS_EXT,
      "SPS_EXT",
      "Sequence parameter set extension seq_parameter_set_extension_rbsp( )"},
     {14, NalType::PREFIX_NAL, "PREFIX_NAL", "Prefix NAL unit prefix_nal_unit_rbsp( )"},
     {15,
      NalType::SUBSET_SPS,
      "SUBSET_SPS",
      "Subset sequence parameter set subset_seq_parameter_set_rbsp( )"},
     {16,
      NalType::DEPTH_PARAMETER_SET,
      "DEPTH_PARAMETER_SET",
      "Depth parameter set depth_parameter_set_rbsp( )"},
     {17, NalType::RESERVED_17, "RESERVED_17", "Reserved"},
     {18, NalType::RESERVED_18, "RESERVED_18", "Reserved"},
     {19,
      NalType::CODED_SLICE_AUX,
      "CODED_SLICE_AUX",
      "Coded slice of an auxiliary coded picture without partitioning"},
     {20,
      NalType::CODED_SLICE_EXTENSION,
      "CODED_SLICE_EXTENSION",
      "slice_layer_without_partitioning_rbsp( )"},
     {21,
      NalType::CODED_SLICE_EXTENSION_DEPTH_MAP,
      "CODED_SLICE_EXTENSION_DEPTH_MAP",
      "Coded slice extension for a depth view component or a 3D-AVC texture view component"},
     {22, NalType::RESERVED_22, "RESERVED_22", "Reserved"},
     {23, NalType::RESERVED_23, "RESERVED_23", "Reserved"}},
    NalType::UNSPECIFIED);

}

using namespace parser::reader;

void nal_unit_header::parse(SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "nal_unit_header");

  reader.readFlag("forbidden_zero_bit", Options().withCheckEqualTo(0));

  this->nal_ref_idc = reader.readBits("nal_ref_idc", 2);

  this->nalUnitTypeID =
      reader.readBits("nal_unit_type", 5, Options().withMeaningMap(nalTypeCoding.getMeaningMap()));

  nal_unit_type = nalTypeCoding.getValue(this->nalUnitTypeID);
}

} // namespace parser::avc
