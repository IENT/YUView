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

#include "AnnexBAVC.h"

#include <cmath>

#include "NalUnitAVC.h"
#include "SEI/buffering_period.h"
#include "SEI/pic_timing.h"
#include "SEI/sei_rbsp.h"
#include "nal_unit_header.h"
#include "parser/Subtitles/AnnexBItuTT35.h"
#include "parser/common/SubByteReaderLogging.h"
#include "pic_parameter_set_rbsp.h"
#include "seq_parameter_set_rbsp.h"
#include "slice_header.h"
#include "slice_rbsp.h"
#include <parser/common/Functions.h>

#define PARSER_AVC_DEBUG_OUTPUT 0
#if PARSER_AVC_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_AVC(msg) qDebug() << msg
#else
#define DEBUG_AVC(fmt) ((void)0)
#endif

namespace parser
{

using namespace avc;

namespace
{

struct CurrentSliceData
{
  int  poc{};
  bool isRandomAccess{};
};

std::optional<FrameParsingData>
getFrameDataWithUpdatedPosition(std::optional<FrameParsingData> data,
                                std::optional<FileStartEndPos>  nalStartEndPosFile,
                                std::optional<CurrentSliceData> currentSliceData)
{
  auto newData = data.value_or(FrameParsingData());
  if (nalStartEndPosFile)
  {
    if (!newData.fileStartEndPos)
      newData.fileStartEndPos = nalStartEndPosFile;
    else
      newData.fileStartEndPos->end = nalStartEndPosFile->end;
  }
  if (currentSliceData)
  {
    newData.poc            = currentSliceData->poc;
    newData.isRandomAccess = currentSliceData->isRandomAccess;
  }
  return newData;
}

} // namespace

double AnnexBAVC::getFramerate() const
{
  // Find the first SPS and return the framerate (if signaled)
  for (auto &nal : this->nalUnitsForSeeking)
  {
    if (nal->header.nal_unit_type == NalType::SPS)
    {
      auto spsRBSP = std::dynamic_pointer_cast<seq_parameter_set_rbsp>(nal->rbsp);
      if (spsRBSP->seqParameterSetData.vui_parameters_present_flag &&
          spsRBSP->seqParameterSetData.vuiParameters.timing_info_present_flag)
        return spsRBSP->seqParameterSetData.vuiParameters.frameRate;
    }
  }

  return 0.0;
}

Size AnnexBAVC::getSequenceSizeSamples() const
{
  // Find the first SPS and return the size
  for (auto &nal : this->nalUnitsForSeeking)
  {
    if (nal->header.nal_unit_type == NalType::SPS)
    {
      auto  spsRBSP = std::dynamic_pointer_cast<seq_parameter_set_rbsp>(nal->rbsp);
      auto &sps     = spsRBSP->seqParameterSetData;
      // We need the decoder picture size and the frame_crop parameters
      int w = sps.PicWidthInSamplesL;
      int h = sps.PicHeightInSamplesL;
      if (sps.frame_cropping_flag)
      {
        w -= (sps.CropUnitX * sps.frame_crop_right_offset) +
             (sps.CropUnitX * sps.frame_crop_left_offset);
        h -= (sps.CropUnitY * sps.frame_crop_bottom_offset) +
             (sps.CropUnitY * sps.frame_crop_top_offset);
      }

      return Size(w, h);
    }
  }

  return {};
}

video::yuv::PixelFormatYUV AnnexBAVC::getPixelFormat() const
{
  using Subsampling = video::yuv::Subsampling;

  // Get the subsampling and bit-depth from the sps
  int  bitDepthY   = -1;
  int  bitDepthC   = -1;
  auto subsampling = Subsampling::UNKNOWN;
  for (auto &nal : this->nalUnitsForSeeking)
  {
    if (nal->header.nal_unit_type == NalType::SPS)
    {
      auto  spsRBSP = std::dynamic_pointer_cast<seq_parameter_set_rbsp>(nal->rbsp);
      auto &sps     = spsRBSP->seqParameterSetData;
      if (sps.chroma_format_idc == 0)
        subsampling = Subsampling::YUV_400;
      else if (sps.chroma_format_idc == 1)
        subsampling = Subsampling::YUV_420;
      else if (sps.chroma_format_idc == 2)
        subsampling = Subsampling::YUV_422;
      else if (sps.chroma_format_idc == 3)
        subsampling = Subsampling::YUV_444;

      bitDepthY = sps.bit_depth_luma_minus8 + 8;
      bitDepthC = sps.bit_depth_chroma_minus8 + 8;
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

AnnexB::ParseResult
AnnexBAVC::parseAndAddNALUnit(int                                           nalID,
                              const ByteVector &                            data,
                              std::optional<BitratePlotModel::BitrateEntry> bitrateEntry,
                              std::optional<FileStartEndPos>                nalStartEndPosFile,
                              std::shared_ptr<TreeItem>                     parent)
{
  AnnexB::ParseResult parseResult;

  if (nalID == -1 && data.empty())
  {
    if (this->curFrameData && this->curFrameData->poc)
    {
      // Save the info of the last frame
      if (!this->addFrameToList(*this->curFrameData->poc,
                                this->curFrameData->fileStartEndPos,
                                this->curFrameData->isRandomAccess))
      {
        if (parent)
          parent->createChildItem("Error - POC " + std::to_string(*this->curFrameData->poc) +
                                  "alread in the POC list.");
        return parseResult;
      }
      if (this->curFrameData->fileStartEndPos)
        DEBUG_AVC("AnnexBAVC::parseAndAddNALUnit Adding start/end "
                  << this->curFrameData->fileStartEndPos->first << "/"
                  << this->curFrameData->fileStartEndPos->second << " - POC "
                  << *this->curFrameData->poc
                  << (this->curFrameData->isRandomAccess ? " - ra" : ""));
      else
        DEBUG_AVC("AnnexBAVC::parseAndAddNALUnit Adding start/end NA/NA - POC "
                  << *this->curFrameData->poc
                  << (this->curFrameData->isRandomAccess ? " - ra" : ""));
    }
    // The file ended
    this->hrd.endOfFile(this->getHRDPlotModel());
    return parseResult;
  }

  // Use the given tree item. If it is not set, use the nalUnitMode (if active).
  // We don't set data (a name) for this item yet.
  // We want to parse the item and then set a good description.
  std::shared_ptr<TreeItem> nalRoot;
  if (parent)
    nalRoot = parent->createChildItem();
  else if (packetModel->rootItem)
    nalRoot = packetModel->rootItem->createChildItem();

  if (nalRoot)
    AnnexB::logNALSize(data, nalRoot, nalStartEndPosFile);

  reader::SubByteReaderLogging reader(data, nalRoot, "", getStartCodeOffset(data));

  std::string specificDescription;
  auto        nalAVC = std::make_shared<NalUnitAVC>(nalID, nalStartEndPosFile);

  std::optional<CurrentSliceData> currentSliceData;

  bool        currentSliceIntra = false;
  std::string currentSliceType;
  try
  {
    nalAVC->header.parse(reader);

    if (nalAVC->header.nal_unit_type == NalType::SPS)
    {
      specificDescription = " SPS";
      auto newSPS         = std::make_shared<seq_parameter_set_rbsp>();
      newSPS->parse(reader);

      this->activeParameterSets.spsMap[newSPS->seqParameterSetData.seq_parameter_set_id] = newSPS;

      specificDescription +=
          " ID " + std::to_string(newSPS->seqParameterSetData.seq_parameter_set_id);

      if (newSPS->seqParameterSetData.vuiParameters.nal_hrd_parameters_present_flag ||
          newSPS->seqParameterSetData.vuiParameters.vcl_hrd_parameters_present_flag)
        this->CpbDpbDelaysPresentFlag = true;

      if (newSPS->seqParameterSetData.vui_parameters_present_flag &&
          newSPS->seqParameterSetData.vuiParameters.nal_hrd_parameters_present_flag)
      {
        this->getHRDPlotModel()->setCPBBufferSize(
            newSPS->seqParameterSetData.vuiParameters.nalHrdParameters.CpbSize[0]);
      }

      DEBUG_AVC("AnnexBAVC::parseAndAddNALUnit Parse SPS ID "
                << newSPS->seqParameterSetData.seq_parameter_set_id);

      nalAVC->rbsp    = newSPS;
      nalAVC->rawData = data;
      this->nalUnitsForSeeking.push_back(nalAVC);
      parseResult.unitTypeName =
          "SPS(" + std::to_string(newSPS->seqParameterSetData.seq_parameter_set_id) + ") ";
    }
    else if (nalAVC->header.nal_unit_type == NalType::PPS)
    {
      specificDescription = " PPS";
      auto newPPS         = std::make_shared<pic_parameter_set_rbsp>();
      newPPS->parse(reader, this->activeParameterSets.spsMap);

      this->activeParameterSets.ppsMap[newPPS->pic_parameter_set_id] = newPPS;

      specificDescription += " ID " + std::to_string(newPPS->pic_parameter_set_id);

      DEBUG_AVC("AnnexBAVC::parseAndAddNALUnit Parse PPS ID " << newPPS->pic_parameter_set_id);

      nalAVC->rbsp    = newPPS;
      nalAVC->rawData = data;
      this->nalUnitsForSeeking.push_back(nalAVC);
      parseResult.unitTypeName = "PPS(" + std::to_string(newPPS->pic_parameter_set_id) + ") ";
    }
    else if (nalAVC->header.nal_unit_type == NalType::CODED_SLICE_NON_IDR ||
             nalAVC->header.nal_unit_type == NalType::CODED_SLICE_IDR ||
             nalAVC->header.nal_unit_type == NalType::CODED_SLICE_DATA_PARTITION_A)
    {
      std::shared_ptr<slice_header> newSliceHeader;
      if (nalAVC->header.nal_unit_type == NalType::CODED_SLICE_DATA_PARTITION_A)
      {
        auto slice = std::make_shared<slice_data_partition_a_layer_rbsp>();
        slice->parse(reader,
                     this->activeParameterSets.spsMap,
                     this->activeParameterSets.ppsMap,
                     nalAVC->header.nal_unit_type,
                     nalAVC->header.nal_ref_idc,
                     this->last_picture_first_slice);
        nalAVC->rbsp        = slice;
        newSliceHeader      = slice->sliceHeader;
        specificDescription = " Slice Partition A";
      }
      else
      {
        auto slice = std::make_shared<slice_layer_without_partitioning_rbsp>();
        slice->parse(reader,
                     this->activeParameterSets.spsMap,
                     this->activeParameterSets.ppsMap,
                     nalAVC->header.nal_unit_type,
                     nalAVC->header.nal_ref_idc,
                     this->last_picture_first_slice);
        nalAVC->rbsp        = slice;
        newSliceHeader      = slice->sliceHeader;
        specificDescription = " Slice";
      }

      std::shared_ptr<seq_parameter_set_rbsp> refSPS;
      if (this->activeParameterSets.ppsMap.count(newSliceHeader->pic_parameter_set_id) > 0)
      {
        auto pps = this->activeParameterSets.ppsMap.at(newSliceHeader->pic_parameter_set_id);
        if (this->activeParameterSets.spsMap.count(pps->seq_parameter_set_id))
        {
          refSPS = this->activeParameterSets.spsMap.at(pps->seq_parameter_set_id);
        }
      }

      if (nalAVC->header.nal_unit_type == NalType::CODED_SLICE_DATA_PARTITION_A)
        this->currentAUPartitionASPS = refSPS;

      if (nalAVC->header.nal_unit_type == NalType::CODED_SLICE_IDR)
        specificDescription += " IDR";
      else if (newSliceHeader->slice_type == SliceType::SLICE_I)
        specificDescription += " I-Slice";

      const auto isRandomAccess = (nalAVC->header.nal_unit_type == NalType::CODED_SLICE_IDR ||
                                   newSliceHeader->slice_type == SliceType::SLICE_I);
      const auto isFirstSlice   = (newSliceHeader->first_mb_in_slice == 0);
      if (!newSliceHeader->bottom_field_flag &&
          (!this->last_picture_first_slice ||
           newSliceHeader->TopFieldOrderCnt != this->last_picture_first_slice->TopFieldOrderCnt ||
           isRandomAccess) &&
          isFirstSlice)
        this->last_picture_first_slice = newSliceHeader;

      specificDescription += " POC " + std::to_string(newSliceHeader->globalPOC);

      if (isFirstSlice)
      {
        currentSliceData = CurrentSliceData({newSliceHeader->globalPOC, isRandomAccess});
        this->currentAUAssociatedSPS = refSPS;
      }

      if (isRandomAccess && isFirstSlice)
        this->nalUnitsForSeeking.push_back(nalAVC);

      currentSliceIntra = isRandomAccess;
      currentSliceType  = to_string(newSliceHeader->slice_type);

      DEBUG_AVC("AnnexBAVC::parseAndAddNALUnit Parsed Slice POC " << newSliceHeader->globalPOC);
      parseResult.unitTypeName = "Slice(POC " + std::to_string(newSliceHeader->globalPOC) + ") ";
    }
    else if (nalAVC->header.nal_unit_type == NalType::CODED_SLICE_DATA_PARTITION_B)
    {
      if (!this->currentAUPartitionASPS)
        throw std::logic_error("No partition A slice header found.");
      auto slice = std::make_shared<slice_data_partition_b_layer_rbsp>();
      slice->parse(reader, this->currentAUPartitionASPS);
      specificDescription      = " Slice Partition B";
      parseResult.unitTypeName = "Slice-PartB ";
    }
    else if (nalAVC->header.nal_unit_type == NalType::CODED_SLICE_DATA_PARTITION_C)
    {
      if (!this->currentAUPartitionASPS)
        throw std::logic_error("No partition A slice header found.");
      auto slice = std::make_shared<slice_data_partition_c_layer_rbsp>();
      slice->parse(reader, this->currentAUPartitionASPS);
      specificDescription      = " Slice Partition C";
      parseResult.unitTypeName = "Slice-PartC ";
    }
    else if (nalAVC->header.nal_unit_type == NalType::SEI)
    {
      specificDescription = " SEI";
      auto newSEI         = std::make_shared<sei_rbsp>();
      newSEI->parse(reader, this->activeParameterSets.spsMap, this->currentAUAssociatedSPS);

      for (const auto &sei : newSEI->seis)
      {
        specificDescription += " " + sei.getPayloadTypeName();

        if (sei.payloadType == 0)
        {
          auto bufferingPeriod = std::dynamic_pointer_cast<buffering_period>(sei.payload);
          if (!this->lastBufferingPeriodSEI)
            this->lastBufferingPeriodSEI = bufferingPeriod;
          else
            this->newBufferingPeriodSEI = bufferingPeriod;
          this->nextAUIsFirstAUInBufferingPeriod = true;
        }
        else if (sei.payloadType == 1)
        {
          auto picTiming = std::dynamic_pointer_cast<pic_timing>(sei.payload);
          if (!this->lastPicTimingSEI)
            this->lastPicTimingSEI = picTiming;
          else
            this->newPicTimingSEI = picTiming;
        }
      }

      for (const auto &sei : newSEI->seisReparse)
        this->reparse_sei.push(sei);

      nalAVC->rbsp = newSEI;
      specificDescription += "(x" + std::to_string(newSEI->seis.size()) + ")";
      DEBUG_AVC("AnnexBAVC::parseAndAddNALUnit Parsed SEI (" << newSEI->seis.size()
                                                             << " messages)");
      parseResult.unitTypeName = "SEI(x" + std::to_string(newSEI->seis.size()) + ") ";
    }
    else if (nalAVC->header.nal_unit_type == NalType::FILLER)
    {
      specificDescription = " Filler";
      DEBUG_AVC("AnnexBAVC::parseAndAddNALUnit Parsed Filler data");
      parseResult.unitTypeName = "Filler ";
    }
    else if (nalAVC->header.nal_unit_type == NalType::AUD)
    {
      specificDescription = " AUD";
      DEBUG_AVC("AnnexBAVC::parseAndAddNALUnit Parsed AUD");
      parseResult.unitTypeName = "AUD ";
    }

    if (nalAVC->header.nal_unit_type == NalType::CODED_SLICE_IDR ||
        nalAVC->header.nal_unit_type == NalType::CODED_SLICE_NON_IDR ||
        nalAVC->header.nal_unit_type == NalType::CODED_SLICE_DATA_PARTITION_A)
    {
      // Reparse the SEI messages that we could not parse so far. This is a slice so all parameter
      // sets should be available now.
      while (!this->reparse_sei.empty())
      {
        try
        {
          auto &sei = this->reparse_sei.front();
          sei.reparse(this->activeParameterSets.spsMap, this->currentAUAssociatedSPS);
          reparse_sei.pop();
        }
        catch (const std::exception &e)
        {
          (void)e;
          DEBUG_AVC("AnnexBAVC::parseAndAddNALUnit Error reparsing SEI");
        }
      }
    }
  }
  catch (const std::exception &e)
  {
    specificDescription += " ERROR " + std::string(e.what());
    parseResult.success = false;
  }

  std::optional<int> curSlicePoc;
  if (currentSliceData)
    curSlicePoc = currentSliceData->poc;
  if (this->curFrameData && this->auDelimiterDetector.isStartOfNewAU(nalAVC, curSlicePoc))
  {
    // Save the info of the last frame
    if (!this->addFrameToList(*this->curFrameData->poc,
                              this->curFrameData->fileStartEndPos,
                              this->curFrameData->isRandomAccess))
    {
      throw std::logic_error("Error - POC " + std::to_string(*this->curFrameData->poc) +
                             " already in the POC list");
    }
    if (this->curFrameData->fileStartEndPos)
      DEBUG_AVC("AnnexBAVC::parseAndAddNALUnit Adding start/end "
                << this->curFrameData->fileStartEndPos->first << "/"
                << this->curFrameData->fileStartEndPos->second << " - POC "
                << *this->curFrameData->poc << (this->curFrameData->isRandomAccess ? " - ra" : ""));
    else
      DEBUG_AVC("AnnexBAVC::parseAndAddNALUnit Adding start/end NA/NA - POC "
                << *this->curFrameData->poc << (this->curFrameData->isRandomAccess ? " - ra" : ""));

    if (this->sizeCurrentAU > 0)
    {
      DEBUG_AVC("AnnexBAVC::parseAndAddNALUnit Start of new AU. Adding bitrate "
                << this->sizeCurrentAU);

      BitratePlotModel::BitrateEntry entry;
      if (bitrateEntry)
      {
        entry.pts      = bitrateEntry->pts;
        entry.dts      = bitrateEntry->dts;
        entry.duration = bitrateEntry->duration;
      }
      else
      {
        entry.pts          = this->lastFramePOC;
        entry.dts          = this->counterAU;
        entry.duration     = 1;
        this->lastFramePOC = *this->curFrameData->poc;
      }
      entry.bitrate  = this->sizeCurrentAU;
      entry.keyframe = this->currentAUAllSlicesIntra;
      entry.frameType =
          QString::fromStdString(convertSliceCountsToString(this->currentAUSliceTypes));
      parseResult.bitrateEntry = entry;

      if (this->lastBufferingPeriodSEI && this->lastPicTimingSEI)
      {
        try
        {
          if (this->activeParameterSets.spsMap.size() > 0)
            this->hrd.addAU(this->sizeCurrentAU * 8,
                            *this->curFrameData->poc,
                            this->activeParameterSets.spsMap[0],
                            this->lastBufferingPeriodSEI,
                            this->lastPicTimingSEI,
                            this->getHRDPlotModel());
        }
        catch (const std::exception &e)
        {
          specificDescription += " HRD Error: " + std::string(e.what());
        }
      }
    }
    this->sizeCurrentAU = 0;
    this->counterAU++;
    this->currentAUAllSlicesIntra = true;
    this->currentAUSliceTypes.clear();
    this->currentAUAssociatedSPS.reset();
    this->currentAUPartitionASPS.reset();
    this->curFrameData.reset();
  }
  curFrameData =
      getFrameDataWithUpdatedPosition(curFrameData, nalStartEndPosFile, currentSliceData);

  if (this->newBufferingPeriodSEI)
    this->lastBufferingPeriodSEI = this->newBufferingPeriodSEI;
  if (this->newPicTimingSEI)
    this->lastPicTimingSEI = this->newPicTimingSEI;
  this->sizeCurrentAU += data.size();

  if (this->nextAUIsFirstAUInBufferingPeriod)
  {
    if (this->counterAU > 0)
      this->hrd.isFirstAUInBufferingPeriod = true;
    this->nextAUIsFirstAUInBufferingPeriod = false;
  }

  if (nalAVC->header.nal_unit_type == NalType::CODED_SLICE_IDR ||
      nalAVC->header.nal_unit_type == NalType::CODED_SLICE_NON_IDR ||
      nalAVC->header.nal_unit_type == NalType::CODED_SLICE_DATA_PARTITION_A)
  {
    if (!currentSliceIntra)
      this->currentAUAllSlicesIntra = false;
    this->currentAUSliceTypes[currentSliceType]++;
  }

  if (nalRoot)
  {
    auto name = "NAL " + std::to_string(nalAVC->nalIdx) + ": " +
                std::to_string(nalAVC->header.nalUnitTypeID) + specificDescription;
    nalRoot->setProperties(name);
  }

  parseResult.success = true;
  return parseResult;
}

std::optional<AnnexB::SeekData> AnnexBAVC::getSeekData(int iFrameNr)
{
  if (iFrameNr >= int(this->getNumberPOCs()) || iFrameNr < 0)
    return {};

  auto seekPOC = this->getFramePOC(unsigned(iFrameNr));

  // Collect the active parameter sets
  using NalMap = std::map<unsigned, std::shared_ptr<NalUnitAVC>>;
  NalMap activeSPSNal;
  NalMap activePPSNal;

  for (auto nal : this->nalUnitsForSeeking)
  {
    if (nal->header.nal_unit_type == NalType::CODED_SLICE_IDR ||
        nal->header.nal_unit_type == NalType::CODED_SLICE_NON_IDR ||
        nal->header.nal_unit_type == NalType::CODED_SLICE_DATA_PARTITION_A)
    {
      int globalPOC{};
      if (nal->header.nal_unit_type == NalType::CODED_SLICE_IDR ||
          nal->header.nal_unit_type == NalType::CODED_SLICE_NON_IDR)
      {
        auto slice = std::dynamic_pointer_cast<slice_layer_without_partitioning_rbsp>(nal->rbsp);
        globalPOC  = slice->sliceHeader->globalPOC;
      }
      else if (nal->header.nal_unit_type == NalType::CODED_SLICE_DATA_PARTITION_A)
      {
        auto slice = std::dynamic_pointer_cast<slice_data_partition_a_layer_rbsp>(nal->rbsp);
        globalPOC  = slice->sliceHeader->globalPOC;
      }

      if (globalPOC == seekPOC)
      {
        // Seek here
        AnnexB::SeekData seekData;
        if (nal->fileStartEndPos)
          seekData.filePos = nal->fileStartEndPos->start;

        // Get the bitstream of all active parameter sets
        for (const auto &nalMap : {activeSPSNal, activePPSNal})
        {
          for (auto const &entry : nalMap)
            seekData.parameterSets.push_back(entry.second->rawData);
        }

        return seekData;
      }
    }
    else if (nal->header.nal_unit_type == NalType::SPS)
    {
      // Add sps (replace old one if existed)
      auto sps = std::dynamic_pointer_cast<seq_parameter_set_rbsp>(nal->rbsp);
      activeSPSNal[sps->seqParameterSetData.seq_parameter_set_id] = nal;
    }
    else if (nal->header.nal_unit_type == NalType::PPS)
    {
      // Add pps (replace old one if existed)
      auto pps = std::dynamic_pointer_cast<pic_parameter_set_rbsp>(nal->rbsp);
      activePPSNal[pps->pic_parameter_set_id] = nal;
    }
  }

  return {};
}

QByteArray AnnexBAVC::getExtradata()
{
  // Convert the SPS and PPS that we found in the bitstream to the libavformat avcc format (see
  // avc.c)
  ByteVector e;
  e.push_back((unsigned char)1); /* version */

  // Get the first SPS and PPS
  std::shared_ptr<seq_parameter_set_rbsp> sps;
  std::shared_ptr<pic_parameter_set_rbsp> pps;
  ByteVector                              spsData;
  ByteVector                              ppsData;
  for (auto nal : this->nalUnitsForSeeking)
  {
    if (nal->header.nal_unit_type == NalType::SPS)
    {
      sps     = std::dynamic_pointer_cast<seq_parameter_set_rbsp>(nal->rbsp);
      spsData = nal->rawData;
    }
    if (nal->header.nal_unit_type == NalType::PPS)
    {
      pps     = std::dynamic_pointer_cast<pic_parameter_set_rbsp>(nal->rbsp);
      ppsData = nal->rawData;
    }

    if (sps && pps)
      break;
  }

  if (!sps || !pps)
    return {};

  // Add profile, compatibility and level
  int profile_compat = 0;
  if (sps->seqParameterSetData.constraint_set0_flag)
    profile_compat += 128;
  if (sps->seqParameterSetData.constraint_set1_flag)
    profile_compat += 64;
  if (sps->seqParameterSetData.constraint_set2_flag)
    profile_compat += 32;
  if (sps->seqParameterSetData.constraint_set3_flag)
    profile_compat += 16;
  if (sps->seqParameterSetData.constraint_set4_flag)
    profile_compat += 8;
  if (sps->seqParameterSetData.constraint_set5_flag)
    profile_compat += 4;

  e.push_back(sps->seqParameterSetData.profile_idc);
  e.push_back(profile_compat);
  e.push_back(sps->seqParameterSetData.level_idc);

  e.push_back((unsigned char)255); /* 6 bits reserved (111111) + 2 bits nal size length - 1 (11) */
  e.push_back((unsigned char)225); /* 3 bits reserved (111) + 5 bits number of sps (00001) */

  // Write SPS
  while (spsData.back() == 0)
    spsData.pop_back();
  auto dataSizeSPS = spsData.size() - getStartCodeOffset(spsData);
  e.push_back((unsigned char)(dataSizeSPS >> 8));
  e.push_back((unsigned char)(dataSizeSPS & 0xff));
  e.insert(e.end(), spsData.begin() + getStartCodeOffset(spsData), spsData.end());

  // Write PPS
  e.push_back((unsigned char)0x01); /* number of pps */
  while (ppsData.back() == 0)
    ppsData.pop_back();

  auto dataSizePPS = ppsData.size() - getStartCodeOffset(ppsData);
  e.push_back((unsigned char)(dataSizePPS >> 8));
  e.push_back((unsigned char)(dataSizePPS & 0xff));
  e.insert(e.end(), ppsData.begin() + getStartCodeOffset(ppsData), ppsData.end());

  return reader::SubByteReaderLogging::convertToQByteArray(e);
}

IntPair AnnexBAVC::getProfileLevel()
{
  for (auto nal : this->nalUnitsForSeeking)
  {
    if (nal->header.nal_unit_type == NalType::SPS)
    {
      auto sps = std::dynamic_pointer_cast<seq_parameter_set_rbsp>(nal->rbsp);
      return {sps->seqParameterSetData.profile_idc, sps->seqParameterSetData.level_idc};
    }
  }
  return {};
}

Ratio AnnexBAVC::getSampleAspectRatio()
{
  for (auto nal : this->nalUnitsForSeeking)
  {
    if (nal->header.nal_unit_type == NalType::SPS)
    {
      auto sps = std::dynamic_pointer_cast<seq_parameter_set_rbsp>(nal->rbsp);
      if (sps->seqParameterSetData.vui_parameters_present_flag &&
          sps->seqParameterSetData.vuiParameters.aspect_ratio_info_present_flag)
      {
        auto aspect_ratio_idc = sps->seqParameterSetData.vuiParameters.aspect_ratio_idc;
        if (aspect_ratio_idc > 0 && aspect_ratio_idc <= 16)
        {
          int widths[]  = {1, 12, 10, 16, 40, 24, 20, 32, 80, 18, 15, 64, 160, 4, 3, 2};
          int heights[] = {1, 11, 11, 11, 33, 11, 11, 11, 33, 11, 11, 33, 99, 3, 2, 1};

          const auto i = aspect_ratio_idc - 1;
          return Ratio({widths[i], heights[i]});
        }
        if (aspect_ratio_idc == 255)
          return Ratio({int(sps->seqParameterSetData.vuiParameters.sar_width),
                        int(sps->seqParameterSetData.vuiParameters.sar_height)});
      }
    }
  }
  return Ratio({1, 1});
}

} // namespace parser