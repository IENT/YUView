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
#include "TreeItem.h"

 /* The basic NAL unit. Contains the NAL header and the file position of the unit.
  */
struct NalUnitBase
{
  NalUnitBase(QUint64Pair filePosStartEnd, int nal_idx) : filePosStartEnd(filePosStartEnd), nal_idx(nal_idx), nal_unit_type_id(-1) {}
  virtual ~NalUnitBase() {} // This class is meant to be derived from.

  // Parse the header from the given data bytes. If a TreeItem pointer is provided, the values will be added to the tree as well.
  virtual bool parse_nal_unit_header(const QByteArray &header_data, TreeItem *root) = 0;

  // Pointer to the first byte of the start code of the NAL unit
  QUint64Pair filePosStartEnd;

  // The index of the nal within the bitstream
  int nal_idx;

  // Get the NAL header including the start code
  virtual QByteArray getNALHeader() const = 0;
  virtual bool isParameterSet() const = 0;
  virtual int  getPOC() const { return -1; }
  // Get the raw NAL unit (excluding a start code, including nal unit header and payload)
  // This only works if the payload was saved of course
  QByteArray getRawNALData() const { return getNALHeader() + nalPayload; }

  // Each nal unit (in all known standards) has a type id
  unsigned int nal_unit_type_id;

  // Optionally, the NAL unit can store it's payload. A parameter set, for example, can thusly be saved completely.
  QByteArray nalPayload;
};
