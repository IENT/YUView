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

#include "common/typedef.h"
#include "parser/common/TreeItem.h"

#include "CommonTypes.h"

#include <QString>
#include <QSharedPointer>
#include <optional>

namespace AV1
{

QString obuTypeToString(OBUType obuType);

/* The basic Open bitstream unit. Contains the OBU header and the file position of the unit.
*/
struct OBU
{
  OBU(std::optional<pairUint64> filePosStartEnd, int obuIdx) : filePosStartEnd(filePosStartEnd), obuIdx(obuIdx) {}
  OBU(QSharedPointer<OBU> obu_src);
  virtual ~OBU() {} // This class is meant to be derived from.

  // Parse the header the given data bytes.
  bool parseHeader(const QByteArray &header_data, unsigned int &nrBytesHeader, TreeItem *root);

  // Pointer to the first byte of the start code of the NAL unit
  std::optional<pairUint64> filePosStartEnd;

  // The index of the obu within the bitstream
  int obuIdx;

  virtual bool isParameterSet() { return false; }
  // Get the raw OBU unit
  // This only works if the payload was saved of course
  //QByteArray getRawOBUData() const { return getOBUHeader() + obuPayload; }

  // Each obu (in all known standards) has a type id
  OBUType obuType           {OBUType::UNSPECIFIED};
  bool    obu_extension_flag {false};
  bool    obu_has_size_field {false};
  
  // OBU extension header
  unsigned int temporal_id {0};
  unsigned int spatial_id  {0};

  uint64_t obu_size {0};

  // Optionally, the OBU unit can store it's payload. A parameter set, for example, can thusly be saved completely.
  QByteArray obuPayload;
};

} // namespace MPEG2