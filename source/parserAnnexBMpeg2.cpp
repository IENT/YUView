/*  This file is part of YUView - The YUV player with advanced analytics toolset
*   <https://github.com/IENT/YUView>
*   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
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

#include "parserAnnexBMpeg2.h"

// Read "numBits" bits into the variable "into". 
#define READBITS(into,numBits) {QString code; into=reader.readBits(numBits, &code); if (itemTree) new TreeItem(#into,into,QString("u(v) -> u(%1)").arg(numBits),code, itemTree);}
#define READBITS_M(into,numBits,meanings) {QString code; into=reader.readBits(numBits, &code); if (itemTree) new TreeItem(#into,into,QString("u(v) -> u(%1)").arg(numBits),code, meanings,itemTree);}

parserAnnexBMpeg2::parserAnnexBMpeg2()
{
}

void parserAnnexBMpeg2::nal_unit_mpeg2::parse_nal_unit_header(const QByteArray &header_byte, TreeItem *root)
{
  if (header_byte.length() != 1)
    throw std::logic_error("The AVC header must have only one byte.");

  // Create a sub byte parser to access the bits
  sub_byte_reader reader(header_byte);

  // Create a new TreeItem root for the item
  // The macros will use this variable to add all the parsed variables
  TreeItem *const itemTree = root ? new TreeItem("nal_unit_header()", root) : nullptr;

  // Parse the start code
  QString code;
  start_code_value = reader.readBits(8, &code);
  QString start_code_value_meaning = interprete_start_code(start_code_value);
  
  if (itemTree) 
    new TreeItem("start_code_value", start_code_value, QString("u(v) -> u(8)"), code, start_code_value_meaning, itemTree);
}

QByteArray parserAnnexBMpeg2::nal_unit_mpeg2::getNALHeader() const
{
  QByteArray ret;
  ret.append(start_code_value);
  return ret;
}

QString parserAnnexBMpeg2::nal_unit_mpeg2::interprete_start_code(int start_code)
{
  if (start_code == 0)
  {
    nal_unit_type = PICTURE;
    return "picture_start_code";
  }
  else if (start_code >= 0x01 && start_code <= 0xaf)
  {
    nal_unit_type = SLICE;
    slice_id = start_code - 1;
    return "slice_start_code";
  }
  else if (start_code == 0xb0 || start_code == 0xb1 || start_code == 0xb6)
  {
    nal_unit_type = RESERVED;
    return "reserved";
  }
  else if (start_code == 0xb2)
  {
    nal_unit_type = USER_DATA;
    return "user_data_start_code";
  }
  else if (start_code == 0xb3)
  {
    nal_unit_type = SEQUENCE_HEADER;
    return "sequence_header_code";
  }
  else if (start_code == 0xb4)
  {
    nal_unit_type = SEQUENCE_ERROR;
    return "sequence_error_code";
  }
  else if (start_code == 0xb5)
  {
    nal_unit_type = EXTENSION_START;
    return "extension_start_code";
  }
  else if (start_code == 0xb7)
  {
    nal_unit_type = SEQUENCE_END;
    return "sequence_end_code";
  }
  else if (start_code == 0xb8)
  {
    nal_unit_type = GROUP_START;
    return "group_start_code";
  }
  else if (start_code >= 0xb9)
  {
    nal_unit_type = SYSTEM_START_CODE;
    system_start_codes = start_code - 0xb9;
    return "system start codes";
  }
  
  return "";
}

void parserAnnexBMpeg2::parseAndAddNALUnit(int nalID, QByteArray data, TreeItem *parent, QUint64Pair nalStartEndPosFile, QString *nalTypeName)
{
  // Skip the NAL unit header
  int skip = 0;
  if (data.at(0) == (char)0 && data.at(1) == (char)0 && data.at(2) == (char)1)
    skip = 3;
  else if (data.at(0) == (char)0 && data.at(1) == (char)0 && data.at(2) == (char)0 && data.at(3) == (char)1)
    skip = 4;
  else
    // No NAL header found
    skip = 0;

  // Read ony byte (the NAL header) (technically there is no NAL in mpeg2 but it works pretty similarly)
  QByteArray nalHeaderBytes = data.mid(skip, 1);
  QByteArray payload = data.mid(skip + 1);

  // Use the given tree item. If it is not set, use the nalUnitMode (if active). 
  // We don't set data (a name) for this item yet. 
  // We want to parse the item and then set a good description.
  QString specificDescription;
  TreeItem *nalRoot = nullptr;
  if (parent)
    nalRoot = new TreeItem(parent);
  else if (!nalUnitModel.rootItem.isNull())
    nalRoot = new TreeItem(nalUnitModel.rootItem.data());

  // Create a nal_unit and read the header
  nal_unit_mpeg2 nal_mpeg2(nalStartEndPosFile, nalID);
  nal_mpeg2.parse_nal_unit_header(nalHeaderBytes, nalRoot);

  if (nal_mpeg2.nal_unit_type == SEQUENCE_HEADER)
  {
    // A sequence header
    auto new_sequence_header = QSharedPointer<sequence_header>(new sequence_header(nal_mpeg2));
    new_sequence_header->parse_sequence_header(payload, nalRoot);
  }
}

parserAnnexBMpeg2::sequence_header::sequence_header(const nal_unit_mpeg2 & nal) : nal_unit_mpeg2(nal)
{
  // TODO...
}

void parserAnnexBMpeg2::sequence_header::parse_sequence_header(const QByteArray & parameterSetData, TreeItem * root)
{
}
