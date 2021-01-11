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

#include "AV1OBU.h"

#include "OpenBitstreamUnit.h"
#include "frame_header_obu.h"
#include "parser/common/SubByteReaderLogging.h"

namespace parser
{

using namespace reader;
using namespace av1;

ParserAV1OBU::ParserAV1OBU(QObject *parent) : Base(parent) { decValues.PrevFrameID = -1; }

std::pair<unsigned, std::string> ParserAV1OBU::parseAndAddOBU(int         obuID,
                                                              ByteVector &data,
                                                              TreeItem *  parent,
                                                              pairUint64  obuStartEndPosFile)
{
  // Use the given tree item. If it is not set, use the nalUnitMode (if active).
  // We don't set data (a name) for this item yet.
  // We want to parse the item and then set a good description.
  TreeItem *obuRoot = nullptr;
  if (parent)
    obuRoot = new TreeItem(parent);
  else if (!packetModel->isNull())
    obuRoot = new TreeItem(packetModel->getRootItem());

  SubByteReaderLogging reader(data, obuRoot);

  reader.logArbitrary("Size", std::to_string(data.size()));

  // Read the OBU header
  OpenBitstreamUnit obu(obuID, obuStartEndPosFile);
  obu.header.parse(reader);

  std::string obuTypeName;
  if (obu.header.obu_type == ObuType::OBU_TEMPORAL_DELIMITER)
  {
    decValues.SeenFrameHeader = false;
  }
  if (obu.header.obu_type == ObuType::OBU_SEQUENCE_HEADER)
  {
    auto new_sequence_header = std::make_shared<sequence_header_obu>();
    new_sequence_header->parse(reader);

    this->active_sequence_header = new_sequence_header;

    obuTypeName = "Sequence Header";
    obu.payload = new_sequence_header;
  }
  else if (obu.header.obu_type == ObuType::OBU_FRAME ||
           obu.header.obu_type == ObuType::OBU_FRAME_HEADER)
  {
    auto new_frame_header = std::shared_ptr<frame_header_obu>();
    new_frame_header->parse(reader,
                            this->active_sequence_header,
                            this->decValues,
                            obu.header.temporal_id,
                            obu.header.spatial_id);

    obuTypeName = "Frame";
    obu.payload = new_frame_header;
  }

  if (obuRoot)
  {
    auto name = "OBU " + std::to_string(obu.obu_idx) + ": " + to_string(obu.header.obu_type) + " " +
                obuTypeName;
    obuRoot->setProperties(name);
  }

  return {unsigned(data.size()), obuTypeName};
}

} // namespace parser