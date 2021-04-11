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

#include "vui_payload.h"

namespace parser::vvc
{

using namespace reader;

void vui_payload::parse(reader::SubByteReaderLogging &reader, unsigned payloadSize)
{
  vui.parse(reader, payloadSize);
  auto more_data_in_payload = !reader.byte_aligned() || reader.nrBytesRead() != payloadSize;
  if (more_data_in_payload)
  {
    size_t nrBits = 8 * payloadSize - reader.nrBitsRead();
    for (; nrBits >= 8; nrBits -= 8)
    {
      reader.readBits("vui_reserved_payload_extension_data_byte", 8);
    }
    reader.readBits("vui_reserved_payload_extension_data_byte", int(nrBits));

    reader.readBits("vui_payload_bit_equal_to_one", 1, Options().withCheckEqualTo(1));
    while (!reader.byte_aligned())
    {
      reader.readBits("vui_payload_bit_equal_to_zero", 1, Options().withCheckEqualTo(0));
    }
  }
}

} // namespace parser::vvc
