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

#include "rep_format.h"

namespace parser::hevc
{

using namespace reader;

void rep_format::parse(SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "rep_format()");

  this->pic_width_vps_in_luma_samples  = reader.readBits("pic_width_vps_in_luma_samples", 16);
  this->pic_height_vps_in_luma_samples = reader.readBits("pic_height_vps_in_luma_samples", 16);
  this->chroma_and_bit_depth_vps_present_flag =
      reader.readFlag("chroma_and_bit_depth_vps_present_flag");
  if (this->chroma_and_bit_depth_vps_present_flag)
  {
    chroma_format_vps_idc = reader.readBits("chroma_format_vps_idc", 2);
    if (this->chroma_format_vps_idc == 3)
      this->separate_colour_plane_vps_flag = reader.readFlag("separate_colour_plane_vps_flag");
    this->bit_depth_vps_luma_minus8   = reader.readBits("bit_depth_vps_luma_minus8", 4);
    this->bit_depth_vps_chroma_minus8 = reader.readBits("bit_depth_vps_chroma_minus8", 4);
  }
  this->conformance_window_vps_flag = reader.readFlag("conformance_window_vps_flag");
  if (this->conformance_window_vps_flag)
  {
    conf_win_vps_left_offset   = reader.readUEV("conf_win_vps_left_offset");
    conf_win_vps_right_offset  = reader.readUEV("conf_win_vps_right_offset");
    conf_win_vps_top_offset    = reader.readUEV("conf_win_vps_top_offset");
    conf_win_vps_bottom_offset = reader.readUEV("conf_win_vps_bottom_offset");
  }
}

} // namespace parser::hevc
