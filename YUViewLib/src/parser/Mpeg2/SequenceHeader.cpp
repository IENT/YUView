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

#include "SequenceHeader.h"

#include "parser/common/parserMacros.h"
#include "parser/common/ReaderHelper.h"

namespace MPEG2
{

bool SequenceHeader::parse(const QByteArray & parameterSetData, TreeItem * root)
{
  this->nalPayload = parameterSetData;
  ReaderHelper reader(parameterSetData, root, "sequence_header()");

  READBITS(horizontal_size_value, 12);
  READBITS(vertical_size_value, 12);
  QStringList aspect_ratio_information_meaning = QStringList()
    << "Forbidden"
    << "SAR 1.0 (Square Sample)"
    << "DAR 3:4"
    << "DAR 9:16"
    << "DAR 1:2.21"
    << "Reserved";
  READBITS_M(aspect_ratio_information, 4, aspect_ratio_information_meaning);
  QStringList frame_rate_code_meaning = QStringList()
    << "Forbidden"
    << "24000:1001 (23.976...)"
    << "24"
    << "25"
    << "30000:1001 (29.97...)"
    << "30"
    << "50"
    << "60000:1001 (59.94)"
    << "60"
    << "Reserved";
  READBITS_M(frame_rate_code, 4, frame_rate_code_meaning);
  READBITS_M(bit_rate_value, 18, "The lower 18 bits of bit_rate.");
  READFLAG(marker_bit);
  if (!marker_bit)
    reader.addErrorMessageChildItem("The marker_bit shall be set to 1 to prevent emulation of start codes.");
  READBITS_M(vbv_buffer_size_value, 10, "the lower 10 bits of vbv_buffer_size");
  READFLAG(constrained_parameters_flag);
  READFLAG(load_intra_quantiser_matrix);
  if (load_intra_quantiser_matrix)
  {
    for (int i=0; i<64; i++)
      READBITS(intra_quantiser_matrix[i], 8);
  }
  READFLAG(load_non_intra_quantiser_matrix);
  if (load_non_intra_quantiser_matrix)
  {
    for (int i=0; i<64; i++)
      READBITS(non_intra_quantiser_matrix[i], 8);
  }
  return true;
}

} // namespace MPEG2