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

#include "decoding_capability_information_rbsp.h"

namespace parser::vvc
{

using namespace parser::reader;

void decoding_capability_information_rbsp::parse(SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "decoding_capability_information_rbsp");

  this->dci_reserved_zero_4bits = reader.readBits("dci_reserved_zero_4bits", 4);
  this->dci_num_ptls_minus1 =
      reader.readBits("dci_num_ptls_minus1", 4, Options().withCheckRange({0, 14}));
  for (unsigned i = 0; i <= dci_num_ptls_minus1; i++)
  {
    this->profile_tier_level_instance.parse(reader, 1, 0);
  }
  this->dci_extension_flag = reader.readFlag("dci_extension_flag", Options().withCheckEqualTo(0));
  if (this->dci_extension_flag)
  {
    while (reader.more_rbsp_data())
    {
      this->dci_extension_data_flag = reader.readFlag("dci_extension_data_flag");
    }
  }
  this->rbsp_trailing_bits_instance.parse(reader);
}

} // namespace parser::vvc
