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

#include "OBU.h"

#include "parser/common/ParserMacros.h"
#include "parser/common/ReaderHelper.h"

namespace AV1
{

QString obuTypeToString(OBUType obuType)
{
  switch(obuType)
  {
  case OBUType::UNSPECIFIED:
    return "UNSPECIFIED";
  case OBUType::RESERVED:
    return "RESERVED";
  case OBUType::OBU_SEQUENCE_HEADER:
    return "OBU_SEQUENCE_HEADER";
  case OBUType::OBU_TEMPORAL_DELIMITER:
    return "OBU_TEMPORAL_DELIMITER";
  case OBUType::OBU_FRAME_HEADER:
    return "OBU_FRAME_HEADER";
  case OBUType::OBU_TILE_GROUP:
    return "OBU_TILE_GROUP";
  case OBUType::OBU_METADATA:
    return "OBU_METADATA";
  case OBUType::OBU_FRAME:
    return "OBU_FRAME";
  case OBUType::OBU_REDUNDANT_FRAME_HEADER:
    return "OBU_REDUNDANT_FRAME_HEADER";
  case OBUType::OBU_TILE_LIST:
    return "OBU_TILE_LIST";
  case OBUType::OBU_PADDING:
    return "OBU_PADDING";
  default:
    return {};
  }
}

OBUType indexToObuType(unsigned index)
{
  switch (index)
  {
    case 0:
      return OBUType::RESERVED;
    case 1:
      return OBUType::OBU_SEQUENCE_HEADER;
    case 2:
      return OBUType::OBU_TEMPORAL_DELIMITER;
    case 3:
      return OBUType::OBU_FRAME_HEADER;
    case 4:
      return OBUType::OBU_TILE_GROUP;
    case 5:
      return OBUType::OBU_METADATA;
    case 6:
      return OBUType::OBU_FRAME;
    case 7:
      return OBUType::OBU_REDUNDANT_FRAME_HEADER;
    case 8:
      return OBUType::OBU_TILE_LIST;
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
      return OBUType::RESERVED;
    case 15:
      return OBUType::OBU_PADDING;
    default:
      return OBUType::UNSPECIFIED;
  }
}

OBU::OBU(QSharedPointer<OBU> obu_src)
{
  // Copy all members but the payload. The payload is only saved for specific obus.
  this->filePosStartEnd = obu_src->filePosStartEnd;
  this->obuIdx = obu_src->obuIdx;
  this->obuType = obu_src->obuType;
  this->obu_extension_flag = obu_src->obu_extension_flag;
  this->obu_has_size_field = obu_src->obu_has_size_field;
  this->temporal_id = obu_src->temporal_id;
  this->spatial_id = obu_src->spatial_id;
  this->obu_size = obu_src->obu_size;
}

bool OBU::parseHeader(const QByteArray &header_data, unsigned int &nrBytesHeader, TreeItem *root)
{
  // Create a sub byte parser to access the bits
  ReaderHelper reader(header_data, root, "obu_header()");

  if (header_data.length() == 0)
    return reader.addErrorMessageChildItem("The OBU header must have at least one byte");

  READZEROBITS(1, "obu_forbidden_bit");

  QStringList obu_type_id_meaning = QStringList()
    << "Reserved"
    << "OBU_SEQUENCE_HEADER"
    << "OBU_TEMPORAL_DELIMITER"
    << "OBU_FRAME_HEADER"
    << "OBU_TILE_GROUP"
    << "OBU_METADATA"
    << "OBU_FRAME"
    << "OBU_REDUNDANT_FRAME_HEADER"
    << "OBU_TILE_LIST"
    << "Reserved"
    << "Reserved"
    << "Reserved"
    << "Reserved"
    << "Reserved"
    << "Reserved"
    << "OBU_PADDING";
  unsigned int obu_type_idx;
  READBITS_M(obu_type_idx, 4, obu_type_id_meaning);
  this->obuType = indexToObuType(obu_type_idx);

  READFLAG(obu_extension_flag);
  READFLAG(obu_has_size_field);

  READZEROBITS(1, "obu_reserved_1bit");
  
  if (obu_extension_flag)
  {
    if (header_data.length() == 1)
      return reader.addErrorMessageChildItem("The OBU header has an obu_extension_header and must have at least two byte");
    READBITS(temporal_id, 3);
    READBITS(spatial_id, 2);

    READZEROBITS(3, "extension_header_reserved_3bits");
  }

  if (obu_has_size_field)
  {
    READLEB128(obu_size);
  }

  nrBytesHeader = reader.nrBytesRead();
  return true;
}

} // namespace AV1