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

#include "AnnexBHEVC.h"

#include <algorithm>
#include <cmath>

#include "SEI/buffering_period.h"
#include "SEI/pic_timing.h"
#include "SEI/sei_rbsp.h"
#include "parser/Subtitles/AnnexBItuTT35.h"
#include "parser/common/Macros.h"
#include "parser/common/SubByteReaderLogging.h"
#include "parser/common/functions.h"
#include "pic_parameter_set_rbsp.h"
#include "seq_parameter_set_rbsp.h"
#include "slice_segment_layer_rbsp.h"
#include "video_parameter_set_rbsp.h"

#define PARSER_HEVC_DEBUG_OUTPUT 0
#if PARSER_HEVC_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_HEVC(msg) qDebug() << msg
#else
#define DEBUG_HEVC(msg) ((void)0)
#endif

namespace parser
{

using namespace hevc;

double AnnexBHEVC::getFramerate() const
{
  // First try to get the framerate from the parameter sets themselves
  for (const auto &nal : this->nalUnitsForSeeking)
  {
    if (nal->header.nal_unit_type == NalType::VPS_NUT)
    {
      auto vps = std::dynamic_pointer_cast<video_parameter_set_rbsp>(nal->rbsp);

      if (vps->vps_timing_info_present_flag)
        return vps->frameRate;
    }
  }

  // The VPS had no information on the frame rate.
  // Look for VUI information in the sps
  for (const auto &nal : this->nalUnitsForSeeking)
  {
    if (nal->header.nal_unit_type == hevc::NalType::SPS_NUT)
    {
      auto sps = std::dynamic_pointer_cast<seq_parameter_set_rbsp>(nal->rbsp);
      if (sps->vui_parameters_present_flag && sps->vuiParameters.vui_timing_info_present_flag)
        return sps->vuiParameters.frameRate;
    }
  }

  return DEFAULT_FRAMERATE;
}

QSize AnnexBHEVC::getSequenceSizeSamples() const
{
  for (const auto &nal : this->nalUnitsForSeeking)
  {
    if (nal->header.nal_unit_type == hevc::NalType::SPS_NUT)
    {
      auto sps = std::dynamic_pointer_cast<seq_parameter_set_rbsp>(nal->rbsp);
      return QSize(sps->get_conformance_cropping_width(), sps->get_conformance_cropping_height());
    }
  }

  return QSize(-1, -1);
}

yuvPixelFormat AnnexBHEVC::getPixelFormat() const
{
  // Get the subsampling and bit-depth from the sps
  int  bitDepthY   = -1;
  int  bitDepthC   = -1;
  auto subsampling = Subsampling::UNKNOWN;
  for (const auto &nal : this->nalUnitsForSeeking)
  {
    if (nal->header.nal_unit_type == hevc::NalType::SPS_NUT)
    {
      auto sps = std::dynamic_pointer_cast<seq_parameter_set_rbsp>(nal->rbsp);
      if (sps->chroma_format_idc == 0)
        subsampling = Subsampling::YUV_400;
      else if (sps->chroma_format_idc == 1)
        subsampling = Subsampling::YUV_420;
      else if (sps->chroma_format_idc == 2)
        subsampling = Subsampling::YUV_422;
      else if (sps->chroma_format_idc == 3)
        subsampling = Subsampling::YUV_444;

      bitDepthY = sps->bit_depth_luma_minus8 + 8;
      bitDepthC = sps->bit_depth_chroma_minus8 + 8;
    }

    if (bitDepthY != -1 && bitDepthC != -1 && subsampling != Subsampling::UNKNOWN)
    {
      if (bitDepthY != bitDepthC)
      {
        // Different luma and chroma bit depths currently not supported
        return yuvPixelFormat();
      }
      return yuvPixelFormat(subsampling, bitDepthY);
    }
  }

  return yuvPixelFormat();
}

QList<QByteArray> AnnexBHEVC::getSeekFrameParamerSets(int iFrameNr, uint64_t &filePos)
{
  if (!this->POCList.contains(iFrameNr))
    return {};

  auto seekPOC = this->POCList[iFrameNr];

  // Collect the active parameter sets
  using NalMap = std::map<unsigned, std::shared_ptr<NalUnitHEVC>>;
  NalMap activeVPSNal;
  NalMap activeSPSNal;
  NalMap activePPSNal;

  for (const auto &nal : this->nalUnitsForSeeking)
  {
    if (nal->header.isSlice())
    {
      auto slice = std::dynamic_pointer_cast<slice_segment_layer_rbsp>(nal->rbsp);

      if (slice->sliceSegmentHeader.globalPOC == seekPOC)
      {
        // Seek here
        if (nal->filePosStartEnd)
          filePos = nal->filePosStartEnd->first;

        // Get the bitstream of all active parameter sets
        QList<QByteArray> paramSets;

        for (auto const &[key, vpsNal] : activeVPSNal)
          paramSets.append(reader::SubByteReaderLogging::convertToQByteArray(vpsNal->rawData));
        for (auto const &[key, spsNal] : activeSPSNal)
          paramSets.append(reader::SubByteReaderLogging::convertToQByteArray(spsNal->rawData));
        for (auto const &[key, ppsNal] : activePPSNal)
          paramSets.append(reader::SubByteReaderLogging::convertToQByteArray(ppsNal->rawData));

        return paramSets;
      }
    }
    if (nal->header.nal_unit_type == hevc::NalType::VPS_NUT)
    {
      auto vps = std::dynamic_pointer_cast<video_parameter_set_rbsp>(nal->rbsp);
      activeVPSNal[vps->vps_video_parameter_set_id] = nal;
    }
    if (nal->header.nal_unit_type == hevc::NalType::SPS_NUT)
    {
      auto sps = std::dynamic_pointer_cast<seq_parameter_set_rbsp>(nal->rbsp);
      activeSPSNal[sps->sps_seq_parameter_set_id] = nal;
    }
    if (nal->header.nal_unit_type == hevc::NalType::PPS_NUT)
    {
      auto pps = std::dynamic_pointer_cast<pic_parameter_set_rbsp>(nal->rbsp);
      activePPSNal[pps->pps_pic_parameter_set_id] = nal;
    }
  }

  return {};
}

QByteArray AnnexBHEVC::getExtradata()
{
  // Just return the VPS, SPS and PPS in NAL unit format. From the format in the extradata, ffmpeg
  // will detect that the input file is in raw NAL unit format and accept AVPackets in NAL unit
  // format.
  ByteVector ret;
  for (const auto &nal : this->nalUnitsForSeeking)
  {
    if (nal->header.nal_unit_type == hevc::NalType::VPS_NUT)
    {
      ret.insert(ret.end(), nal->rawData.begin(), nal->rawData.end());
      break;
    }
  }
  for (const auto &nal : this->nalUnitsForSeeking)
  {
    if (nal->header.nal_unit_type == hevc::NalType::SPS_NUT)
    {
      ret.insert(ret.end(), nal->rawData.begin(), nal->rawData.end());
      break;
    }
  }
  for (const auto &nal : this->nalUnitsForSeeking)
  {
    if (nal->header.nal_unit_type == hevc::NalType::PPS_NUT)
    {
      ret.insert(ret.end(), nal->rawData.begin(), nal->rawData.end());
      break;
    }
  }
  return reader::SubByteReaderLogging::convertToQByteArray(ret);
}

IntPair AnnexBHEVC::getProfileLevel()
{
  for (const auto &nal : this->nalUnitsForSeeking)
  {
    if (nal->header.nal_unit_type == hevc::NalType::SPS_NUT)
    {
      auto sps = std::dynamic_pointer_cast<seq_parameter_set_rbsp>(nal->rbsp);
      return {sps->profileTierLevel.general_profile_idc, sps->profileTierLevel.general_level_idc};
    }
  }
  return {};
}

Ratio AnnexBHEVC::getSampleAspectRatio()
{
  for (const auto &nal : this->nalUnitsForSeeking)
  {
    if (nal->header.nal_unit_type == hevc::NalType::SPS_NUT)
    {
      auto sps = std::dynamic_pointer_cast<seq_parameter_set_rbsp>(nal->rbsp);
      if (sps->vui_parameters_present_flag && sps->vuiParameters.aspect_ratio_info_present_flag)
      {
        int aspect_ratio_idc = sps->vuiParameters.aspect_ratio_idc;
        if (aspect_ratio_idc > 0 && aspect_ratio_idc <= 16)
        {
          int widths[]  = {1, 12, 10, 16, 40, 24, 20, 32, 80, 18, 15, 64, 160, 4, 3, 2};
          int heights[] = {1, 11, 11, 11, 33, 11, 11, 11, 33, 11, 11, 33, 99, 3, 2, 1};

          const int i = aspect_ratio_idc - 1;
          return Ratio({widths[i], heights[i]});
        }
        if (aspect_ratio_idc == 255)
          return Ratio({int(sps->vuiParameters.sar_width), int(sps->vuiParameters.sar_height)});
      }
    }
  }
  return Ratio({1, 1});
}

AnnexB::ParseResult
AnnexBHEVC::parseAndAddNALUnit(int                                           nalID,
                               ByteVector                                    data,
                               std::optional<BitratePlotModel::BitrateEntry> bitrateEntry,
                               std::optional<pairUint64>                     nalStartEndPosFile,
                               TreeItem *                                    parent)
{
  AnnexB::ParseResult parseResult;

  if (nalID == -1 && data.empty())
  {
    if (curFramePOC != -1)
    {
      // Save the info of the last frame
      if (!this->addFrameToList(curFramePOC, curFrameFileStartEndPos, curFrameIsRandomAccess))
      {
        ReaderHelper::addErrorMessageChildItem(
            QString("Error - POC %1 alread in the POC list.").arg(curFramePOC), parent);
        return parseResult;
      }
      if (curFrameFileStartEndPos)
        DEBUG_HEVC("AnnexBHEVC::parseAndAddNALUnit Adding start/end "
                   << curFrameFileStartEndPos->first << "/" << curFrameFileStartEndPos->second
                   << " - POC " << curFramePOC << (curFrameIsRandomAccess ? " - ra" : ""));
      else
        DEBUG_HEVC("AnnexBHEVC::parseAndAddNALUnit Adding start/end NA/NA - POC "
                   << curFramePOC << (curFrameIsRandomAccess ? " - ra" : ""));
    }
    // The file ended
    std::sort(POCList.begin(), POCList.end());
    return parseResult;
  }

  // Use the given tree item. If it is not set, use the nalUnitMode (if active).
  // Create a new TreeItem root for the NAL unit. We don't set data (a name) for this item
  // yet. We want to parse the item and then set a good description.
  TreeItem *nalRoot = nullptr;
  if (parent)
    nalRoot = new TreeItem(parent);
  else if (!packetModel->isNull())
    nalRoot = new TreeItem(packetModel->getRootItem());

  AnnexB::logNALSize(data, nalRoot, nalStartEndPosFile);

  reader::SubByteReaderLogging reader(data, nalRoot, "", getStartCodeOffset(data));

  std::string specificDescription;
  auto        nalHEVC = std::make_shared<NalUnitHEVC>(nalID, nalStartEndPosFile);

  bool        first_slice_segment_in_pic_flag = false;
  bool        currentSliceIntra               = false;
  std::string currentSliceType;
  try
  {
    nalHEVC->header.parse(reader);

    if (nalHEVC->header.nal_unit_type == NalType::VPS_NUT)
    {
      specificDescription = " VPS";
      auto newVPS         = std::make_shared<video_parameter_set_rbsp>();
      newVPS->parse(reader);

      this->activeParameterSets.vpsMap[newVPS->vps_video_parameter_set_id] = newVPS;

      specificDescription += " ID " + std::to_string(newVPS->vps_video_parameter_set_id);

      nalHEVC->rbsp    = newVPS;
      nalHEVC->rawData = data;
      this->nalUnitsForSeeking.push_back(nalHEVC);
      parseResult.nalTypeName = "VPS(" + std::to_string(newVPS->vps_video_parameter_set_id) + ") ";

      DEBUG_HEVC("AnnexBHEVC::parseAndAddNALUnit VPS ID " << newVPS->vps_video_parameter_set_id);
    }
    else if (nalHEVC->header.nal_unit_type == hevc::NalType::SPS_NUT)
    {
      specificDescription = " SPS";
      auto newSPS         = std::make_shared<seq_parameter_set_rbsp>();
      newSPS->parse(reader);

      this->activeParameterSets.spsMap[newSPS->sps_seq_parameter_set_id] = newSPS;

      specificDescription += " ID " + std::to_string(newSPS->sps_seq_parameter_set_id);

      DEBUG_HEVC("AnnexBHEVC::parseAndAddNALUnit Parse SPS ID "
                 << newSPS->sps_seq_parameter_set_id);

      nalHEVC->rbsp    = newSPS;
      nalHEVC->rawData = data;
      this->nalUnitsForSeeking.push_back(nalHEVC);
      parseResult.nalTypeName = "SPS(" + std::to_string(newSPS->sps_seq_parameter_set_id) + ") ";
    }
    else if (nalHEVC->header.nal_unit_type == hevc::NalType::PPS_NUT)
    {
      specificDescription = " PPS";
      auto newPPS         = std::make_shared<pic_parameter_set_rbsp>();
      newPPS->parse(reader);

      this->activeParameterSets.ppsMap[newPPS->pps_pic_parameter_set_id] = newPPS;

      specificDescription += " ID " + std::to_string(newPPS->pps_pic_parameter_set_id);

      DEBUG_HEVC("AnnexBHEVC::parseAndAddNALUnit Parse SPS ID "
                 << newPPS->pps_pic_parameter_set_id);

      nalHEVC->rbsp    = newPPS;
      nalHEVC->rawData = data;
      this->nalUnitsForSeeking.push_back(nalHEVC);
      parseResult.nalTypeName = "SPS(" + std::to_string(newPPS->pps_pic_parameter_set_id) + ") ";
    }
    else if (nalHEVC->header.isSlice())
    {
      specificDescription = " Slice";
      auto newSlice       = std::make_shared<slice_segment_layer_rbsp>();
      newSlice->parse(reader,
                      this->firstAUInDecodingOrder,
                      this->prevTid0PicSlicePicOrderCntLsb,
                      this->prevTid0PicPicOrderCntMsb,
                      nalHEVC->header,
                      this->activeParameterSets.spsMap,
                      this->activeParameterSets.ppsMap,
                      this->lastFirstSliceSegmentInPic);

      // Add the POC of the slice
      if (nalHEVC->header.isIRAP() && newSlice->sliceSegmentHeader.NoRaslOutputFlag &&
          maxPOCCount > 0)
      {
        pocCounterOffset = maxPOCCount + 1;
        maxPOCCount      = -1;
      }
      auto POC = pocCounterOffset + newSlice->sliceSegmentHeader.PicOrderCntVal;
      if (POC > maxPOCCount &&
          !(nalHEVC->header.isIRAP() && newSlice->sliceSegmentHeader.NoRaslOutputFlag))
        maxPOCCount = POC;
      newSlice->sliceSegmentHeader.globalPOC = POC;

      first_slice_segment_in_pic_flag =
          newSlice->sliceSegmentHeader.first_slice_segment_in_pic_flag;
      if (first_slice_segment_in_pic_flag)
        this->lastFirstSliceSegmentInPic = newSlice;

      isRandomAccessSkip = false;
      if (firstPOCRandomAccess == INT_MAX)
      {
        if (nalHEVC->header.nal_unit_type == hevc::NalType::CRA_NUT ||
            nalHEVC->header.nal_unit_type == hevc::NalType::BLA_W_LP ||
            nalHEVC->header.nal_unit_type == hevc::NalType::BLA_N_LP ||
            nalHEVC->header.nal_unit_type == hevc::NalType::BLA_W_RADL)
          // set the POC random access since we need to skip the reordered pictures in the case of
          // CRA/CRANT/BLA/BLANT.
          firstPOCRandomAccess = newSlice->sliceSegmentHeader.PicOrderCntVal;
        else if (nalHEVC->header.nal_unit_type == hevc::NalType::IDR_W_RADL ||
                 nalHEVC->header.nal_unit_type == hevc::NalType::IDR_N_LP)
          firstPOCRandomAccess =
              -INT_MAX; // no need to skip the reordered pictures in IDR, they are decodable.
        else
          isRandomAccessSkip = true;
      }
      // skip the reordered pictures, if necessary
      else if (newSlice->sliceSegmentHeader.PicOrderCntVal < firstPOCRandomAccess &&
               (nalHEVC->header.nal_unit_type == hevc::NalType::RASL_R ||
                nalHEVC->header.nal_unit_type == hevc::NalType::RASL_N))
        isRandomAccessSkip = true;

      if (nalHEVC->header.nuh_temporal_id_plus1 - 1 == 0 && !nalHEVC->header.isRASL() &&
          !nalHEVC->header.isRADL())
      {
        // Let prevTid0Pic be the previous picture in decoding order that has TemporalId
        // equal to 0 and that is not a RASL picture, a RADL picture or an SLNR picture.
        // Set these for the next slice
        this->prevTid0PicSlicePicOrderCntLsb = newSlice->sliceSegmentHeader.slice_pic_order_cnt_lsb;
        this->prevTid0PicPicOrderCntMsb      = newSlice->sliceSegmentHeader.PicOrderCntMsb;
      }

      if (newSlice->sliceSegmentHeader.first_slice_segment_in_pic_flag)
      {
        // This slice NAL is the start of a new frame
        if (curFramePOC != -1)
        {
          // Save the info of the last frame
          if (!this->addFrameToList(curFramePOC, curFrameFileStartEndPos, curFrameIsRandomAccess))
          {
            throw std::logic_error("Error - POC " + std::to_string(curFramePOC) +
                                   " alread in the POC list.");
          }
          if (curFrameFileStartEndPos)
            DEBUG_HEVC("AnnexBHEVC::parseAndAddNALUnit Adding start/end "
                       << curFrameFileStartEndPos->first << "/" << curFrameFileStartEndPos->second
                       << " - POC " << curFramePOC << (curFrameIsRandomAccess ? " - ra" : ""));
          else
            DEBUG_HEVC("AnnexBHEVC::parseAndAddNALUnit Adding start/end NA/NA - POC "
                       << curFramePOC << (curFrameIsRandomAccess ? " - ra" : ""));
        }
        curFrameFileStartEndPos = nalStartEndPosFile;
        curFramePOC             = newSlice->sliceSegmentHeader.globalPOC;
        curFrameIsRandomAccess  = nalHEVC->header.isIRAP();

        {
          std::shared_ptr<seq_parameter_set_rbsp> refSPS;
          auto ppsID = newSlice->sliceSegmentHeader.slice_pic_parameter_set_id;
          if (this->activeParameterSets.ppsMap.count(ppsID) > 0)
          {
            auto pps = this->activeParameterSets.ppsMap.at(ppsID);
            if (this->activeParameterSets.spsMap.count(pps->pps_seq_parameter_set_id))
            {
              refSPS = this->activeParameterSets.spsMap.at(pps->pps_seq_parameter_set_id);
            }
          }
          if (!refSPS)
            throw std::logic_error("Referenced SPS of slice could not be obtained.");
          this->currentAUAssociatedSPS = refSPS;
        }
      }
      else if (curFrameFileStartEndPos && nalStartEndPosFile)
        // Another slice NAL which belongs to the last frame
        // Update the end position
        curFrameFileStartEndPos->second = nalStartEndPosFile->second;

      nalHEVC->rbsp = newSlice;
      if (nalHEVC->header.isIRAP())
      {
        if (newSlice->sliceSegmentHeader.first_slice_segment_in_pic_flag)
          // This is the first slice of a random access point. Add it to the list.
          this->nalUnitsForSeeking.push_back(nalHEVC);
        currentSliceIntra = true;
      }
      currentSliceType = to_string(newSlice->sliceSegmentHeader.slice_type);

      specificDescription     = " POC " + std::to_string(POC);
      parseResult.nalTypeName = "Slice(POC " + std::to_string(POC) + ")";

      DEBUG_HEVC("AnnexBHEVC::parseAndAddNALUnit Slice POC "
                 << POC << " - pocCounterOffset " << pocCounterOffset << " maxPOCCount "
                 << maxPOCCount << (new_slice->isIRAP() ? " - IRAP" : "")
                 << (new_slice->NoRaslOutputFlag ? "" : " - RASL")
                 << (parsingSuccess ? "" : " ERROR"));
    }
    else if (nalHEVC->header.nal_unit_type == hevc::NalType::PREFIX_SEI_NUT ||
             nalHEVC->header.nal_unit_type == hevc::NalType::SUFFIX_SEI_NUT)
    {
      specificDescription = " SEI";
      auto newSEI         = std::make_shared<sei_rbsp>();
      newSEI->parse(reader,
                    nalHEVC->header.nal_unit_type,
                    this->activeParameterSets.vpsMap,
                    this->activeParameterSets.spsMap,
                    this->currentAUAssociatedSPS);

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

      nalHEVC->rbsp = newSEI;

      DEBUG_HEVC("AnnexBHEVC::parseAndAddNALUnit Parsed SEI (" << newSEI->seis.size()
                                                               << " messages)");
      parseResult.nalTypeName = "SEI(x" + std::to_string(newSEI->seis.size()) + ") ";
    }
    else if (nalHEVC->header.nal_unit_type == hevc::NalType::FD_NUT)
    {
      specificDescription = " Filler";
      DEBUG_HEVC("AnnexBHEVC::parseAndAddNALUnit Parsed Fillerdata");
      parseResult.nalTypeName = "Filler ";
    }
    else if (nalHEVC->header.nal_unit_type == hevc::NalType::UNSPEC62 ||
             nalHEVC->header.nal_unit_type == hevc::NalType::UNSPEC63)
    {
      // Dolby vision EL or metadata
      // Technically this is not a specific NAL unit type but dolby vision uses a different
      // seperator that makes the DV metadata and EL data appear to be NAL unit type 62 and 63.
      specificDescription = " Dolby Vision";
      DEBUG_HEVC("AnnexBHEVC::parseAndAddNALUnit Dolby Vision Metadata");
      parseResult.nalTypeName = "Dolby Vision ";
    }

    if (nalHEVC->header.isSlice())
    {
      // Reparse the SEI messages that we could not parse so far. This is a slice so all parameter
      // sets should be available now.
      while (!this->reparse_sei.empty())
      {
        try
        {
          auto sei = reparse_sei.front();
          reparse_sei.pop();
          sei.reparse(this->activeParameterSets.vpsMap,
                      this->activeParameterSets.spsMap,
                      this->currentAUAssociatedSPS);
        }
        catch (const std::exception &e)
        {
          (void)e;
          DEBUG_HEVC("AnnexBHEVC::parseAndAddNALUnit Error reparsing SEI message. " + e.what());
        }
      }
    }
  }
  catch (const std::exception &e)
  {
    specificDescription += " ERROR " + std::string(e.what());
    parseResult.success = false;
  }

  if (this->auDelimiterDetector.isStartOfNewAU(nalHEVC, first_slice_segment_in_pic_flag))
  {
    DEBUG_HEVC("Start of new AU. Adding bitrate " << sizeCurrentAU << " for last AU (#" << counterAU
                                                  << ").");

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
      entry.dts      = int(counterAU);
      entry.duration = 1;
    }
    entry.bitrate   = this->sizeCurrentAU;
    entry.keyframe  = this->currentAUAllSlicesIntra;
    entry.frameType = QString::fromStdString(convertSliceCountsToString(this->currentAUSliceTypes));
    parseResult.bitrateEntry = entry;

    this->sizeCurrentAU = 0;
    this->counterAU++;
    this->currentAUAllSlicesIntra = true;
    this->firstAUInDecodingOrder  = false;
    this->currentAUSliceTypes.clear();
    this->currentAUAssociatedSPS.reset();
  }
  if (this->lastFramePOC != this->curFramePOC)
    this->lastFramePOC = this->curFramePOC;
  this->sizeCurrentAU += data.size();

  if (nalHEVC->header.isSlice())
  {
    if (!currentSliceIntra)
      currentAUAllSlicesIntra = false;
    this->currentAUSliceTypes[currentSliceType]++;
  }

  if (nalRoot)
  {
    auto name = "NAL " + std::to_string(nalHEVC->nalIdx) + ": " +
                std::to_string(nalHEVC->header.nalUnitTypeID) + specificDescription;
    nalRoot->setProperties(name);
  }

  parseResult.success = true;
  return parseResult;
}

} // namespace parser
