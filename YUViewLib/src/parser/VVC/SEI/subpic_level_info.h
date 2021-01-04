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

#include "parser/common/SubByteReaderLogging.h"
#include "sei_payload.h"

namespace parser::vvc
{

class subpic_level_info : public sei_payload
{
public:
  subpic_level_info()  = default;
  ~subpic_level_info() = default;
  void parse(reader::SubByteReaderLogging &reader);

  unsigned           sli_num_ref_levels_minus1{};
  bool               sli_cbr_constraint_flag{};
  bool               sli_explicit_fraction_present_flag{};
  unsigned           sli_num_subpics_minus1{};
  unsigned           sli_max_sublayers_minus1{};
  bool               sli_sublayer_info_present_flag{};
  vector2d<unsigned> sli_non_subpic_layers_fraction{};
  vector2d<unsigned> sli_ref_level_idc{};
  vector3d<unsigned> sli_ref_level_fraction_minus1{};
};

} // namespace parser::vvc
