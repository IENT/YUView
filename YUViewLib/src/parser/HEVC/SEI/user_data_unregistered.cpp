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

#include "user_data_unregistered.h"

#include "parser/common/functions.h"

#include <iostream>
#include <sstream>

namespace parser::hevc

{
using namespace reader;

SEIParsingResult
user_data_unregistered::parse(SubByteReaderLogging &                  reader,
                              bool                                    reparse,
                              VPSMap &                                vpsMap,
                              SPSMap &                                spsMap,
                              std::shared_ptr<seq_parameter_set_rbsp> associatedSPS)
{
  (void)reparse;
  (void)vpsMap;
  (void)spsMap;
  (void)associatedSPS;

  SubByteReaderLoggingSubLevel subLevel(reader, "user_data_unregistered");

  this->uuid_iso_iec_11578 = reader.readBytes("uuid_iso_iec_11578", 16);

  auto firstFourBytes = reader.peekBytes(4);
  if (firstFourBytes == ByteVector({'x', '2', '6', '4'}))
  {
    // This seems to be x264 user data. These contain the encoder settings which might be useful
    SubByteReaderLoggingSubLevel subLevel(reader, "x265 user data");

    std::string dataString;
    while (reader.canReadBits(8))
    {
      auto val = reader.readBits("", 8, Options().withLoggingDisabled());
      dataString.push_back(char(val));
    }

    auto splitDash = splitX26XOptionsString(dataString, " - ");

    unsigned i = 0;
    for (const auto &s : splitDash)
    {
      if (i == 0)
        reader.logArbitrary("Encoder Tag", s);
      else if (i == 1)
        reader.logArbitrary("Version", s);
      else if (i == 2)
        reader.logArbitrary("Codec", s);
      else if (i == 3)
        reader.logArbitrary("Version", s);
      else if (i == 4)
        reader.logArbitrary("URL", s);
      else if (i == 5)
      {
        auto                         options = splitX26XOptionsString(s, " ");
        SubByteReaderLoggingSubLevel optionsLevel(reader, "options");
        for (const auto &option : options)
        {
          if (option == "options:")
            continue;
          auto optionSplit = splitX26XOptionsString(option, "=");
          if (optionSplit.size() != 2)
            reader.logArbitrary("Opt", option);
          else
            reader.logArbitrary(optionSplit[0], optionSplit[1]);
        }
      }
      i++;
    }
  }
  else
  {
    unsigned i = 0;
    while (reader.canReadBits(8))
    {
      reader.readBytes(formatArray("raw_byte", i++), 1);
    }
  }

  return SEIParsingResult::OK;
}

} // namespace parser::hevc