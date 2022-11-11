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

#include "AnnexBVVC.h"

#include <algorithm>
#include <cmath>

#include "SEI/buffering_period.h"
#include "SEI/sei_message.h"
#include "access_unit_delimiter_rbsp.h"
#include "adaptation_parameter_set_rbsp.h"
#include "decoding_capability_information_rbsp.h"
#include "end_of_bitstream_rbsp.h"
#include "end_of_seq_rbsp.h"
#include "filler_data_rbsp.h"
#include "nal_unit_header.h"
#include "operating_point_information_rbsp.h"
#include "pic_parameter_set_rbsp.h"
#include "picture_header_rbsp.h"
#include "seq_parameter_set_rbsp.h"
#include "slice_layer_rbsp.h"
#include "video_parameter_set_rbsp.h"

#define PARSER_VVC_DEBUG_OUTPUT 0
#if PARSER_VVC_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_VVC(msg) qDebug() << msg
#else
#define DEBUG_VVC(msg) ((void)0)
#endif

namespace parser
{

using namespace parser::vvc;

namespace
{

BitratePlotModel::BitrateEntry
createBitrateEntryForAU(ParsingState &                                parsingState,
                        std::optional<BitratePlotModel::BitrateEntry> bitrateEntry)
{
  BitratePlotModel::BitrateEntry entry;
  if (bitrateEntry)
  {
    entry.pts      = bitrateEntry->pts;
    entry.dts      = bitrateEntry->dts;
    entry.duration = bitrateEntry->duration;
  }
  else
  {
    entry.pts      = parsingState.currentAU.poc;
    entry.dts      = int(parsingState.currentAU.counter);
    entry.duration = 1;
  }
  entry.bitrate  = unsigned(parsingState.currentAU.sizeBytes);
  entry.keyframe = parsingState.currentAU.isKeyframe;
  return entry;
}

} // namespace

double AnnexBVVC::getFramerate() const
{
  // Parsing of VUI not implemented yet
  return DEFAULT_FRAMERATE;
}

Size AnnexBVVC::getSequenceSizeSamples() const
{
  for (const auto &nal : this->nalUnitsForSeeking)
  {
    if (nal->header.nal_unit_type == vvc::NalType::SPS_NUT)
    {
      auto sps = std::dynamic_pointer_cast<seq_parameter_set_rbsp>(nal->rbsp);
      return Size(sps->get_max_width_cropping(), sps->get_max_height_cropping());
    }
  }

  return {};
}

video::yuv::PixelFormatYUV AnnexBVVC::getPixelFormat() const
{
  using Subsampling = video::yuv::Subsampling;

  // Get the subsampling and bit-depth from the sps
  int  bitDepthY   = -1;
  int  bitDepthC   = -1;
  auto subsampling = Subsampling::UNKNOWN;
  for (const auto &nal : this->nalUnitsForSeeking)
  {
    if (nal->header.nal_unit_type == vvc::NalType::SPS_NUT)
    {
      auto sps = std::dynamic_pointer_cast<seq_parameter_set_rbsp>(nal->rbsp);
      if (sps->sps_chroma_format_idc == 0)
        subsampling = Subsampling::YUV_400;
      else if (sps->sps_chroma_format_idc == 1)
        subsampling = Subsampling::YUV_420;
      else if (sps->sps_chroma_format_idc == 2)
        subsampling = Subsampling::YUV_422;
      else if (sps->sps_chroma_format_idc == 3)
        subsampling = Subsampling::YUV_444;

      bitDepthY = sps->sps_bitdepth_minus8 + 8;
      bitDepthC = sps->sps_bitdepth_minus8 + 8;
    }

    if (bitDepthY != -1 && bitDepthC != -1 && subsampling != Subsampling::UNKNOWN)
    {
      if (bitDepthY != bitDepthC)
      {
        // Different luma and chroma bit depths currently not supported
        return {};
      }
      return video::yuv::PixelFormatYUV(subsampling, bitDepthY);
    }
  }

  return {};
}

std::optional<AnnexB::SeekData> AnnexBVVC::getSeekData(int iFrameNr)
{
  if (iFrameNr >= int(this->getNumberPOCs()) || iFrameNr < 0)
    return {};

  auto seekPOC = this->getFramePOC(unsigned(iFrameNr));

  // Collect the active parameter sets
  using NalMap = std::map<unsigned, std::shared_ptr<NalUnitVVC>>;
  NalMap activeVPSNal;
  NalMap activeSPSNal;
  NalMap activePPSNal;
  using ApsMap = std::map<std::pair<unsigned, unsigned>, std::shared_ptr<NalUnitVVC>>;
  ApsMap activeAPSNal;

  for (const auto &nal : this->nalUnitsForSeeking)
  {
    if (nal->header.isSlice())
    {
      auto slice = std::dynamic_pointer_cast<slice_layer_rbsp>(nal->rbsp);

      auto picHeader = slice->slice_header_instance.picture_header_structure_instance;
      if (!picHeader)
      {
        DEBUG_VVC("Error - Slice has no picture header");
        continue;
      }

      if (seekPOC >= 0 && picHeader->globalPOC == unsigned(seekPOC))
      {
        // Seek here
        AnnexB::SeekData seekData;
        if (nal->filePosStartEnd)
          seekData.filePos = nal->filePosStartEnd->first;

        for (const auto &nalMap : {activeVPSNal, activeSPSNal, activePPSNal})
        {
          for (const auto &entry : nalMap)
            seekData.parameterSets.push_back(entry.second->rawData);
        }
        for (const auto &aps : activeAPSNal)
          seekData.parameterSets.push_back(aps.second->rawData);

        return seekData;
      }
    }
    if (nal->header.nal_unit_type == NalType::VPS_NUT)
    {
      auto vps = std::dynamic_pointer_cast<video_parameter_set_rbsp>(nal->rbsp);
      activeVPSNal[vps->vps_video_parameter_set_id] = nal;
    }
    if (nal->header.nal_unit_type == NalType::SPS_NUT)
    {
      auto sps = std::dynamic_pointer_cast<seq_parameter_set_rbsp>(nal->rbsp);
      activeSPSNal[sps->sps_seq_parameter_set_id] = nal;
    }
    if (nal->header.nal_unit_type == NalType::PPS_NUT)
    {
      auto pps = std::dynamic_pointer_cast<pic_parameter_set_rbsp>(nal->rbsp);
      activePPSNal[pps->pps_pic_parameter_set_id] = nal;
    }
    if (nal->header.nal_unit_type == NalType::PREFIX_APS_NUT ||
        nal->header.nal_unit_type == NalType::SUFFIX_APS_NUT)
    {
      auto aps     = std::dynamic_pointer_cast<adaptation_parameter_set_rbsp>(nal->rbsp);
      auto apsType = apsParamTypeMapper.indexOf(aps->aps_params_type);
      activeAPSNal[{apsType, aps->aps_adaptation_parameter_set_id}] = nal;
    }
  }

  return {};
}

QByteArray AnnexBVVC::getExtradata()
{
  return {};
}

IntPair AnnexBVVC::getProfileLevel()
{
  for (const auto &nal : this->nalUnitsForSeeking)
  {
    if (nal->header.nal_unit_type == vvc::NalType::SPS_NUT)
    {
      auto sps = std::dynamic_pointer_cast<seq_parameter_set_rbsp>(nal->rbsp);
      return {sps->profile_tier_level_instance.general_profile_idc,
              sps->profile_tier_level_instance.general_level_idc};
    }
  }
  return {};
}

Ratio AnnexBVVC::getSampleAspectRatio()
{
  for (const auto &nal : this->nalUnitsForSeeking)
  {
    if (nal->header.nal_unit_type == vvc::NalType::SPS_NUT)
    {
      auto sps = std::dynamic_pointer_cast<seq_parameter_set_rbsp>(nal->rbsp);
      if (sps->sps_vui_parameters_present_flag)
      {
        auto vui = sps->vui_payload_instance.vui;
        if (vui.vui_aspect_ratio_info_present_flag)
        {
          if (vui.vui_aspect_ratio_idc == 255)
            return {int(vui.vui_sar_height), int(vui.vui_sar_width)};
          else
            return sampleAspectRatioCoding.getValue(vui.vui_aspect_ratio_idc);
        }
      }
    }
  }
  return Ratio({1, 1});
}

AnnexB::ParseResult
AnnexBVVC::parseAndAddUnit(int                                           nalID,
                           const ByteVector &                            data,
                           std::optional<BitratePlotModel::BitrateEntry> bitrateEntry,
                           std::optional<FileStartEndPos>                nalStartEndPosFile,
                           std::shared_ptr<TreeItem>                     parent)
{
  AnnexB::ParseResult parseResult;
  parseResult.success = true;

  if (nalID == -1 && data.empty())
  {
    if (this->parsingState.currentAU.poc != -1)
    {
      parseResult.bitrateEntry = createBitrateEntryForAU(this->parsingState, bitrateEntry);
      if (!this->handleNewAU(this->parsingState))
      {
        DEBUG_VVC("Error handling last AU");
        parseResult.success = false;
      }
    }
    return parseResult;
  }

  // Skip the NAL unit header
  int readOffset = 0;
  if (data.at(0) == (char)0 && data.at(1) == (char)0 && data.at(2) == (char)1)
    readOffset = 3;
  else if (data.at(0) == (char)0 && data.at(1) == (char)0 && data.at(2) == (char)0 &&
           data.at(3) == (char)1)
    readOffset = 4;

  // Use the given tree item. If it is not set, use the nalUnitMode (if active).
  // Create a new TreeItem root for the NAL unit. We don't set data (a name) for this item
  // yet. We want to parse the item and then set a good description.
  std::shared_ptr<TreeItem> nalRoot;
  if (parent)
    nalRoot = parent->createChildItem();
  else if (packetModel->rootItem)
    nalRoot = packetModel->rootItem->createChildItem();

  if (nalRoot)
    AnnexB::logNALSize(data, nalRoot, nalStartEndPosFile);

  reader::SubByteReaderLogging reader(data, nalRoot, "", readOffset);

  std::string specificDescription;
  auto        nalVVC              = std::make_shared<vvc::NalUnitVVC>(nalID, nalStartEndPosFile);
  auto        updatedParsingState = this->parsingState;
  try
  {
    nalVVC->header.parse(reader);

    auto nalType        = nalVVC->header.nal_unit_type;
    specificDescription = " " + NalTypeMapper.getName(nalType);

    if (updatedParsingState.NoOutputBeforeRecoveryFlag.count(nalVVC->header.nuh_layer_id) == 0)
      updatedParsingState.NoOutputBeforeRecoveryFlag[nalVVC->header.nuh_layer_id] = true;

    if (nalType == NalType::VPS_NUT)
    {
      auto newVPS = std::make_shared<video_parameter_set_rbsp>();
      newVPS->parse(reader);

      this->activeParameterSets.vpsMap[newVPS->vps_video_parameter_set_id] = newVPS;

      specificDescription += " ID " + std::to_string(newVPS->vps_video_parameter_set_id);

      nalVVC->rbsp    = newVPS;
      nalVVC->rawData = data;
      this->nalUnitsForSeeking.push_back(nalVVC);
    }
    else if (nalType == NalType::SPS_NUT)
    {
      auto newSPS = std::make_shared<seq_parameter_set_rbsp>();
      newSPS->parse(reader);

      this->activeParameterSets.spsMap[newSPS->sps_seq_parameter_set_id] = newSPS;

      specificDescription += " ID " + std::to_string(newSPS->sps_seq_parameter_set_id);

      nalVVC->rbsp    = newSPS;
      nalVVC->rawData = data;
      this->nalUnitsForSeeking.push_back(nalVVC);
    }
    else if (nalType == NalType::PPS_NUT)
    {
      auto newPPS = std::make_shared<pic_parameter_set_rbsp>();
      newPPS->parse(reader, this->activeParameterSets.spsMap);

      this->activeParameterSets.ppsMap[newPPS->pps_pic_parameter_set_id] = newPPS;

      specificDescription += " ID " + std::to_string(newPPS->pps_pic_parameter_set_id);

      nalVVC->rbsp    = newPPS;
      nalVVC->rawData = data;
      this->nalUnitsForSeeking.push_back(nalVVC);
    }
    else if (nalType == NalType::PREFIX_APS_NUT || nalType == NalType::SUFFIX_APS_NUT)
    {
      auto newAPS = std::make_shared<adaptation_parameter_set_rbsp>();
      newAPS->parse(reader);

      auto apsType = apsParamTypeMapper.indexOf(newAPS->aps_params_type);
      this->activeParameterSets.apsMap[{apsType, newAPS->aps_adaptation_parameter_set_id}] = newAPS;

      specificDescription += " ID " + std::to_string(newAPS->aps_adaptation_parameter_set_id);

      nalVVC->rbsp    = newAPS;
      nalVVC->rawData = data;
      this->nalUnitsForSeeking.push_back(nalVVC);
    }
    else if (nalType == NalType::PH_NUT)
    {
      auto newPictureHeader = std::make_shared<picture_header_rbsp>();
      newPictureHeader->parse(reader,
                              this->activeParameterSets.vpsMap,
                              this->activeParameterSets.spsMap,
                              this->activeParameterSets.ppsMap,
                              updatedParsingState.currentSlice);
      auto &pictureHeader = newPictureHeader->picture_header_structure_instance;
      pictureHeader->calculatePictureOrderCount(
          reader,
          nalType,
          this->activeParameterSets.spsMap,
          this->activeParameterSets.ppsMap,
          updatedParsingState.prevTid0Pic[nalVVC->header.nuh_layer_id],
          updatedParsingState.NoOutputBeforeRecoveryFlag[nalVVC->header.nuh_layer_id]);

      updatedParsingState.NoOutputBeforeRecoveryFlag[nalVVC->header.nuh_layer_id] = false;

      pictureHeader->globalPOC =
          calculateAndUpdateGlobalPOC(isIRAP(nalType), pictureHeader->PicOrderCntVal);

      updatedParsingState.currentPictureHeaderStructure =
          newPictureHeader->picture_header_structure_instance;
      updatedParsingState.currentAU.poc = pictureHeader->globalPOC;

      // 8.3.1
      auto TemporalId = nalVVC->header.nuh_temporal_id_plus1 - 1;
      if (TemporalId == 0 && !pictureHeader->ph_non_ref_pic_flag && nalType != NalType::RASL_NUT &&
          nalType != NalType::RADL_NUT)
        updatedParsingState.prevTid0Pic[nalVVC->header.nuh_layer_id] = pictureHeader;

      specificDescription += " POC " + std::to_string(pictureHeader->PicOrderCntVal);

      nalVVC->rbsp = newPictureHeader;
    }
    else if (nalVVC->header.isSlice())
    {
      specificDescription += " (Slice Header)";
      auto newSliceLayer = std::make_shared<slice_layer_rbsp>();
      newSliceLayer->parse(reader,
                           nalType,
                           this->activeParameterSets.vpsMap,
                           this->activeParameterSets.spsMap,
                           this->activeParameterSets.ppsMap,
                           updatedParsingState.currentPictureHeaderStructure);

      updatedParsingState.currentSlice = newSliceLayer;
      if (newSliceLayer->slice_header_instance.picture_header_structure_instance)
      {
        newSliceLayer->slice_header_instance.picture_header_structure_instance
            ->calculatePictureOrderCount(
                reader,
                nalType,
                this->activeParameterSets.spsMap,
                this->activeParameterSets.ppsMap,
                updatedParsingState.prevTid0Pic[nalVVC->header.nuh_layer_id],
                updatedParsingState.NoOutputBeforeRecoveryFlag[nalVVC->header.nuh_layer_id]);

        updatedParsingState.NoOutputBeforeRecoveryFlag[nalVVC->header.nuh_layer_id] = false;

        newSliceLayer->slice_header_instance.picture_header_structure_instance->globalPOC =
            calculateAndUpdateGlobalPOC(isIRAP(nalType),
                                        newSliceLayer->slice_header_instance
                                            .picture_header_structure_instance->PicOrderCntVal);

        updatedParsingState.currentPictureHeaderStructure =
            newSliceLayer->slice_header_instance.picture_header_structure_instance;
        updatedParsingState.currentAU.poc =
            updatedParsingState.currentPictureHeaderStructure->globalPOC;

        // 8.3.1
        auto TemporalId = nalVVC->header.nuh_temporal_id_plus1 - 1;
        if (TemporalId == 0 &&
            !newSliceLayer->slice_header_instance.picture_header_structure_instance
                 ->ph_non_ref_pic_flag &&
            nalType != NalType::RASL_NUT && nalType != NalType::RADL_NUT)
          updatedParsingState.prevTid0Pic[nalVVC->header.nuh_layer_id] =
              newSliceLayer->slice_header_instance.picture_header_structure_instance;
      }
      else
      {
        if (!updatedParsingState.currentPictureHeaderStructure)
          throw std::logic_error("Slice must have a valid picture header");
        newSliceLayer->slice_header_instance.picture_header_structure_instance =
            updatedParsingState.currentPictureHeaderStructure;
      }

      specificDescription +=
          " POC " + std::to_string(updatedParsingState.currentPictureHeaderStructure->globalPOC);
      specificDescription +=
          " " + to_string(newSliceLayer->slice_header_instance.sh_slice_type) + "-Slice";

      nalVVC->rbsp = newSliceLayer;

      updatedParsingState.currentAU.isKeyframe =
          (nalType == NalType::IDR_W_RADL || nalType == NalType::IDR_N_LP ||
           nalType == NalType::CRA_NUT);
      if (updatedParsingState.currentAU.isKeyframe)
      {
        nalVVC->rawData = data;
        this->nalUnitsForSeeking.push_back(nalVVC);
      }
    }
    else if (nalType == NalType::AUD_NUT)
    {
      auto newAUD = std::make_shared<access_unit_delimiter_rbsp>();
      newAUD->parse(reader);
      nalVVC->rbsp = newAUD;
    }
    else if (nalType == NalType::DCI_NUT)
    {
      auto newDCI = std::make_shared<decoding_capability_information_rbsp>();
      newDCI->parse(reader);
      nalVVC->rbsp = newDCI;
    }
    else if (nalType == NalType::EOB_NUT)
    {
      auto newEOB  = std::make_shared<end_of_bitstream_rbsp>();
      nalVVC->rbsp = newEOB;
    }
    else if (nalType == NalType::EOS_NUT)
    {
      auto newEOS  = std::make_shared<end_of_seq_rbsp>();
      nalVVC->rbsp = newEOS;
    }
    else if (nalType == NalType::FD_NUT)
    {
      auto newFillerData = std::make_shared<filler_data_rbsp>();
      newFillerData->parse(reader);
      nalVVC->rbsp = newFillerData;
    }
    else if (nalType == NalType::OPI_NUT)
    {
      auto newOPI = std::make_shared<operating_point_information_rbsp>();
      newOPI->parse(reader);
      nalVVC->rbsp = newOPI;
    }
    else if (nalType == NalType::SUFFIX_SEI_NUT || nalType == NalType::PREFIX_APS_NUT)
    {
      auto newSEI = std::make_shared<sei_message>();
      newSEI->parse(reader,
                    nalType,
                    nalVVC->header.nuh_temporal_id_plus1 - 1,
                    updatedParsingState.lastBufferingPeriod);

      if (newSEI->payloadType == 0)
      {
        updatedParsingState.lastBufferingPeriod =
            std::dynamic_pointer_cast<buffering_period>(newSEI->sei_payload_instance);
        specificDescription = " Buffering Period SEI";
      }

      nalVVC->rbsp = newSEI;
    }
  }
  catch (const std::exception &e)
  {
    specificDescription += " ERROR " + std::string(e.what());
    parseResult.success = false;
  }

  DEBUG_VVC("AnnexBVVC::parseAndAddNALUnit NAL " + QString::fromStdString(specificDescription));

  if (this->auDelimiterDetector.isStartOfNewAU(nalVVC,
                                               updatedParsingState.currentPictureHeaderStructure))
  {
    auto &parsingStatePreviousAU = this->parsingState;
    parseResult.bitrateEntry     = createBitrateEntryForAU(parsingStatePreviousAU, bitrateEntry);
    if (!this->handleNewAU(parsingStatePreviousAU))
    {
      specificDescription += " ERROR Adding POC " +
                             std::to_string(parsingStatePreviousAU.currentAU.poc) +
                             " to frame list";
      parseResult.success = false;
    }

    updatedParsingState.currentAU.fileStartEndPos = nalStartEndPosFile;
    updatedParsingState.currentAU.sizeBytes       = 0;
    updatedParsingState.currentAU.counter++;
  }
  else if (nalStartEndPosFile)
  {
    if (updatedParsingState.currentAU.fileStartEndPos)
      updatedParsingState.currentAU.fileStartEndPos->end = nalStartEndPosFile->end;
    else
      updatedParsingState.currentAU.fileStartEndPos = nalStartEndPosFile;
  }

  this->parsingState = updatedParsingState;
  this->parsingState.currentAU.sizeBytes += data.size();

  if (nalRoot)
  {
    auto name = "NAL " + std::to_string(nalVVC->nalIdx) + ": " +
                std::to_string(nalVVC->header.nalUnitTypeID) + specificDescription;
    nalRoot->setProperties(name);
  }

  return parseResult;
}

int AnnexBVVC::calculateAndUpdateGlobalPOC(bool isIRAP, unsigned PicOrderCntVal)
{
  if (isIRAP && this->maxPOCCount > 0 && PicOrderCntVal == 0)
  {
    this->pocCounterOffset = this->maxPOCCount + 1;
    this->maxPOCCount      = 0;
  }
  auto poc = this->pocCounterOffset + PicOrderCntVal;
  if (poc > this->maxPOCCount && !isIRAP)
    this->maxPOCCount = poc;
  return poc;
}

bool AnnexBVVC::handleNewAU(ParsingState &parsingState)
{
  DEBUG_VVC("Start of new AU. Adding bitrate " << parsingState.currentAU.sizeBytes << " POC "
                                               << parsingState.currentAU.poc << " AU "
                                               << parsingState.currentAU.counter);

  if (!this->addFrameToList(parsingState.currentAU.poc,
                            parsingState.currentAU.fileStartEndPos,
                            parsingState.currentAU.isKeyframe))
    return false;

  if (this->parsingState.currentAU.fileStartEndPos)
    DEBUG_VVC("Adding start/end " << parsingState.currentAU.fileStartEndPos->first << "/"
                                  << parsingState.currentAU.fileStartEndPos->second << " - AU "
                                  << parsingState.currentAU.counter
                                  << (parsingState.currentAU.isKeyframe ? " - ra" : ""));
  else
    DEBUG_VVC("Adding start/end NA/NA - AU " << parsingState.currentAU.counter
                                             << (parsingState.currentAU.isKeyframe ? " - ra" : ""));

  return true;
}

// 7.4.2.4.3
bool AnnexBVVC::auDelimiterDetector_t::isStartOfNewAU(
    std::shared_ptr<vvc::NalUnitVVC> nal, std::shared_ptr<vvc::picture_header_structure> ph)
{
  if (!nal || !ph)
    return false;

  auto nalType = nal->header.nal_unit_type;

  if (this->lastNalWasVcl &&
      (nalType == NalType::AUD_NUT || nalType == NalType::OPI_NUT || nalType == NalType::DCI_NUT ||
       nalType == NalType::VPS_NUT || nalType == NalType::SPS_NUT || nalType == NalType::PPS_NUT ||
       nalType == NalType::PREFIX_APS_NUT || nalType == NalType::PH_NUT ||
       nalType == NalType::PREFIX_SEI_NUT || nalType == NalType::RSV_NVCL_26 ||
       nalType == NalType::UNSPEC_28 || nalType == NalType::UNSPEC_29))
  {
    this->lastNalWasVcl = false;
    return true;
  }

  auto isSlice = (nalType == NalType::TRAIL_NUT || nalType == NalType::STSA_NUT ||
                  nalType == NalType::RADL_NUT || nalType == NalType::RASL_NUT ||
                  nalType == NalType::IDR_W_RADL || nalType == NalType::IDR_N_LP ||
                  nalType == NalType::CRA_NUT || nalType == NalType::GDR_NUT);

  auto isVcl = (isSlice || nalType == NalType::RSV_VCL_4 || nalType == NalType::RSV_VCL_5 ||
                nalType == NalType::RSV_VCL_6 || nalType == NalType::RSV_IRAP_11);

  if (isVcl && this->lastNalWasVcl)
  {
    if (nal->header.nuh_layer_id != this->lastVcl_nuh_layer_id ||
        ph->ph_pic_order_cnt_lsb != this->lastVcl_ph_pic_order_cnt_lsb ||
        ph->PicOrderCntVal != this->lastVcl_PicOrderCntVal)
    {
      this->lastVcl_nuh_layer_id         = nal->header.nuh_layer_id;
      this->lastVcl_ph_pic_order_cnt_lsb = ph->ph_pic_order_cnt_lsb;
      this->lastVcl_PicOrderCntVal       = ph->PicOrderCntVal;
      return true;
    }
  }

  this->lastNalWasVcl = isVcl;
  return false;
}

} // namespace parser
