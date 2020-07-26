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

#include "parser/common/NalUnitBase.h"

namespace MPEG2
{

// All the different NAL unit types (T-REC-H.262-199507 Page 24 Table 6-1)
enum class NalUnitType
{
  UNSPECIFIED,
  PICTURE,
  SLICE,
  USER_DATA,
  SEQUENCE_HEADER,
  SEQUENCE_ERROR,
  EXTENSION_START,
  SEQUENCE_END,
  GROUP_START,
  SYSTEM_START_CODE,
  RESERVED
};

QString nalUnitTypeToString(NalUnitType nalUnitType);

/* The basic Mpeg2 NAL unit. Technically, there is no concept of NAL units in mpeg2 (h262) but there are start codes for some units
  * and there is a start code so we internally use the NAL concept.
  */
struct NalUnit : NalUnitBase
{
  NalUnit(int nalIdx, std::optional<pairUint64> filePosStartEnd) : NalUnitBase(nalIdx, filePosStartEnd) {}
  NalUnit(QSharedPointer<NalUnit> nalSrc);
  virtual ~NalUnit() {}

  // Parse the parameter set from the given data bytes. If a TreeItem pointer is provided, the values will be added to the tree as well.
  bool parseNalUnitHeader(const QByteArray &header_byte, TreeItem *root) override;

  virtual QByteArray getNALHeader() const override;
  virtual bool isParameterSet() const override { return nal_unit_type == NalUnitType::SEQUENCE_HEADER; }

  QStringList getStartCodeMeanings() const;
  void interpreteStartCodeValue();

  NalUnitType nal_unit_type {NalUnitType::UNSPECIFIED};
  unsigned int slice_id {0};         // in case of SLICE
  unsigned system_start_codes {0};   // in case of SYSTEM_START_CODE
  unsigned int start_code_value {0}; 
};

} // namespace MPEG2