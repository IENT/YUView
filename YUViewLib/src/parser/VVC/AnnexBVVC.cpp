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
#include "parser/common/Macros.h"
#include "parser/common/ReaderHelper.h"
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

using namespace vvc;

double AnnexBVVC::getFramerate() const { return DEFAULT_FRAMERATE; }

QSize AnnexBVVC::getSequenceSizeSamples() const { return QSize(352, 288); }

yuvPixelFormat AnnexBVVC::getPixelFormat() const { return yuvPixelFormat(Subsampling::YUV_420, 8); }

QList<QByteArray> AnnexBVVC::getSeekFrameParamerSets(int iFrameNr, uint64_t &filePos)
{
  Q_UNUSED(iFrameNr);
  Q_UNUSED(filePos);
  return {};
}

QByteArray AnnexBVVC::getExtradata() { return {}; }

IntPair AnnexBVVC::getProfileLevel() { return {0, 0}; }

Ratio AnnexBVVC::getSampleAspectRatio() { return Ratio({1, 1}); }

AnnexB::ParseResult
AnnexBVVC::parseAndAddNALUnit(int                                           nalID,
                              const ByteVector &                            data,
                              std::optional<BitratePlotModel::BitrateEntry> bitrateEntry,
                              std::optional<pairUint64>                     nalStartEndPosFile,
                              TreeItem *                                    parent)
{
  AnnexB::ParseResult parseResult;

  if (nalID == -1 && data.empty())
    return parseResult;

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
  TreeItem *nalRoot = nullptr;
  if (parent)
    nalRoot = new TreeItem(parent);
  else if (!packetModel->isNull())
    nalRoot = new TreeItem(packetModel->getRootItem());

  parseResult.success = true;

  AnnexB::logNALSize(data, nalRoot, nalStartEndPosFile);

  reader::SubByteReaderLogging reader(data, nalRoot, "", readOffset);

  std::string specificDescription;
  auto        nalVVC = std::make_shared<vvc::NalUnitVVC>(nalID, nalStartEndPosFile);
  try
  {
    nalVVC->header.parse(reader);

    if (nalVVC->header.nal_unit_type == NalType::VPS_NUT)
    {
      specificDescription = " VPS";
      auto newVPS         = std::make_shared<video_parameter_set_rbsp>();
      newVPS->parse(reader);

      this->activeParameterSets.vpsMap[newVPS->vps_video_parameter_set_id] = newVPS;

      specificDescription += " ID " + std::to_string(newVPS->vps_video_parameter_set_id);

      nalVVC->rbsp = newVPS;
    }
    else if (nalVVC->header.nal_unit_type == NalType::SPS_NUT)
    {
      specificDescription = " SPS";
      auto newSPS         = std::make_shared<seq_parameter_set_rbsp>();
      newSPS->parse(reader);

      this->activeParameterSets.spsMap[newSPS->sps_seq_parameter_set_id] = newSPS;

      specificDescription += " ID " + std::to_string(newSPS->sps_seq_parameter_set_id);

      nalVVC->rbsp = newSPS;
    }
    else if (nalVVC->header.nal_unit_type == NalType::PPS_NUT)
    {
      specificDescription = " PPS";
      auto newPPS         = std::make_shared<pic_parameter_set_rbsp>();
      newPPS->parse(reader, this->activeParameterSets.spsMap);

      this->activeParameterSets.ppsMap[newPPS->pps_pic_parameter_set_id] = newPPS;

      specificDescription += " ID " + std::to_string(newPPS->pps_pic_parameter_set_id);

      nalVVC->rbsp = newPPS;
    }
    else if (nalVVC->header.nal_unit_type == NalType::PREFIX_APS_NUT ||
             nalVVC->header.nal_unit_type == NalType::SUFFIX_APS_NUT)
    {
      specificDescription = " APS";
      auto newAPS         = std::make_shared<adaptation_parameter_set_rbsp>();
      newAPS->parse(reader);

      this->activeParameterSets.apsMap[newAPS->aps_adaptation_parameter_set_id] = newAPS;

      specificDescription += " ID " + std::to_string(newAPS->aps_adaptation_parameter_set_id);

      nalVVC->rbsp = newAPS;
    }
    else if (nalVVC->header.nal_unit_type == NalType::PH_NUT)
    {
      specificDescription   = " Picture Header";
      auto newPictureHeader = std::make_shared<picture_header_rbsp>();
      newPictureHeader->parse(reader,
                              this->activeParameterSets.vpsMap,
                              this->activeParameterSets.spsMap,
                              this->activeParameterSets.ppsMap,
                              this->parsingState.currentSlice);
      newPictureHeader->picture_header_structure_instance->calculatePictureOrderCount(
          reader,
          nalVVC->header.nal_unit_type,
          this->activeParameterSets.spsMap,
          this->activeParameterSets.ppsMap,
          parsingState.currentPictureHeaderStructure);

      if (this->parsingState.currentPictureHeaderStructure)
        this->parsingState.lastFramePOC =
            this->parsingState.currentPictureHeaderStructure->PicOrderCntVal;

      this->parsingState.currentPictureHeaderStructure =
          newPictureHeader->picture_header_structure_instance;

      specificDescription +=
          " POC " +
          std::to_string(newPictureHeader->picture_header_structure_instance->PicOrderCntVal);

      nalVVC->rbsp = newPictureHeader;
    }
    else if (nalVVC->header.nal_unit_type == NalType::TRAIL_NUT ||
             nalVVC->header.nal_unit_type == NalType::STSA_NUT ||
             nalVVC->header.nal_unit_type == NalType::RADL_NUT ||
             nalVVC->header.nal_unit_type == NalType::RASL_NUT ||
             nalVVC->header.nal_unit_type == NalType::IDR_W_RADL ||
             nalVVC->header.nal_unit_type == NalType::IDR_N_LP ||
             nalVVC->header.nal_unit_type == NalType::CRA_NUT ||
             nalVVC->header.nal_unit_type == NalType::GDR_NUT)
    {
      specificDescription = " Slice Header";
      auto newSliceLayer  = std::make_shared<slice_layer_rbsp>();
      newSliceLayer->parse(reader,
                           nalVVC->header.nal_unit_type,
                           this->activeParameterSets.vpsMap,
                           this->activeParameterSets.spsMap,
                           this->activeParameterSets.ppsMap,
                           this->parsingState.currentPictureHeaderStructure);

      this->parsingState.currentSlice = newSliceLayer;
      if (newSliceLayer->slice_header_instance.picture_header_structure_instance)
      {
        newSliceLayer->slice_header_instance.picture_header_structure_instance
            ->calculatePictureOrderCount(reader,
                                         nalVVC->header.nal_unit_type,
                                         this->activeParameterSets.spsMap,
                                         this->activeParameterSets.ppsMap,
                                         parsingState.currentPictureHeaderStructure);
        this->parsingState.currentPictureHeaderStructure =
            newSliceLayer->slice_header_instance.picture_header_structure_instance;
        this->parsingState.lastFramePOC =
            this->parsingState.currentPictureHeaderStructure->PicOrderCntVal;
      }

      specificDescription +=
          " POC " +
          std::to_string(this->parsingState.currentPictureHeaderStructure->PicOrderCntVal);
      specificDescription +=
          " " + to_string(newSliceLayer->slice_header_instance.sh_slice_type) + "-Slice";

      nalVVC->rbsp = newSliceLayer;
    }
    else if (nalVVC->header.nal_unit_type == NalType::AUD_NUT)
    {
      specificDescription = " AUD";
      auto newAUD         = std::make_shared<access_unit_delimiter_rbsp>();
      newAUD->parse(reader);
      nalVVC->rbsp = newAUD;
    }
    else if (nalVVC->header.nal_unit_type == NalType::DCI_NUT)
    {
      specificDescription = " DCI";
      auto newDCI         = std::make_shared<decoding_capability_information_rbsp>();
      newDCI->parse(reader);
      nalVVC->rbsp = newDCI;
    }
    else if (nalVVC->header.nal_unit_type == NalType::EOB_NUT)
    {
      specificDescription = " EOB";
      auto newEOB         = std::make_shared<end_of_bitstream_rbsp>();
      nalVVC->rbsp        = newEOB;
    }
    else if (nalVVC->header.nal_unit_type == NalType::EOS_NUT)
    {
      specificDescription = " EOS";
      auto newEOS         = std::make_shared<end_of_seq_rbsp>();
      nalVVC->rbsp        = newEOS;
    }
    else if (nalVVC->header.nal_unit_type == NalType::FD_NUT)
    {
      specificDescription = " Filler Data";
      auto newFillerData  = std::make_shared<filler_data_rbsp>();
      newFillerData->parse(reader);
      nalVVC->rbsp = newFillerData;
    }
    else if (nalVVC->header.nal_unit_type == NalType::FD_NUT)
    {
      specificDescription = " OPI";
      auto newOPI         = std::make_shared<operating_point_information_rbsp>();
      newOPI->parse(reader);
      nalVVC->rbsp = newOPI;
    }
    else if (nalVVC->header.nal_unit_type == NalType::SUFFIX_SEI_NUT ||
             nalVVC->header.nal_unit_type == NalType::PREFIX_APS_NUT)
    {
      specificDescription = " SEI";
      auto newSEI         = std::make_shared<sei_message>();
      newSEI->parse(reader,
                    nalVVC->header.nal_unit_type,
                    nalVVC->header.nuh_temporal_id_plus1 - 1,
                    this->parsingState.lastBufferingPeriod);

      if (newSEI->payloadType == 0)
      {
        this->parsingState.lastBufferingPeriod =
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

  // TODO: Add bitrate plotting
  (void)bitrateEntry;

  if (auDelimiterDetector.isStartOfNewAU(nalVVC, this->parsingState.currentPictureHeaderStructure))
  {
    DEBUG_VVC("Start of new AU. Adding bitrate " << this->parsingState.sizeCurrentAU);

    BitratePlotModel::BitrateEntry entry;
    if (bitrateEntry)
    {
      entry.pts      = bitrateEntry->pts;
      entry.dts      = bitrateEntry->dts;
      entry.duration = bitrateEntry->duration;
    }
    else
    {
      entry.pts      = this->parsingState.lastFramePOC;
      entry.dts      = int(this->parsingState.counterAU);
      entry.duration = 1;
    }
    entry.bitrate            = unsigned(this->parsingState.sizeCurrentAU);
    entry.keyframe           = this->parsingState.lastFrameIsKeyframe;
    parseResult.bitrateEntry = entry;

    if (this->parsingState.counterAU > 0)
    {
      if (!addFrameToList(this->parsingState.lastFramePOC,
                          this->parsingState.curFrameFileStartEndPos,
                          this->parsingState.lastFrameIsKeyframe))
      {
        ReaderHelper::addErrorMessageChildItem(QString("Error adding frame to frame list."),
                                               parent);
        return parseResult;
      }
      if (this->parsingState.curFrameFileStartEndPos)
        DEBUG_VVC("Adding start/end " << this->curFrameFileStartEndPos->first << "/"
                                      << this->curFrameFileStartEndPos->second << " - POC "
                                      << this->parsingState.counterAU
                                      << (this->parsingState.lastFrameIsKeyframe ? " - ra" : ""));
      else
        DEBUG_VVC("Adding start/end %d/%d - POC NA/NA"
                  << (this->parsingState.lastFrameIsKeyframe ? " - ra" : ""));
    }
    this->parsingState.curFrameFileStartEndPos = nalStartEndPosFile;
    this->parsingState.sizeCurrentAU           = 0;
    this->parsingState.counterAU++;
  }
  else if (this->parsingState.curFrameFileStartEndPos && nalStartEndPosFile)
    this->parsingState.curFrameFileStartEndPos->second = nalStartEndPosFile->second;

  this->parsingState.sizeCurrentAU += data.size();
  this->parsingState.lastFrameIsKeyframe = (nalVVC->header.nal_unit_type == NalType::IDR_W_RADL ||
                                            nalVVC->header.nal_unit_type == NalType::IDR_N_LP ||
                                            nalVVC->header.nal_unit_type == NalType::CRA_NUT);

  if (nalRoot)
  {
    auto name = "NAL " + std::to_string(nalVVC->nalIdx) + ": " +
                std::to_string(nalVVC->header.nalUnitTypeID) + specificDescription;
    nalRoot->setProperties(name);
  }

  return parseResult;
}

// 7.4.2.4.3
bool AnnexBVVC::auDelimiterDetector_t::isStartOfNewAU(
    std::shared_ptr<vvc::NalUnitVVC> nal, std::shared_ptr<vvc::picture_header_structure> ph)
{
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
