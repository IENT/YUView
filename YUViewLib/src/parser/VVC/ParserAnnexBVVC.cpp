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

#include "parser/common/parserMacros.h"
#include "parser/common/ReaderHelper.h"
#include "PictureHeader.h"

#define PARSER_VVC_DEBUG_OUTPUT 0
#if PARSER_VVC_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_VVC(msg) qDebug() << msg
#else
#define DEBUG_VVC(msg) ((void)0)
#endif

using namespace VVC;

double ParserAnnexBVVC::getFramerate() const
{
  return DEFAULT_FRAMERATE;
}

QSize ParserAnnexBVVC::getSequenceSizeSamples() const
{
  return {};
}

yuvPixelFormat ParserAnnexBVVC::getPixelFormat() const
{
  // Return invalid format. It will be updated once the first
  // frame was decoded. However, this should be correct once we add VVC bitstream parsing.
  return {};
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

QPair<int,int> ParserAnnexBVVC::getSampleAspectRatio()
{
  return QPair<int,int>(1,1);
}

ParserAnnexB::ParseResult ParserAnnexBVVC::parseAndAddNALUnit(int nalID, QByteArray data, std::optional<BitratePlotModel::BitrateEntry> bitrateEntry, std::optional<pairUint64> nalStartEndPosFile, TreeItem *parent)
{
  ParserAnnexB::ParseResult parseResult;

  if (nalID == -1 && data.isEmpty())
  {
    if (!addFrameToList(this->counterAU, this->curFrameFileStartEndPos, false))
    {
      ReaderHelper::addErrorMessageChildItem(QString("Error adding frame to frame list."), parent);
      return parseResult;
    }
    return parseResult;
  }

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
  else if (!this->packetModel->isNull())
    nalRoot = new TreeItem(this->packetModel->getRootItem());

  // Create a nal_unit and read the header
  VVC::NalUnit nal(nalID, nalStartEndPosFile);
  if (!nal.parse_nal_unit_header(nalHeaderBytes, nalRoot))
    return parseResult;

  bool parsingSuccess = true;
  QSharedPointer<NalUnit> parsedNal;
  if (nal.nal_unit_type == SPS_NUT)
  {
    auto newSPS = QSharedPointer<SPS>(new SPS(nal));
    parsingSuccess = newSPS->parse(payload, nalRoot);

    this->activeSPSMap.insert(newSPS->sps_seq_parameter_set_id, newSPS); // Replace if it exists
    this->nalUnitList.append(newSPS);
    parsedNal = newSPS;

    // Add the SPS ID
    specificDescription = parsingSuccess ? QString(" SPS_NUT ID %1").arg(newSPS->sps_seq_parameter_set_id) : " SPS_NUT ERR";
    parseResult.nalTypeName = parsingSuccess ? QString("SPS(%1)").arg(newSPS->sps_seq_parameter_set_id) : "SPS(ERR)";

    DEBUG_VVC("ParserAnnexBVVC::parseAndAddNALUnit SPS ID " << newSPS->sps_seq_parameter_set_id);
  }
  else if (nal.nal_unit_type == PPS_NUT)
  {
    auto newPPS = QSharedPointer<PPS>(new PPS(nal));
    parsingSuccess = newPPS->parse(payload, nalRoot);

    this->activePPSMap.insert(newPPS->pps_pic_parameter_set_id, newPPS); // Replace if it exists
    this->nalUnitList.append(newPPS);
    parsedNal = newPPS;

    // Add the SPS ID
    specificDescription = parsingSuccess ? QString(" SPS_NUT ID %1").arg(newPPS->pps_pic_parameter_set_id) : " PPS_NUT ERR";
    parseResult.nalTypeName = parsingSuccess ? QString("SPS(%1)").arg(newPPS->pps_pic_parameter_set_id) : "PPS(ERR)";

    DEBUG_VVC("ParserAnnexBVVC::parseAndAddNALUnit SPS ID " << newPPS->pps_pic_parameter_set_id);
  }
  else if (nal.isSlice())
  {
    auto newPictureHeader = QSharedPointer<PictureHeader>(new PictureHeader(nal));
    parsingSuccess = newPictureHeader->parse(payload, this->activeSPSMap, this->activePPSMap, false, nalRoot);

    parsedNal = newPictureHeader;

    // Calculate and add POC
    

  }

  if (auDelimiterDetector.isStartOfNewAU(nal, parsedNal))
  {
    DEBUG_VVC("Start of new AU. Adding bitrate " << this->sizeCurrentAU);
    
    BitratePlotModel::BitrateEntry entry;
    entry.pts = this->counterAU;
    entry.dts = this->counterAU;  // TODO: Not true. We need to parse the VVC header data
    entry.bitrate = this->sizeCurrentAU;
    entry.keyframe = false; // TODO: Also not correct. We need parsing.
    parseResult.bitrateEntry = entry;

    if (this->counterAU > 0)
    {
      const bool curFrameIsRandomAccess = (this->counterAU == 1);
      if (!addFrameToList(this->counterAU, this->curFrameFileStartEndPos, curFrameIsRandomAccess))
      {
        ReaderHelper::addErrorMessageChildItem(QString("Error adding frame to frame list."), parent);
        return parseResult;
      }
      DEBUG_VVC("Adding start/end " << this->curFrameFileStartEndPos << " - POC " << this->counterAU << (curFrameIsRandomAccess ? " - ra" : ""));
    }
    this->curFrameFileStartEndPos = nalStartEndPosFile;
    this->sizeCurrentAU = 0;
    this->counterAU++;
  }
  else if (this->curFrameFileStartEndPos && nalStartEndPosFile)
    // Another slice NAL which belongs to the last frame
    // Update the end position
    this->curFrameFileStartEndPos->second = nalStartEndPosFile->second;

  this->sizeCurrentAU += data.size();

  if (nalRoot)
    // Set a useful name of the TreeItem (the root for this NAL)
    nalRoot->itemData.append(QString("NAL %1: %2").arg(nal.nal_idx).arg(nal.nal_unit_type_id) + specificDescription);

  parseResult.success = true;
  return parseResult;
}

bool ParserAnnexBVVC::auDelimiterDetector_t::isStartOfNewAU(NalUnit &nal, QSharedPointer<NalUnit> parsedNal)
{
  bool isStart = false;
  if (this->primaryCodedPictureInAuEncountered)
  {
    static const auto startAUTypes = QList<NalUnitType>() << AUD_NUT << DCI_NUT << VPS_NUT << SPS_NUT << PPS_NUT << PREFIX_APS_NUT << PH_NUT << PREFIX_SEI_NUT << RSV_NVCL_26 << UNSPEC_28 << UNSPEC_29;

    if (nal.nuh_layer_id == 0 && startAUTypes.contains(nal.nal_unit_type))
      isStart = true;

    if (nal.isSlice())
    {
      // ...
    }
    isStart = true;
  }

  if (nal.isSlice())
    this->primaryCodedPictureInAuEncountered = true;

  if (isStart && !nal.isSlice())
    primaryCodedPictureInAuEncountered = false;

  return isStart;
}