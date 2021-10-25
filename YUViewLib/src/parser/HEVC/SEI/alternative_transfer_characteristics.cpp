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

#include "alternative_transfer_characteristics.h"

#include <parser/common/functions.h>

namespace parser::hevc
{

using namespace reader;

SEIParsingResult
alternative_transfer_characteristics::parse(reader::SubByteReaderLogging &          reader,
                                            bool                                    reparse,
                                            VPSMap &                                vpsMap,
                                            SPSMap &                                spsMap,
                                            std::shared_ptr<seq_parameter_set_rbsp> associatedSPS)
{
  (void)reparse;
  (void)vpsMap;
  (void)spsMap;
  (void)associatedSPS;

  SubByteReaderLoggingSubLevel subLevel(reader, "alternative_transfer_characteristics");

  this->preferred_transfer_characteristics = reader.readBits(
      "preferred_transfer_characteristics",
      8,
      Options()
          .withMeaningVector(
              {"Reserved For future use by ITU-T | ISO/IEC",
               "Rec. ITU-R BT.709-6 Rec.ITU - R BT.1361-0 conventional colour gamut system",
               "Unspecified",
               "Reserved For future use by ITU-T | ISO / IEC",
               "Rec. ITU-R BT.470-6 System M (historical) (NTSC)",
               "Rec. ITU-R BT.470-6 System B, G (historical)",
               "Rec. ITU-R BT.601-6 525 or 625, Rec.ITU - R BT.1358 525 or 625, Rec.ITU - R "
               "BT.1700 NTSC Society of Motion Picture and Television Engineers 170M(2004)",
               "Society of Motion Picture and Television Engineers 240M (1999)",
               "Linear transfer characteristics",
               "Logarithmic transfer characteristic (100:1 range)",
               "Logarithmic transfer characteristic (100 * Sqrt( 10 ) : 1 range)",
               "IEC 61966-2-4",
               "Rec. ITU-R BT.1361 extended colour gamut system",
               "IEC 61966-2-1 (sRGB or sYCC)",
               "Rec. ITU-R BT.2020-2 for 10 bit system",
               "Rec. ITU-R BT.2020-2 for 12 bit system",
               "SMPTE ST 2084 for 10, 12, 14 and 16-bit systems Rec. ITU-R BT.2100-0 "
               "perceptual "
               "quantization (PQ) system",
               "SMPTE ST 428-1",
               "Association of Radio Industries and Businesses (ARIB) STD-B67 Rec. ITU-R "
               "BT.2100-0 "
               "hybrid log-gamma (HLG) system"})
          .withMeaning("Reserved For future use by ITU-T | ISO/IEC"));

  return SEIParsingResult::OK;
}

} // namespace parser::hevc