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

#include "PictureHeader.h"

#include "parser/common/parserMacros.h"
#include "parser/common/ReaderHelper.h"

namespace MPEG2
{

QStringList picture_coding_type_meaning = QStringList()
    << "Forbidden" << "intra-coded (I)" << "predictive-coded (P)" << "bidirectionally-predictive-coded (B)" << "dc intra-coded (D)" << "Reserved";

bool PictureHeader::parse(const QByteArray & parameterSetData, TreeItem * root)
{
  this->nalPayload = parameterSetData;
  ReaderHelper reader(parameterSetData, root, "picture_header");

  READBITS(temporal_reference, 10);
  
  READBITS_M(picture_coding_type, 3, picture_coding_type_meaning);
  READBITS(vbv_delay, 16);
  if (picture_coding_type == 2 || picture_coding_type == 3)
  {
    READFLAG(full_pel_forward_vector);
    READBITS(forward_f_code, 3);
  }
  if (picture_coding_type == 3)
  {
    READFLAG(full_pel_backward_vector);
    READBITS(backward_f_code, 3);
  }

  bool abort = false;
  while (reader.testReadingBits(9)) 
  {
    bool extra_bit_picture;
    READFLAG(extra_bit_picture);
    if (!extra_bit_picture)
      abort = false;
    
    if (!abort)
    {
      unsigned int extra_information_picture;
      READBITS(extra_information_picture, 8);
    }
  }
  return true;
}

QString PictureHeader::getPictureTypeString() const
{
  auto i = std::min(int(this->picture_coding_type), picture_coding_type_meaning.count() - 1);
  return picture_coding_type_meaning[i];
}

} // namespace MPEG2