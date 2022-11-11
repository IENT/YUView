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

#include <common/Typedef.h>

#include <optional>

#include "obu_header.h"

namespace parser::av1
{

class ObuPayload
{
public:
  ObuPayload()          = default;
  virtual ~ObuPayload() = default;
};

class OpenBitstreamUnit
{
public:
  OpenBitstreamUnit(int obu_idx, std::optional<FileStartEndPos> filePosStartEnd) : obu_idx(obu_idx)
  {
    if (filePosStartEnd)
      this->filePosStartEnd = *filePosStartEnd;
  }

  obu_header                  header;
  std::shared_ptr<ObuPayload> payload;

  // Pointer to the first byte of the start code of the NAL unit
  FileStartEndPos filePosStartEnd;
  int             obu_idx{};
};

} // namespace parser::av1
