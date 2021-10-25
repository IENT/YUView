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

#include "picture_header.h"

#include "parser/common/SubByteReaderLogging.h"
#include <parser/common/Functions.h>

namespace
{

auto picture_coding_type_meaning =
    std::map<int, std::string>({{0, "Forbidden"},
                                {1, "intra-coded (I)"},
                                {2, "predictive-coded (P)"},
                                {3, "bidirectionally-predictive-coded (B)"},
                                {4, "dc intra-coded (D)"},
                                {5, "Reserved"}});

}

namespace parser::mpeg2
{

using namespace parser::reader;

void picture_header::parse(SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "picture_header");

  this->temporal_reference  = reader.readBits("temporal_reference", 10);
  this->picture_coding_type = reader.readBits(
      "picture_coding_type", 3, Options().withMeaningMap(picture_coding_type_meaning));
  this->vbv_delay = reader.readBits("vbv_delay", 16);
  if (this->picture_coding_type == 2 || this->picture_coding_type == 3)
  {
    this->full_pel_forward_vector = reader.readFlag("full_pel_forward_vector");
    this->forward_f_code          = reader.readBits("forward_f_code", 3);
  }
  if (this->picture_coding_type == 3)
  {
    this->full_pel_backward_vector = reader.readFlag("full_pel_backward_vector");
    this->backward_f_code          = reader.readBits("backward_f_code", 3);
  }

  bool abort = false;
  while (reader.canReadBits(9))
  {
    if (!reader.readFlag("extra_bit_picture"))
      abort = false;

    if (!abort)
      this->extra_information_picture_list.push_back(
          reader.readBits("extra_information_picture", 8));
  }
}

std::string picture_header::getPictureTypeString() const
{
  if (this->picture_coding_type < picture_coding_type_meaning.size())
    return picture_coding_type_meaning[this->picture_coding_type];
  return {};
}

} // namespace parser::mpeg2