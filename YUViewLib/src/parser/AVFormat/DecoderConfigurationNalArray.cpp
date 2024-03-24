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

#include "DecoderConfigurationNalArray.h"

namespace parser::avformat
{

using namespace reader;

void DecoderConfigurationNALUnit::parse(const unsigned        unitID,
                                        SubByteReaderLogging &reader,
                                        ParserAnnexBHEVC *    hevcParser,
                                        BitratePlotModel *    bitrateModel)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "nal unit " + std::to_string(unitID));

  this->nalUnitLength = reader.readBits("nalUnitLength", 16);

  // Get the bytes of the raw nal unit to pass to the "real" hevc parser
  auto nalData = reader.readBytes("", nalUnitLength, Options().withLoggingDisabled());

  // Let the hevc annexB parser parse this
  auto parseResult =
      hevcParser->parseAndAddNALUnit(unitID, nalData, {}, {}, reader.getCurrentItemTree());
  if (parseResult.success && bitrateModel != nullptr && parseResult.bitrateEntry)
    bitrateModel->addBitratePoint(0, *parseResult.bitrateEntry);
}

void DecoderConfigurationNALArray::parse(const unsigned        arrayID,
                                         SubByteReaderLogging &reader,
                                         ParserAnnexBHEVC *    hevcParser,
                                         BitratePlotModel *    bitrateModel)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "nal unit array " + std::to_string(arrayID));

  // The next 3 bytes contain info about the array
  this->array_completeness = reader.readFlag("array_completeness");
  reader.readFlag("reserved_flag_false", Options().withCheckEqualTo(0, CheckLevel::Warning));
  this->nal_unit_type = reader.readBits("nal_unit_type", 6);
  this->numNalus      = reader.readBits("numNalus", 16);

  for (unsigned i = 0; i < numNalus; i++)
  {
    DecoderConfigurationNALUnit nal;
    nal.parse(i, reader, hevcParser, bitrateModel);
    nalList.push_back(nal);
  }
}

} // namespace parser::avformat
