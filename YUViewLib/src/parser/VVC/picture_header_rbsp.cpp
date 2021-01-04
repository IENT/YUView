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

#include "picture_header_rbsp.h"

#include "pic_parameter_set_rbsp.h"
#include "seq_parameter_set_rbsp.h"
#include "slice_layer_rbsp.h"

namespace parser::vvc
{

using namespace parser::reader;

void picture_header_rbsp::parse(SubByteReaderLogging &                 reader,
                                VPSMap &                          vpsMap,
                                SPSMap &                          spsMap,
                                PPSMap &                          ppsMap,
                                std::shared_ptr<slice_layer_rbsp> sl)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "picture_header_rbsp");

  this->picture_header_structure_instance = std::make_shared<picture_header_structure>();
  this->picture_header_structure_instance->parse(reader, vpsMap, spsMap, ppsMap, sl);
  this->rbsp_trailing_bits_instance.parse(reader);
}

} // namespace parser::vvc
