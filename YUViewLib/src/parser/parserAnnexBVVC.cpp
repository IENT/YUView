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

#include "parserAnnexBVVC.h"

#include <algorithm>
#include <cmath>

#include "parserCommonMacros.h"

using namespace parserCommon;

#define PARSER_VVC_DEBUG_OUTPUT 0
#if PARSER_VVC_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_VVC qDebug
#else
#define DEBUG_VVC(fmt,...) ((void)0)
#endif

double parserAnnexBVVC::getFramerate() const
{
  return DEFAULT_FRAMERATE;
}

QSize parserAnnexBVVC::getSequenceSizeSamples() const
{
  return QSize(352, 288);
}

yuvPixelFormat parserAnnexBVVC::getPixelFormat() const
{
  return yuvPixelFormat(YUV_420, 8);
}

QList<QByteArray> parserAnnexBVVC::getSeekFrameParamerSets(int iFrameNr, uint64_t &filePos)
{
  Q_UNUSED(iFrameNr);
  Q_UNUSED(filePos);
  return {};
}

QByteArray parserAnnexBVVC::getExtradata()
{
  return {};
}

QPair<int,int> parserAnnexBVVC::getProfileLevel()
{
  return QPair<int,int>(0,0);
}

QPair<int,int> parserAnnexBVVC::getSampleAspectRatio()
{
  return QPair<int,int>(1,1);
}

bool parserAnnexBVVC::parseAndAddNALUnit(int nalID, QByteArray data, BitrateItemModel *bitrateModel, TreeItem *parent, QUint64Pair nalStartEndPosFile, QString *nalTypeName)
{
  Q_UNUSED(nalTypeName);
  
  if (nalID == -1 && data.isEmpty())
    return false;

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

  // Create a nal_unit and read the header
  nal_unit_vvc nal_vvc(nalStartEndPosFile, nalID);
  if (!nal_vvc.parse_nal_unit_header(nalHeaderBytes, nalRoot))
    return false;

  if (nal_vvc.isAUDelimiter())
  {
    DEBUG_VVC("Start of new AU. Adding bitrate %d", sizeCurrentAU);
    
    BitrateItemModel::bitrateEntry entry;
    entry.pts = counterAU;
    entry.dts = counterAU;  // TODO: Not true. We need to parse the VVC header data
    entry.bitrate = sizeCurrentAU;
    entry.keyframe = false; // TODO: Also not correct. We need parsing.
    bitrateModel->addBitratePoint(0, entry);

    if (counterAU > 0)
    {
      const bool curFrameIsRandomAccess = (counterAU == 1);
      if (!addFrameToList(counterAU, curFrameFileStartEndPos, curFrameIsRandomAccess))
        return reader_helper::addErrorMessageChildItem(QString("Error adding frame to frame list."), parent);
      DEBUG_VVC("Adding start/end %d/%d - POC %d%s", curFrameFileStartEndPos.first, curFrameFileStartEndPos.second, counterAU, curFrameIsRandomAccess ? " - ra" : "");
    }
    curFrameFileStartEndPos = nalStartEndPosFile;
    sizeCurrentAU = 0;
    counterAU++;
  }
  else
    curFrameFileStartEndPos.second = nalStartEndPosFile.second;

  sizeCurrentAU += data.size();

  if (nalRoot)
    // Set a useful name of the TreeItem (the root for this NAL)
    nalRoot->itemData.append(QString("NAL %1: %2").arg(nal_vvc.nal_idx).arg(nal_vvc.nal_unit_type_id) + specificDescription);

  return true;
}

QByteArray parserAnnexBVVC::nal_unit_vvc::getNALHeader() const
{ 
  int out = ((int)nal_unit_type_id << 9) + (nuh_layer_id << 3) + nuh_temporal_id_plus1;
  char c[2] = { (char)(out >> 8), (char)out };
  return QByteArray(c, 2);
}

bool parserAnnexBVVC::nal_unit_vvc::parse_nal_unit_header(const QByteArray &parameterSetData, TreeItem *root)
{
  // Create a sub byte parser to access the bits
  reader_helper reader(parameterSetData, root, "nal_unit_header()");

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
  READBITS_M(nal_unit_type_id, 5, nal_unit_type_id_meaning);
  READBITS(nuh_temporal_id_plus1, 3);

  return true;
}
