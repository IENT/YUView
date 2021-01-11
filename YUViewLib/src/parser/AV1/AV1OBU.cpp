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

#include <algorithm>

#include "common/Macros.h"
#include "common/ReaderHelper.h"

#define READDELTAQ(into) do { if (!read_delta_q(#into, into, reader)) return false; } while(0)

#define SELECT_SCREEN_CONTENT_TOOLS 2
#define SELECT_INTEGER_MV 2
#define NUM_REF_FRAMES 8
#define REFS_PER_FRAME 7
#define PRIMARY_REF_NONE 7
#define MAX_SEGMENTS 8
#define SEG_LVL_MAX 8
#define SEG_LVL_REF_FRAME 5
#define MAX_LOOP_FILTER 63
#define SEG_LVL_ALT_Q 0
#define TOTAL_REFS_PER_FRAME 8

#define SUPERRES_DENOM_BITS 3
#define SUPERRES_DENOM_MIN 9
#define SUPERRES_NUM 8

// The indices into the RefFrame list
#define INTRA_FRAME 0
#define LAST_FRAME 1
#define LAST2_FRAME 2
#define LAST3_FRAME 3
#define GOLDEN_FRAME 4
#define BWDREF_FRAME 5
#define ALTREF2_FRAME 6
#define ALTREF_FRAME 7

#define MAX_TILE_WIDTH 4096
#define MAX_TILE_AREA 4096 * 2304
#define MAX_TILE_COLS 64
#define MAX_TILE_ROWS 64

namespace parser
{

const QStringList ParserAV1OBU::obu_type_toString = QStringList()
  << "RESERVED" << "OBU_SEQUENCE_HEADER" << "OBU_TEMPORAL_DELIMITER" << "OBU_FRAME_HEADER" << "OBU_TILE_GROUP" 
  << "OBU_METADATA" << "OBU_FRAME" << "OBU_REDUNDANT_FRAME_HEADER" << "OBU_TILE_LIST" << "OBU_PADDING";

parserAV1OBU::parserAV1OBU(QObject *parent) : Base(parent)
{
  // Reset all values in parserAV1OBU
  memset(&decValues, 0, sizeof(global_decoding_values));
  decValues.PrevFrameID = -1;
}

parserAV1OBU::obu_unit::obu_unit(QSharedPointer<obu_unit> obu_src)
{
  // Copy all members but the payload. The payload is only saved for specific obus.
  filePosStartEnd = obu_src->filePosStartEnd;
  obu_idx = obu_src->obu_idx;
  obu_type = obu_src->obu_type;
  obu_extension_flag = obu_src->obu_extension_flag;
  obu_has_size_field = obu_src->obu_has_size_field;
  temporal_id = obu_src->temporal_id;
  spatial_id = obu_src->spatial_id;
  obu_size = obu_src->obu_size;
}



std::pair<unsigned, std::string> parserAV1OBU::parseAndAddOBU(int obuID, QByteArray data, TreeItem *parent, pairUint64 obuStartEndPosFile)
{
  // Use the given tree item. If it is not set, use the nalUnitMode (if active). 
  // We don't set data (a name) for this item yet. 
  // We want to parse the item and then set a good description.
  QString specificDescription;
  TreeItem *obuRoot = nullptr;
  if (parent)
    obuRoot = new TreeItem(parent);
  else if (!packetModel->isNull())
    obuRoot = new TreeItem(packetModel->getRootItem());

  // Log OBU size
  new TreeItem("size", data.size(), obuRoot);

  // Read the OBU header
  obu_unit obu(obuStartEndPosFile, obuID);
  unsigned int nrBytesHeader;
  if (!obu.parse_obu_header(data, nrBytesHeader, obuRoot))
    return {};

  // Get the payload of the OBU
  QByteArray obuData = data.mid(nrBytesHeader, obu.obu_size);

  bool parsingSuccess = true;
  std::string obuTypeName;
  if (obu.obu_type == OBU_TEMPORAL_DELIMITER)
  {
    decValues.SeenFrameHeader = false;
  }
  if (obu.obu_type == OBU_SEQUENCE_HEADER)
  {
    // A sequence parameter set
    auto new_sequence_header = QSharedPointer<sequence_header>(new sequence_header(obu));
    parsingSuccess = new_sequence_header->parse_sequence_header(obuData, obuRoot);

    active_sequence_header = new_sequence_header;

    obuTypeName = parsingSuccess ? "SEQ_HEAD" : "SEQ_HEAD(ERR)";
  }
  else if (obu.obu_type == OBU_FRAME || obu.obu_type == OBU_FRAME_HEADER)
  {
    auto new_frame_header = QSharedPointer<frame_header>(new frame_header(obu));
    parsingSuccess = new_frame_header->parse_frame_header(obuData, obuRoot, active_sequence_header, decValues);

    obuTypeName = parsingSuccess ? "FRAME" : "FRAME(ERR)";
  }

  if (obuRoot)
    // Set a useful name of the TreeItem (the root for this NAL)
    obuRoot->itemData.append(QString("OBU %1: %2").arg(obu.obu_idx).arg(obu_type_toString.value(obu.obu_type)) + specificDescription);

  return {nrBytesHeader + (int)obu.obu_size, obuTypeName};
}





bool parserAV1OBU::frame_header::parse_frame_header(const QByteArray &frameHeaderData, TreeItem *root, QSharedPointer<sequence_header> seq_header, global_decoding_values &decValues)
{
  obuPayload = frameHeaderData;
  ReaderHelper reader(frameHeaderData, root, "frame_header_obu()");

  if (decValues.SeenFrameHeader)
  {
    // TODO: Is it meant like this?
    //frame_header_copy();
    if (!parse_uncompressed_header(reader, seq_header, decValues))
      return false;
  } 
  else 
  {
    decValues.SeenFrameHeader = true;
    if (!parse_uncompressed_header(reader, seq_header, decValues))
      return false;
    if (show_existing_frame) 
    {
      // decode_frame_wrapup()
      decValues.SeenFrameHeader = false;
    } 
    else 
    {
      //TileNum = 0;
      decValues.SeenFrameHeader = true;
    }
  }

  return true;
}











} // namespace parser