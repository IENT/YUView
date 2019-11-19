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

#include "parserSubtitleDVB.h"

#include "parserCommonMacros.h"

using namespace parserCommon;

int subtitle_dvb::parseDVBSubtitleSegment(int segmentID, QByteArray data, parserCommon::TreeItem *parent, QString *segmentTypeName)
{
  // Use the given tree item. If it is not set, use the nalUnitMode (if active).
  // We don't set data (a name) for this item yet. 
  // We want to parse the item and then set a good description.
  QString specificDescription;
  if (!parent)
    return -1;
  TreeItem *segmentRoot = new TreeItem(parent);

  // Read the subtitle_segment header
  QByteArray header_data = data.mid(0, 6);

  // Create a sub byte parser to access the bits
  reader_helper reader(header_data, segmentRoot, "subtitling_segment()");
  if (header_data.length() < 6)
    return reader.addErrorMessageChildItem("The subtitling_segment header must have at least six byte");
  
  unsigned int sync_byte;
  READBITS(sync_byte, 8);
  if (sync_byte != 15)
    return reader.addErrorMessageChildItem("The sync_byte must be 0x00001111 (15).");

  unsigned int segment_type;
  QMap<int,QString> segment_type_meaning;
  for (int i = 0; i < 16; i++)
    segment_type_meaning[i] = "reserved for future use";
  segment_type_meaning[16] = "page composition segment";
  segment_type_meaning[17] = "region composition segment";
  segment_type_meaning[18] = "CLUT definition segment";
  segment_type_meaning[19] = "object data segment";
  segment_type_meaning[20] = "display definition segment";
  segment_type_meaning[21] = "disparity signalling segment";
  segment_type_meaning[22] = "alternative_CLUT_segment";
  for (int i = 23; i <= 127; i++)
    segment_type_meaning[i] = "reserved for future use";
  segment_type_meaning[128] = "end of display set segment";
  for (int i = 129; i < 254; i++)
    segment_type_meaning[i] = "private data";
  segment_type_meaning[255] = "stuffing";
  READBITS_M(segment_type, 8, segment_type_meaning);

  unsigned int page_id;
  READBITS(page_id, 16);

  unsigned int segment_length;
  READBITS(segment_length, 16);

  // Parsing of the individial segment data is not implemented ... yet? 
  // Feel free to implement these. The specification is freely available :)

  if (segmentRoot)
    // Set a useful name of the TreeItem (the root for this NAL)
    segmentRoot->itemData.append(QString("DVB Subtitle %1").arg(segmentID));

  return 6 + segment_length;
}