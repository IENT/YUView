/*  This file is part of YUView - The YUV player with advanced analytics toolset
*   <https://github.com/IENT/YUView>
*   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
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

#include "ParserAnnexBVVC.h"

#include <algorithm>
#include <cmath>

#include "parser/common/Macros.h
#include "parser/common/ReaderHelper.h"

#define PARSER_VVC_DEBUG_OUTPUT 0
#if PARSER_VVC_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_VVC(msg) qDebug() << msg
#else
#define DEBUG_VVC(msg) ((void)0)
#endif

namespace parser
{

double ParserAnnexBVVC::getFramerate() const
{
  return DEFAULT_FRAMERATE;
}

QSize ParserAnnexBVVC::getSequenceSizeSamples() const
{
  return QSize(352, 288);
}

yuvPixelFormat ParserAnnexBVVC::getPixelFormat() const
{
  return yuvPixelFormat(Subsampling::YUV_420, 8);
}

QList<QByteArray> ParserAnnexBVVC::getSeekFrameParamerSets(int iFrameNr, uint64_t &filePos)
{
  Q_UNUSED(iFrameNr);
  Q_UNUSED(filePos);
  return {};
}

QByteArray ParserAnnexBVVC::getExtradata()
{
  return {};
}

QPair<int,int> ParserAnnexBVVC::getProfileLevel()
{
  return QPair<int,int>(0,0);
}

Ratio ParserAnnexBVVC::getSampleAspectRatio()
{
  return Ratio({1, 1});
}

ParserAnnexB::ParseResult ParserAnnexBVVC::parseAndAddNALUnit(int nalID, QByteArray data, std::optional<BitratePlotModel::BitrateEntry> bitrateEntry, std::optional<pairUint64> nalStartEndPosFile, TreeItem *parent)
{
  ParserAnnexB::ParseResult parseResult;
  
  if (nalID == -1 && data.isEmpty())
    return parseResult;

  // Skip the NAL unit header
  int skip = 0;
  if (data.at(0) == (char)0 && data.at(1) == (char)0 && data.at(2) == (char)1)
    skip = 3;
  else if (data.at(0) == (char)0 && data.at(1) == (char)0 && data.at(2) == (char)0 && data.at(3) == (char)1)
    skip = 4;
  else
    // No NAL header found
    skip = 0;

  // Read two bytes (the nal header)
  QByteArray nalHeaderBytes = data.mid(skip, 2);
  QByteArray payload = data.mid(skip + 2);
  
  // Use the given tree item. If it is not set, use the nalUnitMode (if active).
  // Create a new TreeItem root for the NAL unit. We don't set data (a name) for this item
  // yet. We want to parse the item and then set a good description.
  QString specificDescription;
  TreeItem *nalRoot = nullptr;
  if (parent)
    nalRoot = new TreeItem(parent);
  else if (!packetModel->isNull())
    nalRoot = new TreeItem(packetModel->getRootItem());

  ParserAnnexB::logNALSize(data, nalRoot, nalStartEndPosFile);

  // Create a nal_unit and read the header
  nal_unit_vvc nal_vvc(nalID, nalStartEndPosFile);
  if (!nal_vvc.parseNalUnitHeader(nalHeaderBytes, nalRoot))
    return parseResult;

  if (nal_vvc.isAUDelimiter())
  {
    DEBUG_VVC("Start of new AU. Adding bitrate " << sizeCurrentAU);
    
    BitratePlotModel::BitrateEntry entry;
    if (bitrateEntry)
    {
      entry.pts = bitrateEntry->pts;
      entry.dts = bitrateEntry->dts;
      entry.duration = bitrateEntry->duration;
    }
    else
    {
      entry.pts = counterAU;
      entry.dts = counterAU;  // TODO: Not true. We need to parse the VVC header data
      entry.duration = 1;
    }
    entry.bitrate = sizeCurrentAU;
    entry.keyframe = false; // TODO: Also not correct. We need parsing.
    parseResult.bitrateEntry = entry;

    if (counterAU > 0)
    {
      const bool curFrameIsRandomAccess = (counterAU == 1);
      if (!addFrameToList(counterAU, curFrameFileStartEndPos, curFrameIsRandomAccess))
      {
        ReaderHelper::addErrorMessageChildItem(QString("Error adding frame to frame list."), parent);
        return parseResult;
      }
      if (curFrameFileStartEndPos)
        DEBUG_VVC("Adding start/end " << curFrameFileStartEndPos->first << "/" << curFrameFileStartEndPos->second << " - POC " << counterAU << (curFrameIsRandomAccess ? " - ra" : ""));
      else
        DEBUG_VVC("Adding start/end %d/%d - POC NA/NA" << (curFrameIsRandomAccess ? " - ra" : ""));
    }
    curFrameFileStartEndPos = nalStartEndPosFile;
    sizeCurrentAU = 0;
    counterAU++;
  }
  else if (curFrameFileStartEndPos && nalStartEndPosFile)
    curFrameFileStartEndPos->second = nalStartEndPosFile->second;

  sizeCurrentAU += data.size();

  if (nalRoot)
    // Set a useful name of the TreeItem (the root for this NAL)
    nalRoot->itemData.append(QString("NAL %1: %2").arg(nal_vvc.nalIdx).arg(nal_vvc.nalUnitTypeID) + specificDescription);

  parseResult.success = true;
  return parseResult;
}

QByteArray ParserAnnexBVVC::nal_unit_vvc::getNALHeader() const
{ 
  int out = ((int)nalUnitTypeID << 9) + (nuh_layer_id << 3) + nuh_temporal_id_plus1;
  char c[2] = { (char)(out >> 8), (char)out };
  return QByteArray(c, 2);
}

bool ParserAnnexBVVC::nal_unit_vvc::parseNalUnitHeader(const QByteArray &parameterSetData, TreeItem *root)
{
  // Create a sub byte parser to access the bits
  ReaderHelper reader(parameterSetData, root, "nal_unit_header()");

  READZEROBITS(1, "forbidden_zero_bit");
  READZEROBITS(1, "nuh_reserved_zero_bit");
  READBITS(nuh_layer_id, 6);
  // Read nal unit type
  QStringList nal_unit_type_id_meaning = QStringList()
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "AUD_NUT Access unit delimiter access_unit_delimiter_rbsp( )"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unknown"
    << "Unspecified";
  READBITS_M(nalUnitTypeID, 5, nal_unit_type_id_meaning);
  READBITS(nuh_temporal_id_plus1, 3);

  return true;
}

} // namespace parser