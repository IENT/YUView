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

#include "vps_vui.h"

namespace parser::hevc
{

using namespace reader;

void vps_vui::parse(SubByteReaderLogging &reader, const bool vps_vui_present_flag)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "vps_vui()");

  this->cross_layer_pic_type_aligned_flag = reader.readFlag("cross_layer_pic_type_aligned_flag");
  if (!this->cross_layer_pic_type_aligned_flag)
    this->cross_layer_irap_aligned_flag = reader.readFlag("cross_layer_irap_aligned_flag");
  else
    this->cross_layer_irap_aligned_flag = vps_vui_present_flag;
  if (this->cross_layer_irap_aligned_flag)
    this->all_layers_idr_aligned_flag = reader.readFlag("all_layers_idr_aligned_flag");
  this->bit_rate_present_vps_flag = reader.readFlag("bit_rate_present_vps_flag");
  this->pic_rate_present_vps_flag = reader.readFlag("pic_rate_present_vps_flag");

  if (this->bit_rate_present_vps_flag || this->pic_rate_present_vps_flag)
  {
  }
}

} // namespace parser::hevc
