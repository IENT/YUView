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

#include "ParserAV1OBU.h"

#include <algorithm>

#include "parser/common/ParserMacros.h"
#include "parser/common/ReaderHelper.h"

#include "GlobalDecodingValues.h"
#include "FrameHeader.h"

using namespace AV1;

ParserAV1OBU::ParserAV1OBU(QObject *parent) : ParserBase(parent)
{
  // Reset all values in ParserAV1OBU
  decValues = GlobalDecodingValues();
}

unsigned int ParserAV1OBU::parseAndAddOBU(int obuID, QByteArray data, TreeItem *parent, std::optional<pairUint64> obuStartEndPosFile, QString *obuTypeName)
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

  // Read the OBU header
  OBU obu(obuStartEndPosFile, obuID);
  unsigned int nrBytesHeader;
  if (!obu.parseHeader(data, nrBytesHeader, obuRoot))
    return false;

  // Get the payload of the OBU
  QByteArray obuData = data.mid(nrBytesHeader, obu.obu_size);

  bool parsingSuccess = true;
  if (obu.obuType == OBUType::OBU_TEMPORAL_DELIMITER)
  {
    decValues.SeenFrameHeader = false;
  }
  if (obu.obuType == OBUType::OBU_SEQUENCE_HEADER)
  {
    // A sequence parameter set
    auto newSequenceHeader = QSharedPointer<SequenceHeader>(new SequenceHeader(obu));
    parsingSuccess = newSequenceHeader->parse(obuData, obuRoot);

    this->activeSequenceHeader = newSequenceHeader;

    if (obuTypeName)
      *obuTypeName = parsingSuccess ? "SEQ_HEAD" : "SEQ_HEAD(ERR)";
  }
  else if (obu.obuType == OBUType::OBU_FRAME || obu.obuType == OBUType::OBU_FRAME_HEADER)
  {
    auto newFrameHeader = QSharedPointer<FrameHeader>(new FrameHeader(obu));
    parsingSuccess = newFrameHeader->parse(obuData, obuRoot, this->activeSequenceHeader, decValues);

    if (obuTypeName)
      *obuTypeName = parsingSuccess ? "FRAME" : "FRAME(ERR)";
  }

  if (obuRoot)
    // Set a useful name of the TreeItem (the root for this NAL)
    obuRoot->itemData.append(QString("OBU %1: %2").arg(obu.obuIdx).arg(obuTypeToString(obu.obuType)) + specificDescription);

  return nrBytesHeader + (int)obu.obu_size;
}