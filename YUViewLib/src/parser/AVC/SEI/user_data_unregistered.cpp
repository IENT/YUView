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

namespace parser::avc

{
using namespace reader;

SEIParsingResult
user_data_unregistered::parse(reader::SubByteReaderLogging &          reader,
                              bool                                    reparse,
                              SPSMap &                                spsMap,
                              std::shared_ptr<seq_parameter_set_rbsp> associatedSPS)
{
  this->uuid_iso_iec_11578 = reader.readBytes("uuid_iso_iec_11578", 16);
  
  // TODO: Test this
  auto firstFourBytes = reader.peekBytes(4);
  if (firstFourBytes == ByteVector({'x', '2', '6', '4'}))
  {
    // This seems to be x264 user data. These contain the encoder settings which might be useful
    SubByteReaderLoggingSubLevel subLevel(reader, "x264 user data");
    
    // TODO: Implement parsing of this using std only
    // Old code:
// #if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
//     auto list = user_data_message.split(QRegExp("[\r\n\t ]+"), Qt::SkipEmptyParts);
// #else
//     auto list = user_data_message.split(QRegExp("[\r\n\t ]+"), QString::SkipEmptyParts);
// #endif

//     bool    options = false;
//     QString aggregate_string;
//     for (QString val : list)
//     {
//       if (options)
//       {
//         QStringList option = val.split("=");
//         if (option.length() == 2)
//         {
//           new TreeItem(option[0], option[1], "", "", "", itemTree);
//         }
//       }
//       else
//       {
//         if (val == "-")
//         {
//           if (aggregate_string != " -" && aggregate_string != "-" && !aggregate_string.isEmpty())
//             new TreeItem("Info", aggregate_string, "", "", "", itemTree);
//           aggregate_string = "";
//         }
//         else if (val == "options:")
//         {
//           options = true;
//           if (aggregate_string != " -" && aggregate_string != "-" && !aggregate_string.isEmpty())
//             new TreeItem("Info", aggregate_string, "", "", "", itemTree);
//         }
//         else
//           aggregate_string += " " + val;
//       }
//     }
  }
  else
  {
    unsigned i = 0;
    while (reader.canReadBits(8))
    {
      reader.readBytes(formatArray("raw_byte", i++), 8);
    }
  }

  return SEIParsingResult::OK;
}

} // namespace parser::avc