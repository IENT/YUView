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

namespace parser::avc
{

// All the different NAL unit types (T-REC-H.265-201504 Page 85)
enum class NalType
{
  UNSPECIFIED,
  CODED_SLICE_NON_IDR,
  CODED_SLICE_DATA_PARTITION_A,
  CODED_SLICE_DATA_PARTITION_B,
  CODED_SLICE_DATA_PARTITION_C,
  CODED_SLICE_IDR,
  SEI,
  SPS,
  PPS,
  AUD,
  END_OF_SEQUENCE,
  END_OF_STREAM,
  FILLER,
  SPS_EXT,
  PREFIX_NAL,
  SUBSET_SPS,
  DEPTH_PARAMETER_SET,
  RESERVED_17,
  RESERVED_18,
  CODED_SLICE_AUX,
  CODED_SLICE_EXTENSION,
  CODED_SLICE_EXTENSION_DEPTH_MAP,
  RESERVED_22,
  RESERVED_23
};

const EnumMapper<NalType>
    NalTypeMapper({{NalType::UNSPECIFIED, "UNSPECIFIED"},
                   {NalType::CODED_SLICE_NON_IDR, "CODED_SLICE_NON_IDR"},
                   {NalType::CODED_SLICE_DATA_PARTITION_A, "CODED_SLICE_DATA_PARTITION_A"},
                   {NalType::CODED_SLICE_DATA_PARTITION_B, "CODED_SLICE_DATA_PARTITION_B"},
                   {NalType::CODED_SLICE_DATA_PARTITION_C, "CODED_SLICE_DATA_PARTITION_C"},
                   {NalType::CODED_SLICE_IDR, "CODED_SLICE_IDR"},
                   {NalType::SEI, "SEI"},
                   {NalType::SPS, "SPS"},
                   {NalType::PPS, "PPS"},
                   {NalType::AUD, "AUD"},
                   {NalType::END_OF_SEQUENCE, "END_OF_SEQUENCE"},
                   {NalType::END_OF_STREAM, "END_OF_STREAM"},
                   {NalType::FILLER, "FILLER"},
                   {NalType::SPS_EXT, "SPS_EXT"},
                   {NalType::PREFIX_NAL, "PREFIX_NAL"},
                   {NalType::SUBSET_SPS, "SUBSET_SPS"},
                   {NalType::DEPTH_PARAMETER_SET, "DEPTH_PARAMETER_SET"},
                   {NalType::RESERVED_17, "RESERVED_17"},
                   {NalType::RESERVED_18, "RESERVED_18"},
                   {NalType::CODED_SLICE_AUX, "CODED_SLICE_AUX"},
                   {NalType::CODED_SLICE_EXTENSION, "CODED_SLICE_EXTENSION"},
                   {NalType::CODED_SLICE_EXTENSION_DEPTH_MAP, "CODED_SLICE_EXTENSION_DEPTH_MAP"},
                   {NalType::RESERVED_22, "RESERVED_22"},
                   {NalType::RESERVED_23, "RESERVED_23"}});

class nal_unit_header
{
public:
  nal_unit_header()  = default;
  ~nal_unit_header() = default;
  void parse(reader::SubByteReaderLogging &reader);

  unsigned nal_ref_idc{};

  NalType  nal_unit_type;
  unsigned nalUnitTypeID;
};

} // namespace parser::avc
