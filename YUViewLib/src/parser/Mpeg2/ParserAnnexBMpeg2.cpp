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

#include "ParserAnnexBMpeg2.h"

#include "parser/common/parserMacros.h"
#include "parser/common/ReaderHelper.h"

#include "Extensions.h"
#include "GroupOfPicturesHeader.h"
#include "UserData.h"

#define PARSER_MPEG2_DEBUG_OUTPUT 0
#if PARSER_MPEG2_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_MPEG2(msg) qDebug() << msg
#else
#define DEBUG_MPEG2(msg) ((void)0)
#endif

using namespace MPEG2;

ParserAnnexB::ParseResult ParserAnnexBMpeg2::parseAndAddNALUnit(int nalID, QByteArray data, std::optional<BitratePlotModel::BitrateEntry> bitrateEntry, std::optional<pairUint64> nalStartEndPosFile, TreeItem *parent)
{
  ParserAnnexB::ParseResult parseResult;

  // Skip the NAL unit header
  int skip = 0;
  if (data.at(0) == (char)0 && data.at(1) == (char)0 && data.at(2) == (char)1)
    skip = 3;
  else if (data.at(0) == (char)0 && data.at(1) == (char)0 && data.at(2) == (char)0 && data.at(3) == (char)1)
    skip = 4;
  else
  {
    // No NAL header found. Skip everything up to the first NAL unit header
    QByteArray startCode;
    startCode.append((char)0);
    startCode.append((char)0);
    startCode.append((char)1);
    skip = data.indexOf(startCode);
    if (skip >= 0)
      skip += 3;
    if (skip == -1)
      // No start code found in the whole data. Try to start reading with a header.
      skip = 0;
  }

  // Read one byte (the NAL header) (technically there is no NAL in mpeg2 but it works pretty similarly)
  QByteArray nalHeaderBytes = data.mid(skip, 1);
  QByteArray payload = data.mid(skip + 1);

  // Use the given tree item. If it is not set, use the nalUnitMode (if active). 
  // We don't set data (a name) for this item yet. 
  // We want to parse the item and then set a good description.
  QString specificDescription;
  TreeItem *nalRoot = nullptr;
  if (parent)
    nalRoot = new TreeItem(parent);
  else if (!packetModel->isNull())
    nalRoot = new TreeItem(packetModel->getRootItem());

  // Create a nal_unit and read the header
  NalUnit nalUnit(nalID, nalStartEndPosFile);
  if (!nalUnit.parseNalUnitHeader(nalHeaderBytes, nalRoot))
    return parseResult;

  bool parsingSuccess = true;
  bool currentSliceIntra = false;
  QString currentSliceType;
  if (nalUnit.nal_unit_type == NalUnitType::SEQUENCE_HEADER)
  {
    auto newSequenceHeader = QSharedPointer<SequenceHeader>(new SequenceHeader(nalUnit));
    parsingSuccess = newSequenceHeader->parse(payload, nalRoot);

    specificDescription = parsingSuccess ? " Sequence Header" : " Sequence Header (Error)";
    parseResult.nalTypeName = parsingSuccess ? " SeqHeader" : " SeqHeader(Err)";
    if (parsingSuccess && !firstSequenceExtension)
      this->firstSequenceHeader = newSequenceHeader;

    DEBUG_MPEG2("ParserAnnexBMpeg2::parseAndAddNALUnit Sequence header");
  }
  else if (nalUnit.nal_unit_type == NalUnitType::PICTURE)
  {
    auto newPictureHeader = QSharedPointer<PictureHeader>(new PictureHeader(nalUnit));
    parsingSuccess = newPictureHeader->parse(payload, nalRoot);
    if (parsingSuccess)
    {
      if (newPictureHeader->temporal_reference == 0)
      {
        if (lastFramePOC >= 0)
          pocOffset = lastFramePOC + 1;
      }
      curFramePOC = pocOffset + newPictureHeader->temporal_reference;
      currentSliceIntra = newPictureHeader->isIntraPicture();
      lastPictureHeader = newPictureHeader;
      currentSliceType = newPictureHeader->getPictureTypeString();
      
      DEBUG_MPEG2("ParserAnnexBMpeg2::parseAndAddNALUnit Picture");
    }

    specificDescription = parsingSuccess ? " Picture Header" : " Picture Header (Error)";
    parseResult.nalTypeName = parsingSuccess ? " PicHeader" : " picHeader(Err)";
  }
  else if (nalUnit.nal_unit_type == NalUnitType::GROUP_START)
  {
    auto newGroupOfPicturesHeader = QSharedPointer<GroupOfPicturesHeader>(new GroupOfPicturesHeader(nalUnit));
    parsingSuccess = newGroupOfPicturesHeader->parse(payload, nalRoot);

    specificDescription = parsingSuccess ? " Group of Pictures" : " Group of Pictures (Error)";
    parseResult.nalTypeName = parsingSuccess ? " PicGroup" : " PicGroup(Err)";

    DEBUG_MPEG2("ParserAnnexBMpeg2::parseAndAddNALUnit Group Start");
  }
  else if (nalUnit.nal_unit_type == NalUnitType::USER_DATA)
  {
    auto newUserData = QSharedPointer<UserData>(new UserData(nalUnit));
    parsingSuccess = newUserData->parse(payload, nalRoot);

    specificDescription = parsingSuccess ? " User Data" : " User Data (Error)";
    parseResult.nalTypeName = parsingSuccess ? " UserData" : " UserData(Err)";

    DEBUG_MPEG2("ParserAnnexBMpeg2::parseAndAddNALUnit User Data");
  }
  else if (nalUnit.nal_unit_type == NalUnitType::EXTENSION_START)
  {
    TreeItem *const message_tree = nalRoot ? new TreeItem("", nalRoot) : nullptr;

    auto newExtension = QSharedPointer<NalExtension>(new NalExtension(nalUnit));
    if (!newExtension->parseStartCode(payload, message_tree))
      return parseResult;

    if (message_tree)
      message_tree->itemData[0] = newExtension->getExtensionFunctionName();

    if (newExtension->extensionType == ExtensionType::EXT_SEQUENCE)
    {
      auto newSequenceExtension = QSharedPointer<SequenceExtension>(new SequenceExtension(newExtension));
      parsingSuccess = newSequenceExtension->parse(payload, message_tree);

      specificDescription = parsingSuccess ? " Sequence Extension": " Sequence Extension (Error)";
      parseResult.nalTypeName = parsingSuccess ? " SeqExt" : " SeqExt(Err)";
      
      if (!this->firstSequenceExtension)
        this->firstSequenceExtension = newSequenceExtension;

      DEBUG_MPEG2("ParserAnnexBMpeg2::parseAndAddNALUnit Extension Start");
    }
    else if (newExtension->extensionType == ExtensionType::EXT_PICTURE_CODING)
    {
      auto newPictureCodingExtension = QSharedPointer<PictureCodingExtension>(new PictureCodingExtension(newExtension));
      parsingSuccess = newPictureCodingExtension->parse(payload, message_tree);

      specificDescription = parsingSuccess ? " Picture Coding Extension" : " Picture Coding Extension (Error)";
      parseResult.nalTypeName = parsingSuccess ? " PicCodExt" : " PicCodExt(Err)";

      DEBUG_MPEG2("ParserAnnexBMpeg2::parseAndAddNALUnit Picture Coding Extension");
    }
    else
    {
      parseResult.nalTypeName = QString(" Ext");
      specificDescription = QString(" Extension");
    }
  }

  const bool isStartOfNewAU = (nalUnit.nal_unit_type == NalUnitType::SEQUENCE_HEADER || (nalUnit.nal_unit_type == NalUnitType::PICTURE && !lastAUStartBySequenceHeader));
  if (isStartOfNewAU && lastFramePOC >= 0)
  {
    DEBUG_MPEG2("Start of new AU. Adding bitrate " << sizeCurrentAU << " for last AU (#" << counterAU << ").");

    BitratePlotModel::BitrateEntry entry;
    if (bitrateEntry)
    {
      entry.pts = bitrateEntry->pts;
      entry.dts = bitrateEntry->dts;
      entry.duration = bitrateEntry->duration;
    }
    else
    {
      entry.pts = lastFramePOC;
      entry.dts = counterAU;
      entry.duration = 1;
    }
    entry.bitrate = sizeCurrentAU;
    entry.keyframe = currentAUAllSlicesIntra;
    entry.frameType = ParserBase::convertSliceTypeMapToString(this->currentAUSliceTypes);
    parseResult.bitrateEntry = entry;

    sizeCurrentAU = 0;
    counterAU++;
    currentAUAllSlicesIntra = true;
    this->currentAUSliceTypes.clear();
  }
  if (lastFramePOC != curFramePOC)
    lastFramePOC = curFramePOC;
  sizeCurrentAU += data.size();
  if (nalUnit.nal_unit_type == NalUnitType::PICTURE && lastAUStartBySequenceHeader)
    lastAUStartBySequenceHeader = false;
  if (nalUnit.nal_unit_type == NalUnitType::SEQUENCE_HEADER)
    lastAUStartBySequenceHeader = true;

  if (nalUnit.nal_unit_type == NalUnitType::PICTURE)
  {
    if (!currentSliceIntra)
      currentAUAllSlicesIntra = false;
    this->currentAUSliceTypes[currentSliceType]++;
  }
  
  if (nalRoot)
    // Set a useful name of the TreeItem (the root for this NAL)
    nalRoot->itemData.append(QString("NAL %1: %2").arg(nalUnit.nal_idx).arg(nalUnitTypeToString(nalUnit.nal_unit_type)) + specificDescription);

  parseResult.success = true;
  return parseResult;
}

QPair<int,int> ParserAnnexBMpeg2::getProfileLevel()
{
  if (this->firstSequenceExtension)
    return QPair<int,int>(this->firstSequenceExtension->profile_identification, this->firstSequenceExtension->level_identification);
  return QPair<int,int>(0,0);
}

double ParserAnnexBMpeg2::getFramerate() const
{
  double frame_rate = 0.0;
  if (this->firstSequenceHeader && this->firstSequenceHeader->frame_rate_code > 0 && this->firstSequenceHeader->frame_rate_code <= 8)
  {
    QList<double> frame_rates = QList<double>() << 0.0 << 24000/1001 << 24 << 25 << 30000/1001 << 30 << 50 << 60000/1001 << 60;
    frame_rate = frame_rates[this->firstSequenceHeader->frame_rate_code];

    if (this->firstSequenceExtension)
    {
      frame_rate *= (this->firstSequenceExtension->frame_rate_extension_n + 1) / (this->firstSequenceExtension->frame_rate_extension_d + 1);
    }
  }

  return frame_rate;
}

QSize ParserAnnexBMpeg2::getSequenceSizeSamples() const
{
  int w = 0, h = 0;
  if (this->firstSequenceHeader)
  {
    w = this->firstSequenceHeader->horizontal_size_value;
    h = this->firstSequenceHeader->vertical_size_value;

    if (this->firstSequenceExtension)
    {
      w += this->firstSequenceExtension->horizontal_size_extension << 12;
      h += this->firstSequenceExtension->vertical_size_extension << 12;
    }
  }
  return QSize(w, h);
}

yuvPixelFormat ParserAnnexBMpeg2::getPixelFormat() const
{
  if (this->firstSequenceExtension)
  {
    int c = this->firstSequenceExtension->chroma_format;
    if (c == 1)
      return yuvPixelFormat(Subsampling::YUV_420, 8);
    if (c == 2)
      return yuvPixelFormat(Subsampling::YUV_422, 8);
    if (c == 3)
      return yuvPixelFormat(Subsampling::YUV_444, 8);
  }
  return yuvPixelFormat();
}

QPair<int,int> ParserAnnexBMpeg2::getSampleAspectRatio()
{
  if (this->firstSequenceHeader)
  {
    const int ratio = this->firstSequenceHeader->aspect_ratio_information;
    if (ratio == 2)
      return QPair<int,int>(3, 4);
    if (ratio == 3)
      return QPair<int,int>(9, 16);
    if (ratio == 4)
      return QPair<int,int>(100, 221);
    if (ratio == 2)
      return QPair<int,int>(3, 4);
  }
  return QPair<int,int>(1, 1);
}