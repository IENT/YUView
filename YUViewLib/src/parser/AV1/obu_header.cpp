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

#include "obu_header.h"

namespace parser::av1
{

using namespace reader;

namespace
{

auto ObuTypeMap = std::map<unsigned, ObuType>({{0, ObuType::RESERVED},
                                               {1, ObuType::OBU_SEQUENCE_HEADER},
                                               {2, ObuType::OBU_TEMPORAL_DELIMITER},
                                               {3, ObuType::OBU_FRAME_HEADER},
                                               {4, ObuType::OBU_TILE_GROUP},
                                               {5, ObuType::OBU_METADATA},
                                               {6, ObuType::OBU_FRAME},
                                               {7, ObuType::OBU_REDUNDANT_FRAME_HEADER},
                                               {8, ObuType::OBU_TILE_LIST},
                                               {9, ObuType::RESERVED},
                                               {10, ObuType::RESERVED},
                                               {11, ObuType::RESERVED},
                                               {12, ObuType::RESERVED},
                                               {13, ObuType::RESERVED},
                                               {14, ObuType::RESERVED},
                                               {15, ObuType::OBU_PADDING}});

auto ObuNameMap = std::map<ObuType, std::string>(
    {{ObuType::RESERVED, "RESERVED"},
     {ObuType::OBU_SEQUENCE_HEADER, "OBU_SEQUENCE_HEADER"},
     {ObuType::OBU_TEMPORAL_DELIMITER, "OBU_TEMPORAL_DELIMITER"},
     {ObuType::OBU_FRAME_HEADER, "OBU_FRAME_HEADER"},
     {ObuType::OBU_TILE_GROUP, "OBU_TILE_GROUP"},
     {ObuType::OBU_METADATA, "OBU_METADATA"},
     {ObuType::OBU_FRAME, "OBU_FRAME"},
     {ObuType::OBU_REDUNDANT_FRAME_HEADER, "OBU_REDUNDANT_FRAME_HEADER"},
     {ObuType::OBU_TILE_LIST, "OBU_TILE_LIST"},
     {ObuType::OBU_PADDING, "OBU_PADDING"},
     {ObuType::UNSPECIFIED, "UNSPECIFIED"}});

auto getObuMeaningVector()
{
  MeaningMap vec;
  for (const auto &type : ObuTypeMap)
    vec[type.first] = ObuNameMap[type.second];
  return vec;
}

} // namespace

std::string to_string(ObuType obuType) { return ObuNameMap[obuType]; }

void obu_header::parse(SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "obu_header");

  reader.readFlag("obu_forbidden_bit", Options().withCheckEqualTo(0));

  this->obu_type_idx =
      reader.readBits("obu_type_idx", 4, Options().withMeaningMap(getObuMeaningVector()));

  this->obu_type = ObuTypeMap[this->obu_type_idx];

  this->obu_extension_flag = reader.readFlag("obu_extension_flag");
  this->obu_has_size_field = reader.readFlag("obu_has_size_field");

  reader.readFlag("obu_reserved_1bit", Options().withCheckEqualTo(0));

  if (this->obu_extension_flag)
  {
    this->temporal_id = reader.readBits("temporal_id", 3);
    this->spatial_id = reader.readBits("spatial_id", 2);

    reader.readBits("extension_header_reserved_3bits", 3);
  }

  if (this->obu_has_size_field)
  {
    this->obu_size = reader.readLEB128("obu_size");
  }
}

} // namespace parser::av1