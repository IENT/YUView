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

#include <common/Typedef.h>
#include <parser/common/CodingEnum.h>
#include <parser/common/SubByteReaderLogging.h>

namespace parser::av1
{

enum class ObuType
{
  RESERVED,
  OBU_SEQUENCE_HEADER,
  OBU_TEMPORAL_DELIMITER,
  OBU_FRAME_HEADER,
  OBU_TILE_GROUP,
  OBU_METADATA,
  OBU_FRAME,
  OBU_REDUNDANT_FRAME_HEADER,
  OBU_TILE_LIST,
  OBU_PADDING,
  UNSPECIFIED
};

static CodingEnum<ObuType>
    obuTypeCoding({{0, ObuType::RESERVED, "RESERVED"},
                   {1, ObuType::OBU_SEQUENCE_HEADER, "OBU_SEQUENCE_HEADER"},
                   {2, ObuType::OBU_TEMPORAL_DELIMITER, "OBU_TEMPORAL_DELIMITER"},
                   {3, ObuType::OBU_FRAME_HEADER, "OBU_FRAME_HEADER"},
                   {4, ObuType::OBU_TILE_GROUP, "OBU_TILE_GROUP"},
                   {5, ObuType::OBU_METADATA, "OBU_METADATA"},
                   {6, ObuType::OBU_FRAME, "OBU_FRAME"},
                   {7, ObuType::OBU_REDUNDANT_FRAME_HEADER, "OBU_REDUNDANT_FRAME_HEADER"},
                   {8, ObuType::OBU_TILE_LIST, "OBU_TILE_LIST"},
                   {9, ObuType::RESERVED, "RESERVED"},
                   {10, ObuType::RESERVED, "RESERVED"},
                   {11, ObuType::RESERVED, "RESERVED"},
                   {12, ObuType::RESERVED, "RESERVED"},
                   {13, ObuType::RESERVED, "RESERVED"},
                   {14, ObuType::RESERVED, "RESERVED"},
                   {15, ObuType::OBU_PADDING, "OBU_PADDING"}},
                  ObuType::RESERVED);

class obu_header
{
public:
  obu_header()          = default;
  virtual ~obu_header() = default;

  void parse(reader::SubByteReaderLogging &reader);

  unsigned obu_type_idx;
  ObuType  obu_type{ObuType::UNSPECIFIED};
  bool     obu_extension_flag{false};
  bool     obu_has_size_field{false};

  // OBU extension header
  unsigned int temporal_id{0};
  unsigned int spatial_id{0};

  uint64_t obu_size{0};
};

} // namespace parser::av1