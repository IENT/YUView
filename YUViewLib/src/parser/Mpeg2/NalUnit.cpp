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

#include "NalUnit.h"

#include "parser/common/ParserMacros.h"
#include "parser/common/ReaderHelper.h"

namespace MPEG2
{

QString nalUnitTypeToString(NalUnitType nalUnitType)
{
  switch (nalUnitType)
  {
  case NalUnitType::UNSPECIFIED:
    return "UNSPECIFIED";
  case NalUnitType::PICTURE:
    return "PICTURE";
  case NalUnitType::SLICE:
    return "SLICE";
  case NalUnitType::USER_DATA:
    return "USER_DATA";
  case NalUnitType::SEQUENCE_HEADER:
    return "SEQUENCE_HEADER";
  case NalUnitType::SEQUENCE_ERROR:
    return "SEQUENCE_ERROR";
  case NalUnitType::EXTENSION_START:
    return "EXTENSION_START";
  case NalUnitType::SEQUENCE_END:
    return "SEQUENCE_END";
  case NalUnitType::GROUP_START:
    return "GROUP_START";
  case NalUnitType::SYSTEM_START_CODE:
    return "SYSTEM_START_CODE";
  case NalUnitType::RESERVED:
    return "RESERVED";
  default:
    return {};
  }
}

NalUnit::NalUnit(QSharedPointer<NalUnit> nal_src) 
  : NalUnitBase(nal_src->nal_idx, nal_src->filePosStartEnd)
{
  this->nal_unit_type = nal_src->nal_unit_type;
  this->slice_id = nal_src->slice_id;
  this->system_start_codes = nal_src->system_start_codes;
  this->start_code_value = nal_src->start_code_value;  
}

bool NalUnit::parseNalUnitHeader(const QByteArray &header_byte, TreeItem *root)
{
  // Create a sub byte parser to access the bits
  ReaderHelper reader(header_byte, root, "header_code()");

  if (header_byte.length() != 1)
    return reader.addErrorMessageChildItem("The header code must be one byte only.");

  READBITS_M(start_code_value, 8, this->getStartCodeMeanings());
  interpreteStartCodeValue();

  return true;
}

QByteArray NalUnit::getNALHeader() const
{
  QByteArray ret;
  ret.append(start_code_value);
  return ret;
}

QStringList NalUnit::getStartCodeMeanings() const
{
  QStringList meanings = QStringList();
  meanings.append("picture_start_code");
  // 01 through AF
  for (int i=0x01; i<0xaf; i++)
    meanings.append(QString("slice_start_code - slice %1").arg(i));
  meanings.append("reserved");
  meanings.append("reserved");
  meanings.append("user_data_start_code");
  meanings.append("sequence_header_code");
  meanings.append("sequence_error_code");
  meanings.append("extension_start_code");
  meanings.append("reserved");
  meanings.append("sequence_end_code");
  meanings.append("group_start_code");
  meanings.append("system start codes");
  
  return meanings;
}

void NalUnit::interpreteStartCodeValue()
{
  if (start_code_value == 0)
    nal_unit_type = NalUnitType::PICTURE;
  else if (start_code_value >= 0x01 && start_code_value <= 0xaf)
  {
    nal_unit_type = NalUnitType::SLICE;
    slice_id = start_code_value - 1;
  }
  else if (start_code_value == 0xb0 || start_code_value == 0xb1 || start_code_value == 0xb6)
    nal_unit_type = NalUnitType::RESERVED;
  else if (start_code_value == 0xb2)
    nal_unit_type = NalUnitType::USER_DATA;
  else if (start_code_value == 0xb3)
    nal_unit_type = NalUnitType::SEQUENCE_HEADER;
  else if (start_code_value == 0xb4)
    nal_unit_type = NalUnitType::SEQUENCE_ERROR;
  else if (start_code_value == 0xb5)
    nal_unit_type = NalUnitType::EXTENSION_START;
  else if (start_code_value == 0xb7)
    nal_unit_type = NalUnitType::SEQUENCE_END;
  else if (start_code_value == 0xb8)
    nal_unit_type = NalUnitType::GROUP_START;
  else if (start_code_value >= 0xb9)
  {
    nal_unit_type = NalUnitType::SYSTEM_START_CODE;
    system_start_codes = start_code_value - 0xb9;
  }
  else
    nal_unit_type = NalUnitType::UNSPECIFIED;
}

} // namespace MPEG2