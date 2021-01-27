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

#include "AnnexBMpeg2.h"

#include "NalUnitMpeg2.h"
#include "group_of_pictures_header.h"
#include "nal_extension.h"
#include "parser/common/Macros.h"
#include "parser/common/functions.h"
#include "picture_header.h"
#include "sequence_extension.h"
#include "sequence_header.h"
#include "user_data.h"

#include <algorithm>

#define PARSER_MPEG2_DEBUG_OUTPUT 0
#if PARSER_MPEG2_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_MPEG2(msg) qDebug() << msg
#else
#define DEBUG_MPEG2(msg) ((void)0)
#endif

namespace parser
{

using namespace mpeg2;

AnnexB::ParseResult
AnnexBMpeg2::parseAndAddNALUnit(int                                           nalID,
                                const ByteVector &                            data,
                                std::optional<BitratePlotModel::BitrateEntry> bitrateEntry,
                                std::optional<pairUint64>                     nalStartEndPosFile,
                                TreeItem *                                    parent)
{
  AnnexB::ParseResult parseResult;

  // Skip the NAL unit header
  unsigned readOffset = 0;
  if (data.at(0) == (char)0 && data.at(1) == (char)0 && data.at(2) == (char)1)
    readOffset = 3;
  else if (data.at(0) == (char)0 && data.at(1) == (char)0 && data.at(2) == (char)0 &&
           data.at(3) == (char)1)
    readOffset = 4;
  else
  {
    ByteVector startCode({char(0), char(0), char(1)});
    auto itStartCode = std::search(data.begin(), data.end(), startCode.begin(), startCode.end());
    if (itStartCode == data.end())
      // No start code found in the whole data. Try to start reading with a header.
      readOffset = 0;
    else
      readOffset = unsigned(itStartCode - data.begin()) + 3;
  }

  // Use the given tree item. If it is not set, use the nalUnitMode (if active).
  // We don't set data (a name) for this item yet.
  // We want to parse the item and then set a good description.
  std::string specificDescription;
  TreeItem *  nalRoot = nullptr;
  if (parent)
    nalRoot = new TreeItem(parent);
  else if (!packetModel->isNull())
    nalRoot = new TreeItem(packetModel->getRootItem());

  reader::SubByteReaderLogging reader(data, nalRoot, "", readOffset);

  AnnexB::logNALSize(data, nalRoot, nalStartEndPosFile);

  // Create a nal_unit and read the header
  NalUnitMpeg2 nal_mpeg2(nalID, nalStartEndPosFile);
  nal_mpeg2.header.parse(reader);

  bool        currentSliceIntra = false;
  std::string currentSliceType;
  if (nal_mpeg2.header.nal_unit_type == NalType::SEQUENCE_HEADER)
  {
    DEBUG_MPEG2("AnnexBMpeg2::parseAndAddNALUnit Sequence header");

    auto newSequenceHeader = std::make_shared<sequence_header>();
    newSequenceHeader->parse(reader);

    if (!this->firstSequenceHeader)
      this->firstSequenceHeader = newSequenceHeader;

    nal_mpeg2.rbsp          = newSequenceHeader;
    specificDescription     = " Sequence Header";
    parseResult.nalTypeName = "SeqHeader";
  }
  else if (nal_mpeg2.header.nal_unit_type == NalType::PICTURE)
  {
    DEBUG_MPEG2("AnnexBMpeg2::parseAndAddNALUnit Picture Header");

    auto newPictureHeader = std::make_shared<picture_header>();
    newPictureHeader->parse(reader);

    if (newPictureHeader->temporal_reference == 0)
    {
      if (lastFramePOC >= 0)
        this->pocOffset = this->lastFramePOC + 1;
    }
    this->curFramePOC       = this->pocOffset + newPictureHeader->temporal_reference;
    currentSliceIntra       = newPictureHeader->isIntraPicture();
    this->lastPictureHeader = newPictureHeader;
    currentSliceType        = newPictureHeader->getPictureTypeString();

    nal_mpeg2.rbsp          = newPictureHeader;
    specificDescription     = " Picture Header POC " + std::to_string(this->curFramePOC);
    parseResult.nalTypeName = "PicHeader(POC " + std::to_string(this->curFramePOC) + ")";
  }
  else if (nal_mpeg2.header.nal_unit_type == NalType::GROUP_START)
  {
    DEBUG_MPEG2("AnnexBMpeg2::parseAndAddNALUnit Group Start");

    auto newGroupOfPictureHeader = std::make_shared<group_of_pictures_header>();
    newGroupOfPictureHeader->parse(reader);

    nal_mpeg2.rbsp          = newGroupOfPictureHeader;
    specificDescription     = " Group of Pictures";
    parseResult.nalTypeName = "GOP";
  }
  else if (nal_mpeg2.header.nal_unit_type == NalType::USER_DATA)
  {
    DEBUG_MPEG2("AnnexBMpeg2::parseAndAddNALUnit User Data");

    auto newUserData = std::make_shared<user_data>();
    newUserData->parse(reader);

    nal_mpeg2.rbsp          = newUserData;
    specificDescription     = " User Data";
    parseResult.nalTypeName = "UserData";
  }
  else if (nal_mpeg2.header.nal_unit_type == NalType::EXTENSION_START)
  {
    DEBUG_MPEG2("AnnexBMpeg2::parseAndAddNALUnit Extension Start");

    auto newExtension = std::make_shared<nal_extension>();
    newExtension->parse(reader);

    if (newExtension->extension_type == ExtensionType::EXT_SEQUENCE)
    {
      this->firstSequenceExtension =
          std::dynamic_pointer_cast<sequence_extension>(newExtension->payload);
    }

    nal_mpeg2.rbsp          = newExtension;
    specificDescription     = " Extension";
    parseResult.nalTypeName = "Extension";
  }

  const bool isStartOfNewAU =
      (nal_mpeg2.header.nal_unit_type == NalType::SEQUENCE_HEADER ||
       (nal_mpeg2.header.nal_unit_type == NalType::PICTURE && !lastAUStartBySequenceHeader));
  if (isStartOfNewAU && lastFramePOC >= 0)
  {
    DEBUG_MPEG2("Start of new AU. Adding bitrate " << sizeCurrentAU << " for last AU (#"
                                                   << counterAU << ").");

    BitratePlotModel::BitrateEntry entry;
    if (bitrateEntry)
    {
      entry.pts      = bitrateEntry->pts;
      entry.dts      = bitrateEntry->dts;
      entry.duration = bitrateEntry->duration;
    }
    else
    {
      entry.pts      = lastFramePOC;
      entry.dts      = counterAU;
      entry.duration = 1;
    }
    entry.bitrate  = sizeCurrentAU;
    entry.keyframe = currentAUAllSlicesIntra;
    entry.frameType =
        QString::fromStdString(convertSliceCountsToString(this->currentAUSliceCounts));
    parseResult.bitrateEntry = entry;

    sizeCurrentAU = 0;
    counterAU++;
    currentAUAllSlicesIntra = true;
    this->currentAUSliceCounts.clear();
  }
  if (lastFramePOC != curFramePOC)
    lastFramePOC = curFramePOC;
  sizeCurrentAU += unsigned(data.size());
  if (nal_mpeg2.header.nal_unit_type == NalType::PICTURE && lastAUStartBySequenceHeader)
    lastAUStartBySequenceHeader = false;
  if (nal_mpeg2.header.nal_unit_type == NalType::SEQUENCE_HEADER)
    lastAUStartBySequenceHeader = true;

  if (nal_mpeg2.header.nal_unit_type == NalType::PICTURE)
  {
    if (!currentSliceIntra)
      currentAUAllSlicesIntra = false;
    this->currentAUSliceCounts[currentSliceType]++;
  }

  if (nalRoot)
  {
    auto name = "NAL " + std::to_string(nal_mpeg2.nalIdx) + ": " +
                nalTypeCoding.getMeaning(nal_mpeg2.header.nal_unit_type) + specificDescription;
    nalRoot->setProperties(name);
  }

  parseResult.success = true;
  return parseResult;
}

IntPair AnnexBMpeg2::getProfileLevel()
{
  if (firstSequenceExtension)
    return {firstSequenceExtension->profile_identification,
            firstSequenceExtension->level_identification};
  return {};
}

double AnnexBMpeg2::getFramerate() const
{
  double frame_rate = 0.0;
  if (firstSequenceHeader && firstSequenceHeader->frame_rate_code > 0 &&
      firstSequenceHeader->frame_rate_code <= 8)
  {
    auto frame_rates =
        std::vector<double>({0.0, 24000 / 1001, 24, 25, 30000 / 1001, 30, 50, 60000 / 1001, 60});
    frame_rate = frame_rates[firstSequenceHeader->frame_rate_code];

    if (firstSequenceExtension)
    {
      frame_rate *= (firstSequenceExtension->frame_rate_extension_n + 1) /
                    (firstSequenceExtension->frame_rate_extension_d + 1);
    }
  }

  return frame_rate;
}

QSize AnnexBMpeg2::getSequenceSizeSamples() const
{
  int w = 0, h = 0;
  if (firstSequenceHeader)
  {
    w = firstSequenceHeader->horizontal_size_value;
    h = firstSequenceHeader->vertical_size_value;

    if (firstSequenceExtension)
    {
      w += firstSequenceExtension->horizontal_size_extension << 12;
      h += firstSequenceExtension->vertical_size_extension << 12;
    }
  }
  return QSize(w, h);
}

yuvPixelFormat AnnexBMpeg2::getPixelFormat() const
{
  if (firstSequenceExtension)
  {
    int c = firstSequenceExtension->chroma_format;
    if (c == 1)
      return yuvPixelFormat(Subsampling::YUV_420, 8);
    if (c == 2)
      return yuvPixelFormat(Subsampling::YUV_422, 8);
    if (c == 3)
      return yuvPixelFormat(Subsampling::YUV_444, 8);
  }
  return yuvPixelFormat();
}

Ratio AnnexBMpeg2::getSampleAspectRatio()
{
  if (firstSequenceHeader)
  {
    const int ratio = firstSequenceHeader->aspect_ratio_information;
    if (ratio == 2)
      return Ratio({3, 4});
    if (ratio == 3)
      return Ratio({9, 16});
    if (ratio == 4)
      return Ratio({100, 221});
    if (ratio == 2)
      return Ratio({3, 4});
  }
  return Ratio({1, 1});
}

} // namespace parser