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

#include "Subtitles/AnnexBItuTT35.h"
#include "common/Macros.h"
#include "common/SubByteReaderLogging.h"

#define PARSER_AVC_DEBUG_OUTPUT 0
#if PARSER_AVC_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_AVC(msg) qDebug() << msg
#else
#define DEBUG_AVC(fmt) ((void)0)
#endif

namespace parser
{

double AnnexBAVC::getFramerate() const
{
  // Find the first SPS and return the framerate (if signaled)
  for (auto nal : nalUnitList)
  {
    // This should be an hevc nal
    auto nal_avc = nal.dynamicCast<nal_unit_avc>();

    if (nal_avc->nal_unit_type == SPS)
    {
      auto s = nal.dynamicCast<sps>();
      if (s->vui_parameters_present_flag && s->vui_parameters.timing_info_present_flag)
      return s->vui_parameters.frameRate;
    }
  }

  return 0.0;
}

QSize AnnexBAVC::getSequenceSizeSamples() const
{
  // Find the first SPS and return the size
  for (auto nal : nalUnitList)
  {
    // This should be an hevc nal
    auto nal_avc = nal.dynamicCast<nal_unit_avc>();

    if (nal_avc->nal_unit_type == SPS)
    {
      auto s = nal.dynamicCast<sps>();
      // We need the decoder picture size and the frame_crop parameters
      int w = s->PicWidthInSamplesL;
      int h = s->PicHeightInSamplesL;
      if (s->frame_cropping_flag)
      {
        w -= (s->CropUnitX * s->frame_crop_right_offset) + (s->CropUnitX * s->frame_crop_left_offset);
        h -= (s->CropUnitY * s->frame_crop_bottom_offset) + (s->CropUnitY * s->frame_crop_top_offset);
      }

      return QSize(w, h);
    }
  }

  return QSize(-1,-1);
}

yuvPixelFormat AnnexBAVC::getPixelFormat() const
{
  // Get the subsampling and bit-depth from the sps
  int bitDepthY = -1;
  int bitDepthC = -1;
  auto subsampling = Subsampling::UNKNOWN;
  for (auto nal : nalUnitList)
  {
    // This should be an hevc nal
    auto nal_avc = nal.dynamicCast<nal_unit_avc>();

    if (nal_avc->nal_unit_type == SPS)
    {
      auto s = nal_avc.dynamicCast<sps>();
      if (s->chroma_format_idc == 0)
        subsampling = Subsampling::YUV_400;
      else if (s->chroma_format_idc == 1)
        subsampling = Subsampling::YUV_420;
      else if (s->chroma_format_idc == 2)
        subsampling = Subsampling::YUV_422;
      else if (s->chroma_format_idc == 3)
        subsampling = Subsampling::YUV_444;

      bitDepthY = s->bit_depth_luma_minus8 + 8;
      bitDepthC = s->bit_depth_chroma_minus8 + 8;
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

AnnexB::ParseResult AnnexBAVC::parseAndAddNALUnit(int nalID, QByteArray data, std::optional<BitratePlotModel::BitrateEntry> bitrateEntry, std::optional<pairUint64> nalStartEndPosFile, TreeItem *parent)
{
  AnnexB::ParseResult parseResult;

  if (nalID == -1 && data.isEmpty())
  {
    if (curFramePOC != -1)
    {
      // Save the info of the last frame
      if (!addFrameToList(curFramePOC, curFrameFileStartEndPos, curFrameIsRandomAccess))
      {
        ReaderHelper::addErrorMessageChildItem(QString("Error - POC %1 alread in the POC list.").arg(curFramePOC), parent);
        return parseResult;
      }
      if (curFrameFileStartEndPos)
        DEBUG_AVC("AnnexBAVC::parseAndAddNALUnit Adding start/end " << curFrameFileStartEndPos->first << "/" << curFrameFileStartEndPos->second << " - POC " << curFramePOC << (curFrameIsRandomAccess ? " - ra" : ""));
      else
        DEBUG_AVC("AnnexBAVC::parseAndAddNALUnit Adding start/end NA/NA - POC " << curFramePOC << (curFrameIsRandomAccess ? " - ra" : ""));
    }
    // The file ended
    std::sort(POCList.begin(), POCList.end());
    hrd.endOfFile(this->getHRDPlotModel());
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

  // Read ony byte (the NAL header)
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

  AnnexB::logNALSize(data, nalRoot, nalStartEndPosFile);

  // Create a nal_unit and read the header
  nal_unit_avc nal_avc(nalID, nalStartEndPosFile);
  if (!nal_avc.parseNalUnitHeader(nalHeaderBytes, nalRoot))
    return parseResult;

  if (nal_avc.isSlice())
  {
    // Reparse the SEI messages that we could not parse so far. This is a slice so all parameter sets should be available now.
    while (!reparse_sei.empty())
    {
      auto sei = reparse_sei.front();
      reparse_sei.pop_front();
      if (sei->payloadType == 0)
      {
        auto buffering_period = sei.dynamicCast<buffering_period_sei>();
        buffering_period->reparse_buffering_period_sei(this->active_SPS_list);
      }
      if (sei->payloadType == 1)
      {
        auto pic_timing = sei.dynamicCast<pic_timing_sei>();
        pic_timing->reparse_pic_timing_sei(this->active_SPS_list, CpbDpbDelaysPresentFlag);
      }
    }
  }

  bool parsingSuccess = true;
  bool currentSliceIntra = false;
  QString currentSliceType;
  if (nal_avc.nal_unit_type == SPS)
  {
    // A sequence parameter set
    auto new_sps = QSharedPointer<sps>(new sps(nal_avc));
    parsingSuccess = new_sps->parse_sps(payload, nalRoot);

    // Add sps (replace old one if existed)
    this->active_SPS_list.insert(new_sps->seq_parameter_set_id, new_sps);

    // Also add sps to list of all nals
    nalUnitList.append(new_sps);

    // Add the SPS ID
    specificDescription = parsingSuccess ? QString(" SPS_NUT ID %1").arg(new_sps->seq_parameter_set_id) : " SPS_NUT ERR";
    parseResult.nalTypeName = "SPS(" +  (parsingSuccess ? std::to_string(new_sps->seq_parameter_set_id) : "ERR") + ")";
    
    if (new_sps->vui_parameters.nal_hrd_parameters_present_flag || new_sps->vui_parameters.vcl_hrd_parameters_present_flag)
      CpbDpbDelaysPresentFlag = true;

    if (new_sps->vui_parameters_present_flag && new_sps->vui_parameters.nal_hrd_parameters_present_flag)
    {
      this->getHRDPlotModel()->setCPBBufferSize(new_sps->vui_parameters.nal_hrd.CpbSize[0]);
    }

    DEBUG_AVC("AnnexBAVC::parseAndAddNALUnit Parse SPS ID " << new_sps->seq_parameter_set_id);
  }
  else if (nal_avc.nal_unit_type == PPS) 
  {
    // A picture parameter set
    auto new_pps = QSharedPointer<pps>(new pps(nal_avc));
    parsingSuccess = new_pps->parse_pps(payload, nalRoot, this->active_SPS_list);

    // Add pps (replace old one if existed)
    active_PPS_list.insert(new_pps->pic_parameter_set_id, new_pps);

    // Also add pps to list of all nals
    nalUnitList.append(new_pps);

    // Add the PPS ID
    specificDescription = parsingSuccess ? QString(" PPS_NUT ID %1").arg(new_pps->pic_parameter_set_id) : "PPS_NUT ERR";
    parseResult.nalTypeName = "PPS(" +  (parsingSuccess ? std::to_string(new_pps->pic_parameter_set_id) : "ERR") + ")";

    DEBUG_AVC("AnnexBAVC::parseAndAddNALUnit Parse PPS ID " << new_pps->pic_parameter_set_id);
  }
  else if (nal_avc.isSlice())
  {
    // Create a new slice unit
    auto new_slice = QSharedPointer<slice_header>(new slice_header(nal_avc));
    parsingSuccess = new_slice->parse_slice_header(payload, this->active_SPS_list, active_PPS_list, last_picture_first_slice, nalRoot);

    if (parsingSuccess && !new_slice->bottom_field_flag && 
      (last_picture_first_slice.isNull() || new_slice->TopFieldOrderCnt != last_picture_first_slice->TopFieldOrderCnt || new_slice->isRandomAccess()) &&
      new_slice->first_mb_in_slice == 0)
      last_picture_first_slice = new_slice;

    // Add the POC of the slice
    specificDescription = parsingSuccess ? QString(" POC %1").arg(new_slice->globalPOC) : " ERR";
    parseResult.nalTypeName = "Slice(" +  (parsingSuccess ? ("POC " + std::to_string(new_slice->globalPOC)) : "ERR") + ")";

    if (parsingSuccess)
    {
      if (new_slice->first_mb_in_slice == 0)
      {
        // This slice NAL is the start of a new frame
        if (curFramePOC != -1)
        {
          // Save the info of the last frame
          if (!addFrameToList(curFramePOC, curFrameFileStartEndPos, curFrameIsRandomAccess))
          {
            ReaderHelper::addErrorMessageChildItem(QString("Error - POC %1 alread in the POC list.").arg(curFramePOC), nalRoot);
            return parseResult;
          }
          if (curFrameFileStartEndPos)
            DEBUG_AVC("AnnexBAVC::parseAndAddNALUnit Adding start/end " << curFrameFileStartEndPos->first << "/" << curFrameFileStartEndPos->second << " - POC " << curFramePOC << (curFrameIsRandomAccess ? " - ra" : ""));
          else
            DEBUG_AVC("AnnexBAVC::parseAndAddNALUnit Adding start/end NA/NA - POC " << curFramePOC << (curFrameIsRandomAccess ? " - ra" : ""));
        }
        curFrameFileStartEndPos = nalStartEndPosFile;
        curFramePOC = new_slice->globalPOC;
        curFrameIsRandomAccess = new_slice->isRandomAccess();
      }
      else if (curFrameFileStartEndPos && nalStartEndPosFile)
        // Another slice NAL which belongs to the last frame
        // Update the end position
        curFrameFileStartEndPos->second = nalStartEndPosFile->second;

      if (new_slice->isRandomAccess() && new_slice->first_mb_in_slice == 0)
      {
        // This is the first slice of a random access point. Add it to the list.
        nalUnitList.append(new_slice);
      }

      currentSliceIntra = new_slice->isRandomAccess();
      currentSliceType = new_slice->getSliceTypeString();

      DEBUG_AVC("AnnexBAVC::parseAndAddNALUnit Parsed Slice POC " << new_slice->globalPOC);
    }
    else
    {
      DEBUG_AVC("AnnexBAVC::parseAndAddNALUnit Parsed Slice Error");
  }
  }
  else if (nal_avc.nal_unit_type == SEI)
  {
    // An SEI. Each sei_rbsp may contain multiple sei_message
    auto new_sei = QSharedPointer<sei>(new sei(nal_avc));
    QByteArray sei_data = payload;

    int sei_count = 0;
    while(!sei_data.isEmpty())
    {
      TreeItem *const message_tree = nalRoot ? new TreeItem(nalRoot) : nullptr;
      
      // Parse the SEI header and remove it from the data array
      int nrBytes = new_sei->parse_sei_header(sei_data, message_tree);
      if (nrBytes == -1)
        return parseResult;
      sei_data.remove(0, nrBytes);

      if (message_tree)
        message_tree->itemData[0] = QString("sei_message %1 - %2").arg(sei_count).arg(new_sei->payloadTypeName);

      // The real number of bytes to read from the bitstream may be higher than the indicated payload size (emulation prevention)
      int realPayloadSize = determineRealNumberOfBytesSEIEmulationPrevention(sei_data, new_sei->payloadSize);
      QByteArray sub_sei_data = sei_data.mid(0, realPayloadSize);

      parsingSuccess = true;
      sei_parsing_return_t result;
      QSharedPointer<sei> reparse;
      if (new_sei->payloadType == 0)
      {
        auto new_buffering_period_sei = QSharedPointer<buffering_period_sei>(new buffering_period_sei(new_sei));
        result = new_buffering_period_sei->parse_buffering_period_sei(sub_sei_data, this->active_SPS_list, message_tree);
        reparse = new_buffering_period_sei;
        if (this->lastBufferingPeriodSEI.isNull())
          this->lastBufferingPeriodSEI = new_buffering_period_sei;
        else
          this->newBufferingPeriodSEI = new_buffering_period_sei;
        this->nextAUIsFirstAUInBufferingPeriod = true;
      }
      else if (new_sei->payloadType == 1)
      {
        auto new_pic_timing_sei = QSharedPointer<pic_timing_sei>(new pic_timing_sei(new_sei));
        result = new_pic_timing_sei->parse_pic_timing_sei(sub_sei_data, this->active_SPS_list, CpbDpbDelaysPresentFlag, message_tree);
        reparse = new_pic_timing_sei;
        if (this->lastPicTimingSEI.isNull())
          this->lastPicTimingSEI = new_pic_timing_sei;
        else
          this->newPicTimingSEI = new_pic_timing_sei;
      }
      else if (new_sei->payloadType == 4)
      {
        try
        {
          auto data = reader::SubByteReaderLogging::convertToByteVector(sub_sei_data);
          subtitle::itutt35::parse_user_data_registered_itu_t_t35(data, message_tree);
          result = SEI_PARSING_OK;
        }
        catch(...)
        {
          result = SEI_PARSING_ERROR;
        }
      }
      else if (new_sei->payloadType == 5)
      {
        auto new_user_data_sei = QSharedPointer<user_data_sei>(new user_data_sei(new_sei));
        result = new_user_data_sei->parse_user_data_sei(sub_sei_data, message_tree);
      }
      else
        // The default parser just logs the raw bytes
        result = new_sei->parser_sei_bytes(sub_sei_data, message_tree);
      
      if (result == SEI_PARSING_WAIT_FOR_PARAMETER_SETS)
        reparse_sei.append(reparse);
      if (result == SEI_PARSING_ERROR)
        parsingSuccess = false;

      // Remove the sei payload bytes from the data
      sei_data.remove(0, realPayloadSize);
      if (sei_data.length() == 1)
      {
        // This should be the rspb trailing bits (10000000)
        sei_data.remove(0, 1);
      }

      sei_count++;
    }

    specificDescription = parsingSuccess ? QString(" (#%1)").arg(sei_count) : QString(" (#%1-ERR)").arg(sei_count);
    parseResult.nalTypeName = "SEI(" + (parsingSuccess ? ("#" + std::to_string(sei_count)) : "ERR") + ")";

    DEBUG_AVC("AnnexBAVC::parseAndAddNALUnit Parsed SEI (" << sei_count << " messages)");
  }
  else if (nal_avc.nal_unit_type == FILLER)
  {
    parseResult.nalTypeName = "FILLER";
    DEBUG_AVC("AnnexBAVC::parseAndAddNALUnit Parsed Filler data");
  }
  else if (nal_avc.nal_unit_type == AUD)
  {
    parseResult.nalTypeName = "AUD";
    DEBUG_AVC("AnnexBAVC::parseAndAddNALUnit Parsed Access Unit Delimiter (AUD)");
  }

  if (auDelimiterDetector.isStartOfNewAU(nal_avc, curFramePOC))
  {
    if (this->sizeCurrentAU > 0)
    {
      DEBUG_AVC("AnnexBAVC::parseAndAddNALUnit Start of new AU. Adding bitrate " << this->sizeCurrentAU);

      BitratePlotModel::BitrateEntry entry;
      if (bitrateEntry)
      {
        entry.pts = bitrateEntry->pts;
        entry.dts = bitrateEntry->dts;
        entry.duration = bitrateEntry->duration;
      }
      else
      {
        entry.pts = this->lastFramePOC;
        entry.dts = this->counterAU;
        entry.duration = 1;
      }
      entry.bitrate = this->sizeCurrentAU;
      entry.keyframe = this->currentAUAllSlicesIntra;
      entry.frameType = Base::convertSliceTypeMapToString(this->currentAUSliceTypes);
      parseResult.bitrateEntry = entry;
      if (this->active_SPS_list.size() > 0)
        this->hrd.addAU(this->sizeCurrentAU * 8, curFramePOC, this->active_SPS_list[0], this->lastBufferingPeriodSEI, this->lastPicTimingSEI, this->getHRDPlotModel());
    }
    this->sizeCurrentAU = 0;
    this->counterAU++;
    this->currentAUAllSlicesIntra = true;
    this->currentAUSliceTypes.clear();
  }
  if (this->newBufferingPeriodSEI)
    this->lastBufferingPeriodSEI = this->newBufferingPeriodSEI;
  if (this->newPicTimingSEI)
    this->lastPicTimingSEI = this->newPicTimingSEI;
  if (this->lastFramePOC != curFramePOC)
    this->lastFramePOC = curFramePOC;
  this->sizeCurrentAU += data.size();

  if (this->nextAUIsFirstAUInBufferingPeriod)
  {
    if (this->counterAU > 0)
      this->hrd.isFirstAUInBufferingPeriod = true;
    this->nextAUIsFirstAUInBufferingPeriod = false;
  }

  if (nal_avc.isSlice())
  {
    if (!currentSliceIntra)
      this->currentAUAllSlicesIntra = false;
    this->currentAUSliceTypes[currentSliceType]++;
  }

  if (nalRoot)
  {
    // Set a useful name of the TreeItem (the root for this NAL)
    nalRoot->itemData.append(QString("NAL %1: %2").arg(nal_avc.nalIdx).arg(nal_unit_type_toString.value(nal_avc.nal_unit_type)) + specificDescription);
    nalRoot->setError(!parsingSuccess);
  }

  parseResult.success = true;
  return parseResult;
}

QList<QByteArray> AnnexBAVC::getSeekFrameParamerSets(int iFrameNr, uint64_t &filePos)
{
  // Get the POC for the frame number
  int seekPOC = POCList[iFrameNr];

  // Collect the active parameter sets
  sps_map active_SPS_list;
  pps_map active_PPS_list;

  for (auto nal : nalUnitList)
  {
    // This should be an hevc nal
    auto nal_avc = nal.dynamicCast<nal_unit_avc>();

    if (nal_avc->isSlice()) 
    {
      // We can cast this to a slice.
      auto s = nal_avc.dynamicCast<slice_header>();

      if (s->globalPOC == seekPOC)
      {
        // Seek here
        if (s->filePosStartEnd)
          filePos = s->filePosStartEnd->first;

        // Get the bitstream of all active parameter sets
        QList<QByteArray> paramSets;

        for (auto s : this->active_SPS_list)
          paramSets.append(s->getRawNALData());
        for (auto p : active_PPS_list)
          paramSets.append(p->getRawNALData());

        return paramSets;
      }
    }
    else if (nal_avc->nal_unit_type == SPS) 
    {
      // Add sps (replace old one if existed)
      auto s = nal_avc.dynamicCast<sps>();
      this->active_SPS_list.insert(s->seq_parameter_set_id, s);
    }
    else if (nal_avc->nal_unit_type == PPS)
    {
      // Add pps (replace old one if existed)
      auto p = nal_avc.dynamicCast<pps>();
      active_PPS_list.insert(p->pic_parameter_set_id, p);
    }
  }

  return QList<QByteArray>();
}

QByteArray AnnexBAVC::getExtradata()
{
  // Convert the SPS and PPS that we found in the bitstream to the libavformat avcc format (see avc.c)
  QByteArray e;
  e += 1; /* version */

  // Get the first SPS and PPS
  QSharedPointer<sps> s;
  QSharedPointer<pps> p;
  for (auto nal : nalUnitList)
  {
    // This should be an hevc nal
    auto nal_avc = nal.dynamicCast<nal_unit_avc>();

    if (nal_avc->nal_unit_type == SPS)
      s = nal.dynamicCast<sps>();
    if (nal_avc->nal_unit_type == PPS)
      p = nal.dynamicCast<pps>();

    if (s && p)
      break;
  }

  if (!s || !p)
    // SPS or PPS not found
    return QByteArray();

  // Add profile, compatibility and level
  int profile_compat = 0;
  if (s->constraint_set0_flag)
    profile_compat += 128;
  if (s->constraint_set1_flag)
    profile_compat += 64;
  if (s->constraint_set2_flag)
    profile_compat += 32;
  if (s->constraint_set3_flag)
    profile_compat += 16;
  if (s->constraint_set4_flag)
    profile_compat += 8;
  if (s->constraint_set5_flag)
    profile_compat += 4;

  e += s->profile_idc;   /* profile */
  e += profile_compat;   /* profile compat */
  e += s->level_idc;     /* level */

  e += (unsigned char)255; /* 6 bits reserved (111111) + 2 bits nal size length - 1 (11) */
  e += (unsigned char)225; /* 3 bits reserved (111) + 5 bits number of sps (00001) */

  // Write SPS
  QByteArray sps_payload = s->nalPayload;
  while (sps_payload.at(sps_payload.length()-1) == 0)
    sps_payload.chop(1);
  e += (sps_payload.length() + 1) >> 8;
  e += (sps_payload.length() + 1) & 0xff;
  e += s->getNALHeader();
  e += sps_payload;

  // Write PPS
  e += 0x01; /* number of pps */
  QByteArray pps_payload = p->nalPayload;
  while (pps_payload.at(pps_payload.length()-1) == 0)
    pps_payload.chop(1);
  e += (pps_payload.length() + 1) >> 8;
  e += (pps_payload.length() + 1) & 0xff;
  e += p->getNALHeader();
  e += pps_payload;

  return e;
}

IntPair AnnexBAVC::getProfileLevel()
{
  for (auto nal : nalUnitList)
  {
    // This should be an hevc nal
    auto nal_avc = nal.dynamicCast<nal_unit_avc>();

    if (nal_avc->nal_unit_type == SPS)
    {
      auto s = nal.dynamicCast<sps>();
      return {s->profile_idc, s->level_idc};
    }
  }
  return {};
}

Ratio AnnexBAVC::getSampleAspectRatio()
{
  for (auto nal : nalUnitList)
  {
    // This should be an hevc nal
    auto nal_avc = nal.dynamicCast<nal_unit_avc>();

    if (nal_avc->nal_unit_type == SPS)
    {
      auto s = nal.dynamicCast<sps>();
      if (s->vui_parameters_present_flag && s->vui_parameters.aspect_ratio_info_present_flag)
      {
        int aspect_ratio_idc = s->vui_parameters.aspect_ratio_idc;
        if (aspect_ratio_idc > 0 && aspect_ratio_idc <= 16)
        {
          int widths[] = {1, 12, 10, 16, 40, 24, 20, 32, 80, 18, 15, 64, 160, 4, 3, 2};
          int heights[] = {1, 11, 11, 11, 33, 11, 11, 11, 33, 11, 11, 33, 99, 3, 2, 1};

          const int i = aspect_ratio_idc - 1;
          return Ratio({widths[i], heights[i]});
        }
        if (aspect_ratio_idc == 255)
          return Ratio({int(s->vui_parameters.sar_width), int(s->vui_parameters.sar_height)});
      }
    }
  }
  return Ratio({1, 1});
}

int AnnexBAVC::determineRealNumberOfBytesSEIEmulationPrevention(QByteArray &in, int nrBytes)
{
  if (in.length() <= 0)
    return 0;

  int nrZeroBytes = 0;
  int pos = 0;
  while (nrBytes > 0 && pos < in.length())
  {
    char c = (char)in.at(pos);

    if (nrZeroBytes == 2 && c == 3)
      // Emulation prevention
      nrZeroBytes = 0;
    else
    {
      if (c == 0)
        nrZeroBytes++;
      else
        nrZeroBytes = 0;
      nrBytes--;
    }

    pos++;
  }

  return pos;
}

bool AnnexBAVC::auDelimiterDetector_t::isStartOfNewAU(nal_unit_avc &nal_avc, int curFramePOC)
{
  const bool isSlice = (nal_avc.nal_unit_type == CODED_SLICE_NON_IDR || nal_avc.nal_unit_type == CODED_SLICE_IDR);
  
  // 7.4.1.2.3 Order of NAL units and coded pictures and association to access units
  bool isStart = false;
  if (this->primaryCodedPictureInAuEncountered)
  {
    // The first of any of the following NAL units after the last VCL NAL unit of a primary coded picture specifies the start of a new access unit:
    if (nal_avc.nal_unit_type == AUD)
      isStart = true;

    if (nal_avc.nal_unit_type == SPS || nal_avc.nal_unit_type == PPS)
      isStart = true;

    if (nal_avc.nal_unit_type == SEI)
      isStart = true;

    const bool fourteenToEigtheen = (nal_avc.nal_unit_type == PREFIX_NAL ||
                                     nal_avc.nal_unit_type == SUBSET_SPS ||
                                     nal_avc.nal_unit_type == DEPTH_PARAMETER_SET ||
                                     nal_avc.nal_unit_type == RESERVED_17 ||
                                     nal_avc.nal_unit_type == RESERVED_18);
    if (fourteenToEigtheen)
      isStart = true;

    if (isSlice && curFramePOC != this->lastSlicePoc)
    {
      isStart = true;
      this->lastSlicePoc = curFramePOC;
    }
  }
  
  if (isSlice)
    this->primaryCodedPictureInAuEncountered = true;

  if (isStart && !isSlice)
    this->primaryCodedPictureInAuEncountered = false;

  return isStart;
}

void AnnexBAVC::HRD::addAU(unsigned auBits, unsigned poc, QSharedPointer<sps> const &sps, QSharedPointer<buffering_period_sei> const &lastBufferingPeriodSEI, QSharedPointer<pic_timing_sei> const &lastPicTimingSEI, HRDPlotModel *plotModel)
{
  Q_ASSERT(plotModel != nullptr);
  if (!sps->vui_parameters_present_flag || !sps->vui_parameters.nal_hrd_parameters_present_flag)
    return;

  // There could be multiple but we currently only support one.
  const auto SchedSelIdx = 0;

  const auto initial_cpb_removal_delay = lastBufferingPeriodSEI->initial_cpb_removal_delay[SchedSelIdx];
  const auto initial_cpb_removal_delay_offset = lastBufferingPeriodSEI->initial_cpb_removal_delay_offset[SchedSelIdx];

  const bool cbr_flag = sps->vui_parameters.nal_hrd.cbr_flag[SchedSelIdx];

  // TODO: Investigate the difference between our results and the results from stream-Eye
  // I noticed that the results from stream eye differ by a (seemingly random) additional
  // number of bits that stream eye counts for au 0.
  // For one example I investigated it looked like the parameter sets (SPS + PPS) were counted twice for the
  // first AU.
  // if (this->au_n == 0)
  // {
  //   auBits += 49 * 8;
  // }

  /* Some notation:
    t_ai: The time at which the first bit of access unit n begins to enter the CPB is
    referred to as the initial arrival time. t_r_nominal_n: the nominal removal time of the
    access unit from the CPB t_r_n: The removal time of access unit n
  */

  // Annex C: The variable t c is derived as follows and is called a clock tick:
  time_t t_c = time_t(sps->vui_parameters.num_units_in_tick) / sps->vui_parameters.time_scale;

  // ITU-T Rec H.264 (04/2017) - C.1.2 - Timing of coded picture removal
  // Part 1: the nominal removal time of the access unit from the CPB
  time_t t_r_nominal_n;
  if (this->au_n == 0)
    t_r_nominal_n = time_t(initial_cpb_removal_delay) / 90000;
  else
  {
    // n is not equal to 0. The removal time depends on the removal time of the previous AU.
    const auto cpb_removal_delay = unsigned(lastPicTimingSEI->cpb_removal_delay);
    t_r_nominal_n = this->t_r_nominal_n_first + t_c * cpb_removal_delay;
  }

  if (this->isFirstAUInBufferingPeriod)
    this->t_r_nominal_n_first = t_r_nominal_n;

  // ITU-T Rec H.264 (04/2017) - C.1.1 - Timing of bitstream arrival
  time_t t_ai;
  if (this->au_n == 0)
    t_ai = 0;
  else
  {
    if (cbr_flag)
      t_ai = this->t_af_nm1;
    else
    {
      time_t t_ai_earliest = 0;
      time_t init_delay;
      if (this->isFirstAUInBufferingPeriod)
        init_delay = time_t(initial_cpb_removal_delay) / 90000;
      else
        init_delay = time_t(initial_cpb_removal_delay + initial_cpb_removal_delay_offset) / 90000;
      t_ai_earliest = t_r_nominal_n - init_delay;
      t_ai = std::max(this->t_af_nm1, t_ai_earliest);
    }
  }

  // The final arrival time for access unit n is derived by:
  const int bitrate = sps->vui_parameters.nal_hrd.BitRate[SchedSelIdx];
  time_t t_af       = t_ai + time_t(auBits) / bitrate;

  // ITU-T Rec H.264 (04/2017) - C.1.2 - Timing of coded picture removal
  // Part 2: The removal time of access unit n is specified as follows:
  time_t t_r_n;
  if (!sps->vui_parameters.low_delay_hrd_flag || t_r_nominal_n >= t_af)
    t_r_n = t_r_nominal_n;
  else
    // NOTE: This indicates that the size of access unit n, b(n), is so large that it
    // prevents removal at the nominal removal time.
    t_r_n = t_r_nominal_n * t_c * (t_af - t_r_nominal_n) / t_c;

  // C.3 - Bitstream conformance
  // 1.
  if (this->au_n > 0 && this->isFirstAUInBufferingPeriod)
  {
    time_t t_g_90 = (t_r_nominal_n - this->t_af_nm1) * 90000;
    time_t initial_cpb_removal_delay_time(initial_cpb_removal_delay);
    if (!cbr_flag && initial_cpb_removal_delay_time > ceil(t_g_90))
    {
      // TODO: These logs should go somewhere!
      DEBUG_AVC("HRD AU " << this->au_n << " POC " << poc << " - Warning: Conformance fail. initial_cpb_removal_delay " << initial_cpb_removal_delay << " - should be <= ceil(t_g_90) " << double(ceil(t_g_90)));
    }

    if (cbr_flag && initial_cpb_removal_delay_time < floor(t_g_90))
    {
      DEBUG_AVC("HRD AU " << this->au_n << " POC " << poc << " - Warning: Conformance fail. initial_cpb_removal_delay " << initial_cpb_removal_delay << " - should be >= floor(t_g_90) " << double(floor(t_g_90)));
    }
  }

  // C.3. - 2 Check the decoding buffer fullness

  // Calculate the fill state of the decoding buffer. This works like this:
  // For each frame (AU), the buffer is fileed from t_ai to t_af (which indicate) from
  // when to when bits are recieved for a frame. time t_r, the access unit is removed from
  // the buffer. If none of this applies, the buffer fill stays constant.
  // (All values are scaled by 90000 to avoid any rounding errors)
  const auto buffer_size = (uint64_t) sps->vui_parameters.nal_hrd.CpbSize[SchedSelIdx];

  // The time windows we have to process is from t_af_nm1 to t_af (t_ai is in between
  // these two).

  // 1: Process the time from t_af_nm1 (>) to t_ai (<=) (previously processed AUs might be removed
  //    in this time window (tr_n)). In this time, no data is added to the buffer (no
  //    frame is being received) but frames may be removed from the buffer. This time can
  //    be zero.
  //    In case of a CBR encode, no time of constant bitrate should occur.
  if (this->t_af_nm1 < t_ai)
  {
    auto it = this->framesToRemove.begin();
    auto lastFrameTime = this->t_af_nm1;
    while (it != this->framesToRemove.end())
    {
      if (it->t_r < this->t_af_nm1 || (*it).t_r <= t_ai)
      {
        if (it->t_r < this->t_af_nm1)
        {
          // This should not happen (all frames prior to t_af_nm1 should have been
          // removed from the buffer already). Remove now and warn.
          DEBUG_AVC("HRD AU " << this->au_n << " POC " << poc << " - Warning: Removing frame with removal time (" << double(it->t_r) << ") before final arrival time (" << double(t_af_nm1) << "). Buffer underflow");
        }
        this->addConstantBufferLine(poc, lastFrameTime, (*it).t_r, plotModel);
        this->removeFromBufferAndCheck((*it), poc, (*it).t_r, plotModel);
        it = this->framesToRemove.erase(it);
        if (it != this->framesToRemove.end())
          lastFrameTime = (*it).t_r;
      }
      else
        break;
    }
  }

  // 2. For the time from t_ai to t_af, the buffer is filled with the signaled bitrate.
  //    The overall bits which are added in this time period correspond to the size
  //    of the frame in bits which will be later removed again.
  //    Also, previously received frames can be removed in this time interval.
  assert(t_af > t_ai);
  auto relevant_frames = this->popRemoveFramesInTimeInterval(t_ai, t_af);

  bool underflowRemoveCurrentAU = false;
  if (t_r_n <= t_af)
  {
    // The picture is removed from the buffer before the last bit of it could be
    // received. This is a buffer underflow. We handle this by aborting transmission of bits for that AU
    // when the frame is recieved.
    underflowRemoveCurrentAU = true;
  }

  unsigned int au_buffer_add = auBits;
  {
    // time_t au_time_expired = t_af - t_ai;
    // uint64_t au_bits_add   = (uint64_t)(au_time_expired * (unsigned int) bitrate);
    // assert(au_buffer_add == au_bits_add || au_buffer_add == au_bits_add + 1 ||
    // au_buffer_add + 1 == au_bits_add);
  }
  if (relevant_frames.empty() && !underflowRemoveCurrentAU)
  {
    this->addToBufferAndCheck(au_buffer_add, buffer_size, poc, t_ai, t_af, plotModel);
  }
  else
  {
    // While bits are coming into the buffer, frames are removed as well.
    // For each period, we add the bits (and check the buffer state) and remove the
    // frames from the buffer (and check the buffer state).
    auto t_ai_sub                    = t_ai;
    unsigned int buffer_add_sum      = 0;
    long double buffer_add_remainder = 0;
    for (auto frame : relevant_frames)
    {
      assert(frame.t_r >= t_ai_sub && frame.t_r < t_af);
      const time_t time_expired         = frame.t_r - t_ai_sub;
      long double buffer_add_fractional = bitrate * time_expired + buffer_add_remainder;
      unsigned int buffer_add = floor(buffer_add_fractional);
      buffer_add_remainder    = buffer_add_fractional - buffer_add;
      buffer_add_sum += buffer_add;
      this->addToBufferAndCheck(buffer_add, buffer_size, poc, t_ai_sub, frame.t_r, plotModel);
      this->removeFromBufferAndCheck(frame, poc, frame.t_r, plotModel);
      t_ai_sub = frame.t_r;
    }
    if (underflowRemoveCurrentAU)
    {
      if (t_r_n < t_ai_sub)
      {
        // The removal time is even before the previous frame?!? Is this even possible?
      }
      // The last interval from t_ai_sub to t_r_n. After t_r_n we stop the current frame.
      const time_t time_expired         = t_r_n - t_ai_sub;
      long double buffer_add_fractional = bitrate * time_expired + buffer_add_remainder;
      unsigned int buffer_add = floor(buffer_add_fractional);
      this->addToBufferAndCheck(buffer_add, buffer_size, poc, t_ai_sub, t_r_n, plotModel);
      this->removeFromBufferAndCheck(HRDFrameToRemove(t_r_n, auBits, poc), poc, t_r_n, plotModel);
      // "Stop transmission" at this point. We have removed the frame so transmission of more bytes for it make no sense.
      t_af = t_r_n;
    }
    else
    {
      // The last interval from t_ai_sub to t_af
      assert(au_buffer_add >= buffer_add_sum);
      unsigned int buffer_add_remain = au_buffer_add - buffer_add_sum;
      // The sum should correspond to the size of the complete AU
      time_t time_expired               = t_af - t_ai_sub;
      long double buffer_add_fractional = bitrate * time_expired + buffer_add_remainder;
      {
        unsigned int buffer_add = round(buffer_add_fractional);
        buffer_add_sum += buffer_add;
        // assert(buffer_add_sum == au_buffer_add || buffer_add_sum + 1 == au_buffer_add
        // || buffer_add_sum == au_buffer_add + 1);
      }
      this->addToBufferAndCheck(buffer_add_remain, buffer_size, poc, t_ai_sub, t_af, plotModel);
    }
  }

  if (!underflowRemoveCurrentAU)
    framesToRemove.push_back(HRDFrameToRemove(t_r_n, auBits, poc));

  this->t_af_nm1 = t_af;

  // C.3 - 3. A CPB underflow is specified as the condition in which t_r,n(n) is less than
  // t_af(n). When low_delay_hrd_flag is equal to 0, the CPB shall never underflow.
  if (t_r_nominal_n < t_af && !sps->vui_parameters.low_delay_hrd_flag)
  {
    DEBUG_AVC("HRD AU " << this->au_n << " POC " << poc << " - Warning: Decoding Buffer underflow t_r_n " << double(t_r_n) << " t_af " << double(t_af));
  }

  this->au_n++;
  this->isFirstAUInBufferingPeriod = false;
}

void AnnexBAVC::HRD::endOfFile(HRDPlotModel *plotModel)
{
  // From time this->t_af_nm1 onwards, just remove all of the frames which have not been removed yet.
  auto lastFrameTime = this->t_af_nm1;
  for (const auto &frame : this->framesToRemove)
  {
    this->addConstantBufferLine(frame.poc, lastFrameTime, frame.t_r, plotModel);
    this->removeFromBufferAndCheck(frame, frame.poc, frame.t_r, plotModel);
    lastFrameTime = frame.t_r;
  }
  this->framesToRemove.clear();
}

QList<AnnexBAVC::HRD::HRDFrameToRemove> AnnexBAVC::HRD::popRemoveFramesInTimeInterval(time_t from, time_t to)
{
  QList<AnnexBAVC::HRD::HRDFrameToRemove> l;
  auto it = this->framesToRemove.begin();
  double t_r_previous = 0;
  while (it != this->framesToRemove.end())
  {
    if ((*it).t_r < from)
    {
      DEBUG_AVC("Warning: Frame " << (*it).poc << " was not removed at the time (" << double((*it).t_r) << ") it should have been. Dropping it now.");
      it = this->framesToRemove.erase(it);
      continue;
    }
    if ((*it).t_r < t_r_previous)
    {
      DEBUG_AVC("Warning: Frame " << (*it).poc << " has a removal time (" << double((*it).t_r) << ") before the previous frame (" << t_r_previous << "). Dropping it now.");
      it = this->framesToRemove.erase(it);
      continue;
    }
    if ((*it).t_r >= from && (*it).t_r < to)
    {
      t_r_previous = (*it).t_r;
      l.push_back((*it));
      it = this->framesToRemove.erase(it);
      continue;
    }
    if ((*it).t_r >= to)
      break;
    // Prevent an infinite loop in case of wrong data. We should never reach this if all timings are correct.
    it++;
  }
  return l;
}

void AnnexBAVC::HRD::addToBufferAndCheck(unsigned bufferAdd, unsigned bufferSize, int poc, time_t t_begin, time_t t_end, HRDPlotModel *plotModel)
{
  const auto bufferOld = this->decodingBufferLevel;
  this->decodingBufferLevel += bufferAdd;
  
  {
    HRDPlotModel::HRDEntry entry;
    entry.type = HRDPlotModel::HRDEntry::EntryType::Adding;
    entry.cbp_fullness_start = bufferOld;
    entry.cbp_fullness_end = this->decodingBufferLevel;
    entry.time_offset_start = t_begin;
    entry.time_offset_end = t_end;
    entry.poc = poc;
    plotModel->addHRDEntry(entry);
  }
  if (this->decodingBufferLevel > bufferSize)
  {
    this->decodingBufferLevel = bufferSize;
    DEBUG_AVC("HRD AU " << this->au_n << " POC " << poc << " - Warning: Time " << double(t_end) << " Decoding Buffer overflow by " << (int(this->decodingBufferLevel) - bufferSize) << "bits" << " added bits " << bufferAdd << ")");
  }
}

void AnnexBAVC::HRD::removeFromBufferAndCheck(const HRDFrameToRemove &frame, int poc, time_t removalTime, HRDPlotModel *plotModel)
{
  Q_UNUSED(poc);

  // Remove the frame from the buffer
  unsigned int bufferSub = frame.bits;
  const auto bufferOld   = this->decodingBufferLevel;
  this->decodingBufferLevel -= bufferSub;
  {
    HRDPlotModel::HRDEntry entry;
    entry.type = HRDPlotModel::HRDEntry::EntryType::Removal;
    entry.cbp_fullness_start = bufferOld;
    entry.cbp_fullness_end = this->decodingBufferLevel;
    entry.time_offset_start = removalTime;
    entry.time_offset_end = removalTime;
    entry.poc = frame.poc;
    plotModel->addHRDEntry(entry);
  }
  if (this->decodingBufferLevel < 0)
  {
    // The buffer did underflow; i.e. we need to decode a pictures
    // at the time but there is not enough data in the buffer to do so (to take the AU
    // out of the buffer).
    DEBUG_AVC("HRD AU " << this->au_n << " POC " << poc << " - Warning: Time " << double(frame.t_r) << " Decoding Buffer underflow by " << this->decodingBufferLevel << "bits");
  }
}

void AnnexBAVC::HRD::addConstantBufferLine(int poc, time_t t_begin, time_t t_end, HRDPlotModel *plotModel)
{
  HRDPlotModel::HRDEntry entry;
  entry.type = HRDPlotModel::HRDEntry::EntryType::Adding;
  entry.cbp_fullness_start = this->decodingBufferLevel;
  entry.cbp_fullness_end = this->decodingBufferLevel;
  entry.time_offset_start = t_begin;
  entry.time_offset_end = t_end;
  entry.poc = poc;
  plotModel->addHRDEntry(entry);
}

} //namespace parser