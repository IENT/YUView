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

#include "dpb_parameters.h"

namespace parser::vvc
{

using namespace parser::reader;

void dpb_parameters::parse(SubByteReaderLogging &reader,
                           unsigned         MaxSubLayersMinus1,
                           bool             subLayerInfoFlag)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "dpb_parameters");

  for (unsigned i = (subLayerInfoFlag ? 0 : MaxSubLayersMinus1); i <= MaxSubLayersMinus1; i++)
  {
    this->dpb_max_dec_pic_buffering_minus1.push_back(
        reader.readUEV("dpb_max_dec_pic_buffering_minus1"));
    this->dpb_max_num_reorder_pics.push_back(reader.readUEV("dpb_max_num_reorder_pics"));
    this->dpb_max_latency_increase_plus1.push_back(
        reader.readUEV("dpb_max_latency_increase_plus1"));
  }
}

} // namespace parser::vvc
