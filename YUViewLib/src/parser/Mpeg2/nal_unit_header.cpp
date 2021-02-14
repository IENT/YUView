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

namespace parser::mpeg2
{

using namespace parser::reader;

void nal_unit_header::parse(SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "header_code()");

  {
    Options opt;
    opt.meaningMap[0] = "picture_start_code";
    // 01 through AF
    for (int i = 0x01; i < 0xaf; i++)
      opt.meaningMap[i] = "slice_start_code - slice " + std::to_string(i);
    opt.meaningMap[176] = "reserved";
    opt.meaningMap[177] = "reserved";
    opt.meaningMap[178] = "user_data_start_code";
    opt.meaningMap[179] = "sequence_header_code";
    opt.meaningMap[180] = "sequence_error_code";
    opt.meaningMap[181] = "extension_start_code";
    opt.meaningMap[182] = "reserved";
    opt.meaningMap[183] = "sequence_end_code";
    opt.meaningMap[184] = "group_start_code";
    opt.meaningMap[185] = "system start codes";

    this->start_code_value = reader.readBits("start_code_value", 8, opt);
  }

  if (this->start_code_value == 0)
    this->nal_unit_type = NalType::PICTURE;
  else if (this->start_code_value >= 0x01 && this->start_code_value <= 0xaf)
  {
    this->nal_unit_type = NalType::SLICE;
    this->slice_id      = this->start_code_value - 1;
  }
  else if (this->start_code_value == 0xb0 || this->start_code_value == 0xb1 ||
           this->start_code_value == 0xb6)
    this->nal_unit_type = NalType::RESERVED;
  else if (this->start_code_value == 0xb2)
    this->nal_unit_type = NalType::USER_DATA;
  else if (this->start_code_value == 0xb3)
    this->nal_unit_type = NalType::SEQUENCE_HEADER;
  else if (this->start_code_value == 0xb4)
    this->nal_unit_type = NalType::SEQUENCE_ERROR;
  else if (this->start_code_value == 0xb5)
    this->nal_unit_type = NalType::EXTENSION_START;
  else if (this->start_code_value == 0xb7)
    this->nal_unit_type = NalType::SEQUENCE_END;
  else if (this->start_code_value == 0xb8)
    this->nal_unit_type = NalType::GROUP_START;
  else if (this->start_code_value >= 0xb9)
  {
    this->nal_unit_type      = NalType::SYSTEM_START_CODE;
    this->system_start_codes = this->start_code_value - 0xb9;
  }
  else
    this->nal_unit_type = NalType::UNSPECIFIED;
}

} // namespace parser::mpeg2