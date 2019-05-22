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

#include "parserAnnexBHEVC.h"

#include <algorithm>
#include <cmath>

#include "parserCommonMacros.h"

using namespace parserCommon;

#define PARSER_HEVC_DEBUG_OUTPUT 0
#if PARSER_HEVC_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_HEVC qDebug
#else
#define DEBUG_HEVC(fmt,...) ((void)0)
#endif

const QStringList parserAnnexBHEVC::nal_unit_type_toString = QStringList()
<< "TRAIL_N" << "TRAIL_R" << "TSA_N" << "TSA_R" << "STSA_N" << "STSA_R" << "RADL_N" << "RADL_R" << "RASL_N" << "RASL_R" << "RSV_VCL_N10" << "RSV_VCL_N12" << "RSV_VCL_N14" << 
"RSV_VCL_R11" << "RSV_VCL_R13" << "RSV_VCL_R15" << "BLA_W_LP" << "BLA_W_RADL" << "BLA_N_LP" << "IDR_W_RADL" <<
"IDR_N_LP" << "CRA_NUT" << "RSV_IRAP_VCL22" << "RSV_IRAP_VCL23" << "RSV_VCL24" << "RSV_VCL25" << "RSV_VCL26" << "RSV_VCL27" << "RSV_VCL28" << "RSV_VCL29" <<
"RSV_VCL30" << "RSV_VCL31" << "VPS_NUT" << "SPS_NUT" << "PPS_NUT" << "AUD_NUT" << "EOS_NUT" << "EOB_NUT" << "FD_NUT" << "PREFIX_SEI_NUT" <<
"SUFFIX_SEI_NUT" << "RSV_NVCL41" << "RSV_NVCL42" << "RSV_NVCL43" << "RSV_NVCL44" << "RSV_NVCL45" << "RSV_NVCL46" << "RSV_NVCL47" << "UNSPECIFIED";

double parserAnnexBHEVC::getFramerate() const
{
  // First try to get the framerate from the parameter sets themselves
  for (auto nal : nalUnitList)
  {
    // This should be an hevc nal
    auto nal_hevc = nal.dynamicCast<nal_unit_hevc>();

    if (nal_hevc->nal_type == VPS_NUT) 
    {
      auto v = nal_hevc.dynamicCast<vps>();

      if (v->vps_timing_info_present_flag)
        // The VPS knows the frame rate
        return v->frameRate;
    }
  }

  // The VPS had no information on the frame rate.
  // Look for VUI information in the sps
  for (auto nal : nalUnitList)
  {
    // This should be an hevc nal
    auto nal_hevc = nal.dynamicCast<nal_unit_hevc>();

    if (nal_hevc->nal_type == SPS_NUT)
    {
      auto s = nal_hevc.dynamicCast<sps>();
      if (s->vui_parameters_present_flag && s->sps_vui_parameters.vui_timing_info_present_flag)
        // The VUI knows the frame rate
        return s->sps_vui_parameters.frameRate;
    }
  }

  return DEFAULT_FRAMERATE;
}

QSize parserAnnexBHEVC::getSequenceSizeSamples() const
{
  // Find the first SPS and return the size
  for (auto nal : nalUnitList)
  {
    // This should be an hevc nal
    auto nal_hevc = nal.dynamicCast<nal_unit_hevc>();

    if (nal_hevc->nal_type == SPS_NUT) 
    {
      auto s = nal.dynamicCast<sps>();
      return QSize(s->get_conformance_cropping_width(), s->get_conformance_cropping_height());
    }
  }

  return QSize(-1,-1);
}

yuvPixelFormat parserAnnexBHEVC::getPixelFormat() const
{
  // Get the subsampling and bit-depth from the sps
  int bitDepthY = -1;
  int bitDepthC = -1;
  YUVSubsamplingType subsampling = YUV_NUM_SUBSAMPLINGS;
  for (auto nal : nalUnitList)
  {
    // This should be an hevc nal
    auto nal_hevc = nal.dynamicCast<nal_unit_hevc>();

    if (nal_hevc->nal_type == SPS_NUT)
    {
      auto s = nal_hevc.dynamicCast<sps>();
      if (s->chroma_format_idc == 0)
        subsampling = YUV_400;
      else if (s->chroma_format_idc == 1)
        subsampling = YUV_420;
      else if (s->chroma_format_idc == 2)
        subsampling = YUV_422;
      else if (s->chroma_format_idc == 3)
        subsampling = YUV_444;

      bitDepthY = s->bit_depth_luma_minus8 + 8;
      bitDepthC = s->bit_depth_chroma_minus8 + 8;
    }

    if (bitDepthY != -1 && bitDepthC != -1 && subsampling != YUV_NUM_SUBSAMPLINGS)
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

QList<QByteArray> parserAnnexBHEVC::getSeekFrameParamerSets(int iFrameNr, uint64_t &filePos)
{
  // Get the POC for the frame number
  int seekPOC = POCList[iFrameNr];

  // Collect the active parameter sets
  vps_map active_VPS_list;
  sps_map active_SPS_list;
  pps_map active_PPS_list;

  for (auto nal : nalUnitList)
  {
    // This should be an hevc nal
    auto nal_hevc = nal.dynamicCast<nal_unit_hevc>();

    if (nal_hevc->isSlice()) 
    {
      // We can cast this to a slice.
      auto s = nal_hevc.dynamicCast<slice>();

      if (s->globalPOC == seekPOC)
      {
        // Seek here
        filePos = s->filePosStartEnd.first;

        // Get the bitstream of all active parameter sets
        QList<QByteArray> paramSets;

        for (auto v : active_VPS_list)
          paramSets.append(v->getRawNALData());
        for (auto s : active_SPS_list)
          paramSets.append(s->getRawNALData());
        for (auto p : active_PPS_list)
          paramSets.append(p->getRawNALData());

        return paramSets;
      }
    }
    else if (nal_hevc->nal_type == VPS_NUT)
    {
      // Add vps (replace old one if existed)
      auto v = nal_hevc.dynamicCast<vps>();
      active_VPS_list.insert(v->vps_video_parameter_set_id, v);
    }
    else if (nal_hevc->nal_type == SPS_NUT) 
    {
      // Add sps (replace old one if existed)
      auto s = nal_hevc.dynamicCast<sps>();
      active_SPS_list.insert(s->sps_seq_parameter_set_id, s);
    }
    else if (nal_hevc->nal_type == PPS_NUT) 
    {
      // Add pps (replace old one if existed)
      auto p = nal_hevc.dynamicCast<pps>();
      active_PPS_list.insert(p->pps_pic_parameter_set_id, p);
    }
  }

  return QList<QByteArray>();
}

QByteArray parserAnnexBHEVC::getExtradata()
{
  // Just return the VPS, SPS and PPS in NAL unit format. From the format in the extradata, ffmpeg will detect that
  // the input file is in raw NAL unit format and accept AVPacets in NAL unit format.
  QByteArray ret;
  QByteArray startCode;
  startCode.append((char)0);
  startCode.append((char)0);
  startCode.append((char)0);
  startCode.append((char)1);
  for (auto nal : nalUnitList)
  {
    // This should be an hevc nal
    auto nal_hevc = nal.dynamicCast<nal_unit_hevc>();
    if (nal_hevc->nal_type == VPS_NUT)
    {
      auto v = nal.dynamicCast<vps>();
      ret.append(startCode);
      ret.append(v->getRawNALData());
      break;
    }
  }
  for (auto nal : nalUnitList)
  {
    // This should be an hevc nal
    auto nal_hevc = nal.dynamicCast<nal_unit_hevc>();
    if (nal_hevc->nal_type == SPS_NUT)
    {
      auto s = nal.dynamicCast<sps>();
      ret.append(startCode);
      ret.append(s->getRawNALData());
      break;
    }
  }
  for (auto nal : nalUnitList)
  {
    // This should be an hevc nal
    auto nal_hevc = nal.dynamicCast<nal_unit_hevc>();
    if (nal_hevc->nal_type == PPS_NUT)
    {
      auto p = nal.dynamicCast<pps>();
      ret.append(startCode);
      ret.append(p->getRawNALData());
      break;
    }
  }
  return ret;
}

QPair<int,int> parserAnnexBHEVC::getProfileLevel()
{
  for (auto nal : nalUnitList)
  {
    // This should be an hevc nal
    auto nal_hevc = nal.dynamicCast<nal_unit_hevc>();

    if (nal_hevc->nal_type == SPS_NUT)
    {
      auto s = nal.dynamicCast<sps>();
      return QPair<int,int>(s->ptl.general_profile_idc, s->ptl.general_level_idc);
    }
  }
  return QPair<int,int>(0,0);
}

QPair<int,int> parserAnnexBHEVC::getSampleAspectRatio()
{
  for (auto nal : nalUnitList)
  {
    // This should be an hevc nal
    auto nal_hevc = nal.dynamicCast<nal_unit_hevc>();

    if (nal_hevc->nal_type == SPS_NUT)
    {
      auto s = nal.dynamicCast<sps>();
      if (s->vui_parameters_present_flag && s->sps_vui_parameters.aspect_ratio_info_present_flag)
      {
        int aspect_ratio_idc = s->sps_vui_parameters.aspect_ratio_idc;
        if (aspect_ratio_idc > 0 && aspect_ratio_idc <= 16)
        {
          int widths[] = {1, 12, 10, 16, 40, 24, 20, 32, 80, 18, 15, 64, 160, 4, 3, 2};
          int heights[] = {1, 11, 11, 11, 33, 11, 11, 11, 33, 11, 11, 33, 99, 3, 2, 1};

          const int i = aspect_ratio_idc - 1;
          return QPair<int,int>(widths[i], heights[i]);
        }
        if (aspect_ratio_idc == 255)
          return QPair<int,int>(s->sps_vui_parameters.sar_width, s->sps_vui_parameters.sar_height);
        return QPair<int,int>(0,0);
      }
    }
  }
  return QPair<int,int>(1,1);
}

bool parserAnnexBHEVC::parseAndAddNALUnit(int nalID, QByteArray data, TreeItem *parent, QUint64Pair nalStartEndPosFile, QString *nalTypeName)
{
  if (nalID == -1 && data.isEmpty())
  {
    if (curFramePOC != -1)
    {
      // Save the info of the last frame
      if (!addFrameToList(curFramePOC, curFrameFileStartEndPos, curFrameIsRandomAccess))
        return reader_helper::addErrorMessageChildItem(QString("Error - POC %1 alread in the POC list.").arg(curFramePOC), parent);
      DEBUG_HEVC("Adding start/end %d/%d - POC %d%s", curFrameFileStartEndPos.first, curFrameFileStartEndPos.second, curFramePOC, curFrameIsRandomAccess ? " - ra" : "");
    }
    // The file ended
    std::sort(POCList.begin(), POCList.end());
    return false;
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
  else if (!packetModel->isNull())
    nalRoot = new TreeItem(packetModel->getRootItem());

  // Create a nal_unit and read the header
  nal_unit_hevc nal_hevc(nalStartEndPosFile, nalID);
  if (!nal_hevc.parse_nal_unit_header(nalHeaderBytes, nalRoot))
    return false;

  if (nal_hevc.isSlice())
  {
    // Reparse the SEI messages that we could not parse so far. This is a slice so all parameter sets should be available now.
    while (!reparse_sei.empty())
    {
      auto sei = reparse_sei.front();
      reparse_sei.pop_front();
      if (sei->payloadType == 0)
      {
        auto buffering_period = sei.dynamicCast<buffering_period_sei>();
        buffering_period->reparse_buffering_period_sei(active_SPS_list);
      }
      else if (sei->payloadType == 1)
      {
        auto pic_timing = sei.dynamicCast<pic_timing_sei>();
        pic_timing->reparse_pic_timing_sei(active_VPS_list, active_SPS_list);
      }
      else if (sei->payloadType == 129)
      {
        auto active_param_set_sei = sei.dynamicCast<active_parameter_sets_sei>();
        active_param_set_sei->reparse_active_parameter_sets_sei(active_VPS_list);
      }
    }
  }

  bool parsingSuccess = true;
  if (nal_hevc.nal_type == VPS_NUT)
  {
    // A video parameter set
    auto new_vps = QSharedPointer<vps>(new vps(nal_hevc));
    parsingSuccess = new_vps->parse_vps(payload, nalRoot);

    // Put parameter sets into the NAL unit list
    nalUnitList.append(new_vps);

    // Add vps (replace old one if existed)
    active_VPS_list.insert(new_vps->vps_video_parameter_set_id, new_vps);

    // Add the VPS ID
    specificDescription = parsingSuccess ? QString(" VPS_NUT ID %1").arg(new_vps->vps_video_parameter_set_id) : " VPS_NUT ERR";
    if (nalTypeName)
      *nalTypeName = parsingSuccess ? QString("VPS(%1)").arg(new_vps->vps_video_parameter_set_id) : "VPS(ERR)";
  }
  else if (nal_hevc.nal_type == SPS_NUT)
  {
    // A sequence parameter set
    auto new_sps = QSharedPointer<sps>(new sps(nal_hevc));
    parsingSuccess = new_sps->parse_sps(payload, nalRoot);

    // Add sps (replace old one if existed)
    active_SPS_list.insert(new_sps->sps_seq_parameter_set_id, new_sps);

    // Also add sps to list of all nals
    nalUnitList.append(new_sps);

    // Add the SPS ID
    specificDescription = parsingSuccess ? QString(" SPS_NUT ID %1").arg(new_sps->sps_seq_parameter_set_id) : " SPS_NUT ERR";
    if (nalTypeName)
      *nalTypeName = parsingSuccess ? QString("SPS(%1)").arg(new_sps->sps_seq_parameter_set_id) : "SPS(ERR)";
  }
  else if (nal_hevc.nal_type == PPS_NUT) 
  {
    // A picture parameter set
    auto new_pps = QSharedPointer<pps>(new pps(nal_hevc));
    parsingSuccess = new_pps->parse_pps(payload, nalRoot);

    // Add pps (replace old one if existed)
    active_PPS_list.insert(new_pps->pps_pic_parameter_set_id, new_pps);

    // Also add pps to list of all nals
    nalUnitList.append(new_pps);

    // Add the PPS ID
    specificDescription = parsingSuccess ? QString(" PPS_NUT ID %1").arg(new_pps->pps_pic_parameter_set_id) : " PPS_NUT ERR";
    if (nalTypeName)
      *nalTypeName = parsingSuccess ? QString("PPS(%1)").arg(new_pps->pps_pic_parameter_set_id) : "PPS(ERR)";
  }
  else if (nal_hevc.isSlice())
  {
    // Create a new slice unit
    auto new_slice = QSharedPointer<slice>(new slice(nal_hevc));
    parsingSuccess = new_slice->parse_slice(payload, active_SPS_list, active_PPS_list, lastFirstSliceSegmentInPic, nalRoot);

    int POC = -1;
    if (parsingSuccess)
    {
      // Add the POC of the slice
      if (new_slice->isIRAP() && new_slice->NoRaslOutputFlag && maxPOCCount > 0)
      {
        pocCounterOffset = maxPOCCount + 1;
        maxPOCCount = -1;
      }
      POC = pocCounterOffset + new_slice->PicOrderCntVal;
      DEBUG_HEVC("parserAnnexBHEVC::parseAndAddNALUnit calculated POC %d - pocCounterOffset %d maxPOCCount %d%s%s", POC, pocCounterOffset, maxPOCCount, (new_slice->isIRAP() ? " - IRAP" : ""), (new_slice->NoRaslOutputFlag ? "" : " - RASL"));
      if (POC > maxPOCCount && !(new_slice->isIRAP() && new_slice->NoRaslOutputFlag))
        maxPOCCount = POC;
      new_slice->globalPOC = POC;
    
      if (new_slice->first_slice_segment_in_pic_flag)
        lastFirstSliceSegmentInPic = new_slice;

      isRandomAccessSkip = false;
      if (firstPOCRandomAccess == INT_MAX)
      {
        if (nal_hevc.nal_type == CRA_NUT
          || nal_hevc.nal_type == BLA_W_LP
          || nal_hevc.nal_type == BLA_N_LP
          || nal_hevc.nal_type == BLA_W_RADL)
        {
          // set the POC random access since we need to skip the reordered pictures in the case of CRA/CRANT/BLA/BLANT.
          firstPOCRandomAccess = new_slice->PicOrderCntVal;
        }
        else if (nal_hevc.nal_type == IDR_W_RADL || nal_hevc.nal_type == IDR_N_LP)
        {
          firstPOCRandomAccess = -INT_MAX; // no need to skip the reordered pictures in IDR, they are decodable.
        }
        else
        {
          isRandomAccessSkip = true;
        }
      }
      // skip the reordered pictures, if necessary
      else if (new_slice->PicOrderCntVal < firstPOCRandomAccess && (nal_hevc.nal_type == RASL_R || nal_hevc.nal_type == RASL_N))
      {
        isRandomAccessSkip = true;
      }

      if (new_slice->first_slice_segment_in_pic_flag)
      {
        // This slice NAL is the start of a new frame
        if (curFramePOC != -1)
        {
          // Save the info of the last frame
          if (!addFrameToList(curFramePOC, curFrameFileStartEndPos, curFrameIsRandomAccess))
            return reader_helper::addErrorMessageChildItem(QString("Error - POC %1 alread in the POC list.").arg(curFramePOC), nalRoot);
          DEBUG_HEVC("Adding start/end %d/%d - POC %d%s", curFrameFileStartEndPos.first, curFrameFileStartEndPos.second, curFramePOC, curFrameIsRandomAccess ? " - ra" : "");
        }
        curFrameFileStartEndPos = nalStartEndPosFile;
        curFramePOC = new_slice->globalPOC;
        curFrameIsRandomAccess = new_slice->isIRAP();
      }
      else
        // Another slice NAL which belongs to the last frame
        // Update the end position
        curFrameFileStartEndPos.second = nalStartEndPosFile.second;

      if (nal_hevc.isIRAP())
      {
        if (new_slice->first_slice_segment_in_pic_flag)
        {
          // This is the first slice of a random access point. Add it to the list.
          nalUnitList.append(new_slice);
        }
      }
    }

    specificDescription = parsingSuccess ? QString(" POC %1").arg(POC) : " POC ERR";
    if (nalTypeName)
      *nalTypeName = parsingSuccess ? QString("Slice(POC %1)").arg(POC) : "Slice(ERR)";
  }
  else if (nal_hevc.nal_type == PREFIX_SEI_NUT || nal_hevc.nal_type == SUFFIX_SEI_NUT)
  {
    // An SEI NAL. Each SEI NAL may contain multiple sei_payloads
    auto new_sei = QSharedPointer<sei>(new sei(nal_hevc));
    QByteArray sei_data = payload;

    int sei_count = 0;
    while(!sei_data.isEmpty())
    {
      TreeItem *const message_tree = nalRoot ? new TreeItem("", nalRoot) : nullptr;

      // Parse the SEI header and remove it from the data array
      int nrBytes = new_sei->parse_sei_header(sei_data, message_tree);
      if (nrBytes == -1)
        return false;

      sei_data.remove(0, nrBytes);

      if (message_tree)
        message_tree->itemData[0] = QString("sei_message %1 - %2").arg(sei_count).arg(new_sei->payloadTypeName);

      QByteArray sub_sei_data = sei_data.mid(0, new_sei->payloadSize);

      parsingSuccess = true;
      sei_parsing_return_t result;
      QSharedPointer<sei> reparse;
      if (new_sei->payloadType == 0)
      {
        auto new_buffering_period_sei = QSharedPointer<buffering_period_sei>(new buffering_period_sei(new_sei));
        result = new_buffering_period_sei->parse_buffering_period_sei(sub_sei_data, active_SPS_list, message_tree);
        reparse = new_buffering_period_sei;
      }
      else if (new_sei->payloadType == 1)
      {
        auto new_pic_timing_sei = QSharedPointer<pic_timing_sei>(new pic_timing_sei(new_sei));
        result = new_pic_timing_sei->parse_pic_timing_sei(sub_sei_data, active_VPS_list, active_SPS_list, message_tree);
        reparse = new_pic_timing_sei;
      }
      else if (new_sei->payloadType == 5)
      {
        auto new_user_data_sei = QSharedPointer<user_data_sei>(new user_data_sei(new_sei));
        result = new_user_data_sei->parse_user_data_sei(sub_sei_data, message_tree);
      }
      else if (new_sei->payloadType == 129)
      {
        auto new_active_parameter_sets_sei = QSharedPointer<active_parameter_sets_sei>(new active_parameter_sets_sei(new_sei));
        result = new_active_parameter_sets_sei->parse_active_parameter_sets_sei(sub_sei_data, active_VPS_list, message_tree);
        reparse = new_active_parameter_sets_sei;
      }
      else if (new_sei->payloadType == 147)
      {
        auto new_alternative_transfer_characteristics_sei = QSharedPointer<alternative_transfer_characteristics_sei>(new alternative_transfer_characteristics_sei(new_sei));
        result = new_alternative_transfer_characteristics_sei->parse_alternative_transfer_characteristics_sei(sub_sei_data, message_tree);
        reparse = new_alternative_transfer_characteristics_sei;
      }
      else
        // The default parser just logs the raw bytes
        result = new_sei->parser_sei_bytes(sub_sei_data, message_tree);

      if (result == SEI_PARSING_WAIT_FOR_PARAMETER_SETS)
        reparse_sei.append(reparse);
      if (result == SEI_PARSING_ERROR)
        parsingSuccess = false;

      // Remove the sei payload bytes from the data
      sei_data.remove(0, new_sei->payloadSize);
      if (sei_data.length() == 1)
      {
        // This should be the rspb trailing bits (10000000)
        sei_data.remove(0, 1);
      }

      sei_count++;
    }

    specificDescription = QString(" Number Messages: %1").arg(sei_count);
    if (nalTypeName)
      *nalTypeName = QString("SEI(#%1)").arg(sei_count);
  }

  if (nalRoot)
    // Set a useful name of the TreeItem (the root for this NAL)
    nalRoot->itemData.append(QString("NAL %1: %2").arg(nal_hevc.nal_idx).arg(nal_unit_type_toString.value(nal_hevc.nal_type)) + specificDescription);

  return true;
}


bool parserAnnexBHEVC::profile_tier_level::parse_profile_tier_level(reader_helper &reader, bool profilePresentFlag, int maxNumSubLayersMinus1)
{
  reader_sub_level s(reader, "profile_tier_level()");

  // Profile tier level
  if (profilePresentFlag) 
  {
    READBITS(general_profile_space, 2);
    READFLAG(general_tier_flag);
    READBITS(general_profile_idc, 5);

    for(int j = 0; j < 32; j++)
      READFLAG(general_profile_compatibility_flag[j]);
    READFLAG(general_progressive_source_flag);
    READFLAG(general_interlaced_source_flag);
    READFLAG(general_non_packed_constraint_flag);
    READFLAG(general_frame_only_constraint_flag);

    if(general_profile_idc == 4 || general_profile_compatibility_flag[4] || 
      general_profile_idc == 5 || general_profile_compatibility_flag[5] || 
      general_profile_idc == 6 || general_profile_compatibility_flag[6] || 
      general_profile_idc == 7 || general_profile_compatibility_flag[7]) 
    {
      READFLAG(general_max_12bit_constraint_flag);
      READFLAG(general_max_10bit_constraint_flag);
      READFLAG(general_max_8bit_constraint_flag);
      READFLAG(general_max_422chroma_constraint_flag);
      READFLAG(general_max_420chroma_constraint_flag);
      READFLAG(general_max_monochrome_constraint_flag);
      READFLAG(general_intra_constraint_flag);
      READFLAG(general_one_picture_only_constraint_flag);
      READFLAG(general_lower_bit_rate_constraint_flag);

      READZEROBITS(34, "general_reserved_zero_bits");
    }
    else
      READZEROBITS(43, "general_reserved_zero_bits");
    
    if((general_profile_idc >= 1 && general_profile_idc <= 5) ||
      general_profile_compatibility_flag[1] || general_profile_compatibility_flag[2] || 
      general_profile_compatibility_flag[3] || general_profile_compatibility_flag[4] || 
      general_profile_compatibility_flag[5])
      READFLAG(general_inbld_flag);
    else 
      READFLAG(general_reserved_zero_bit);
  }

  QString (*general_level_idc_meaning)(unsigned int) = [](unsigned int idc)
  {
    if (idc % 30 == 0)
      return QString("Level %1").arg(idc / 30);
    else
      return QString("Level %1").arg(double(idc) / 30, -1, 'f', 1);
  };
  READBITS_M(general_level_idc, 8, general_level_idc_meaning);

  for(int i = 0; i < maxNumSubLayersMinus1; i++)
  {
    READFLAG(sub_layer_profile_present_flag[i]);
    READFLAG(sub_layer_level_present_flag[i]);
  }

  if(maxNumSubLayersMinus1 > 0)
    for(int i = maxNumSubLayersMinus1; i < 8; i++)
      READZEROBITS(2, QString("reserved_zero_2bits[%1]").arg(i));

  for(int i = 0; i < maxNumSubLayersMinus1; i++)
  {
    if(sub_layer_profile_present_flag[i])
    {
      READBITS(sub_layer_profile_space[i], 2);
      READFLAG(sub_layer_tier_flag[i]);

      READBITS(sub_layer_profile_idc[i], 5);

      for(int j = 0; j < 32; j++)
        READFLAG(sub_layer_profile_compatibility_flag[i][j]);

      READFLAG(sub_layer_progressive_source_flag[i]);
      READFLAG(sub_layer_interlaced_source_flag[i]);
      READFLAG(sub_layer_non_packed_constraint_flag[i]);
      READFLAG(sub_layer_frame_only_constraint_flag[i]);

      if(sub_layer_profile_idc[i] == 4 || sub_layer_profile_compatibility_flag[i][4] || 
        sub_layer_profile_idc[i] == 5 || sub_layer_profile_compatibility_flag[i][5] || 
        sub_layer_profile_idc[i] == 6 || sub_layer_profile_compatibility_flag[i][6] || 
        sub_layer_profile_idc[i] == 7 || sub_layer_profile_compatibility_flag[i][7])
      {
        READFLAG(sub_layer_max_12bit_constraint_flag[i]);
        READFLAG(sub_layer_max_10bit_constraint_flag[i]);
        READFLAG(sub_layer_max_8bit_constraint_flag[i]);
        READFLAG(sub_layer_max_422chroma_constraint_flag[i]);
        READFLAG(sub_layer_max_420chroma_constraint_flag[i]);
        READFLAG(sub_layer_max_monochrome_constraint_flag[i]);
        READFLAG(sub_layer_intra_constraint_flag[i]);
        READFLAG(sub_layer_one_picture_only_constraint_flag[i]);
        READFLAG(sub_layer_lower_bit_rate_constraint_flag[i]);

        READZEROBITS(34, QString("sub_layer_reserved_zero_bits[%1]").arg(i));
      }
      else
        READZEROBITS(43, QString("sub_layer_reserved_zero_bits[%1]").arg(i));

      if((sub_layer_profile_idc[i] >= 1 && sub_layer_profile_idc[i] <= 5) ||
        sub_layer_profile_compatibility_flag[1] || sub_layer_profile_compatibility_flag[2] || 
        sub_layer_profile_compatibility_flag[3] || sub_layer_profile_compatibility_flag[4] || 
        sub_layer_profile_compatibility_flag[5])
        READFLAG(sub_layer_inbld_flag[i]);
      else
        READFLAG(sub_layer_reserved_zero_bit[i]);
    }

    if(sub_layer_level_present_flag[i])
      READBITS(sub_layer_level_idc[i], 8);
  }
  return true;
}

bool parserAnnexBHEVC::sub_layer_hrd_parameters::parse_sub_layer_hrd_parameters(reader_helper &reader, int subLayerId, int CpbCnt, bool sub_pic_hrd_params_present_flag, bool SubPicHrdFlag, unsigned int bit_rate_scale, unsigned int cpb_size_scale, unsigned int cpb_size_du_scale)
{
  Q_UNUSED(subLayerId);
  reader_sub_level s(reader, "sub_layer_hrd_parameters()");

  if (CpbCnt >= 32)
    return reader.addErrorMessageChildItem("The value of CpbCnt must be in the range of 0 to 31");

  for(int i = 0; i <= CpbCnt; i++)
  {
    READUEV_A_M(bit_rate_value_minus1, i, "(together with bit_rate_scale) specifies the maximum input bit rate for the i-th CPB when the CPB operates at the access unit level.");
    if (bit_rate_value_minus1[i] > 4294967294)
      return reader.addErrorMessageChildItem("bit_rate_value_minus1[i] shall be in the range of 0 to 2 32 - 2, inclusive.");
    if (i > 0 && bit_rate_value_minus1[i] <= bit_rate_value_minus1[i-1])
      return reader.addErrorMessageChildItem("For any i > 0, bit_rate_value_minus1[i] shall be greater than bit_rate_value_minus1[i-1].");
    if (!SubPicHrdFlag)
    {
      int value = (bit_rate_value_minus1[i] + 1) * (1 << (6 + bit_rate_scale));
      BitRate.append(value);
      LOGVAL(BitRate[i]);
    }

    READUEV_A_M(cpb_size_value_minus1, i, "Is used together with cpb_size_scale to specify the i-th CPB size when the CPB operates at the access unit level.");
    if (cpb_size_value_minus1[i] > 4294967294)
      return reader.addErrorMessageChildItem("bit_rate_value_minus1[i] shall be in the range of 0 to 2 32 - 2, inclusive.");
    if (i > 0 && cpb_size_value_minus1[i] > cpb_size_value_minus1[i-1])
      return reader.addErrorMessageChildItem("For any i greater than 0, cpb_size_value_minus1[i] shall be less than or equal to cpb_size_value_minus1[i-1].");
    if (!SubPicHrdFlag)
    {
      int value = (cpb_size_value_minus1[i] + 1) * (1 << (4 + cpb_size_scale));
      CpbSize.append(value);
      LOGVAL(CpbSize[i]);
    }

    if (sub_pic_hrd_params_present_flag)
    {
      READUEV_A_M(cpb_size_du_value_minus1, i, "Is used together with cpb_size_du_scale to specify the i-th CPB size when the CPB operates at sub-picture level.");
      if (cpb_size_du_value_minus1[i] > 4294967294)
        return reader.addErrorMessageChildItem("cpb_size_du_value_minus1[ i ] shall be in the range of 0 to 2 32 - 2, inclusive.");
      if (i > 0 && cpb_size_du_value_minus1[i] > cpb_size_du_value_minus1[i-1])
        return reader.addErrorMessageChildItem("For any i greater than 0, cpb_size_du_value_minus1[i] shall be less than or equal to cpb_size_du_value_minus1[i-1].");
      if (SubPicHrdFlag)
      {
        int value = (cpb_size_du_value_minus1[i] + 1) * (1 << (4 + cpb_size_du_scale));
        CpbSize.append(value);
        LOGVAL(CpbSize[i]);
      }
    
      READUEV_A_M(bit_rate_du_value_minus1, i, "(together with bit_rate_scale) specifies the maximum input bit rate for the i-th CPB when the CPB operates at the sub-picture level.");
      if (bit_rate_du_value_minus1[i] > 4294967294)
        return reader.addErrorMessageChildItem("bit_rate_du_value_minus1[ i ] shall be in the range of 0 to 2 32 - 2, inclusive.");
      if (i > 0 && bit_rate_du_value_minus1[i] <= bit_rate_du_value_minus1[i-1])
        return reader.addErrorMessageChildItem("For any i > 0, bit_rate_du_value_minus1[i] shall be greater than bit_rate_du_value_minus1[i-1].");
      if (SubPicHrdFlag)
      {
        int value = (bit_rate_du_value_minus1[i] + 1) * (1 << (6 + bit_rate_scale));
        BitRate.append(value);
        LOGVAL(BitRate[i]);
      }
    }
    READFLAG_A_M(cbr_flag, i, "False specifies that to decode this CVS by the HRD using the i-th CPB specification, the hypothetical stream scheduler (HSS) operates in an intermittent bit rate mode. True specifies that the HSS operates in a constant bit rate (CBR) mode.");
  }
  return true;
}

bool parserAnnexBHEVC::hrd_parameters::parse_hrd_parameters(reader_helper &reader, bool commonInfPresentFlag, int maxNumSubLayersMinus1)
{
  reader_sub_level s(reader, "hrd_parameters()");
  
  sub_pic_hrd_params_present_flag = false;

  if (commonInfPresentFlag)
  {
    READFLAG(nal_hrd_parameters_present_flag);
    READFLAG(vcl_hrd_parameters_present_flag);

    if(nal_hrd_parameters_present_flag || vcl_hrd_parameters_present_flag)
    {
      READFLAG_M(sub_pic_hrd_params_present_flag, "Can the HRD operate at access unit or sub-picture level (true) or only at access unit level (false)");
      if (sub_pic_hrd_params_present_flag)
      {
        READBITS_M(tick_divisor_minus2, 8, "Specifies the clock sub-tick. A clock sub-tick is the minimum interval of time that can be represented in the coded data when sub_pic_hrd_params_present_flag is equal to 1");
        READBITS_M(du_cpb_removal_delay_increment_length_minus1, 5, "Specifies the length, in bits, of some coded symbols");
        READFLAG(sub_pic_cpb_params_in_pic_timing_sei_flag);
        READBITS(dpb_output_delay_du_length_minus1, 5);
      }
      READBITS_M(bit_rate_scale, 4, "(together with bit_rate_value_minus1[i]) specifies the maximum input bit rate of the i-th CPB.");
      READBITS_M(cpb_size_scale, 4, "(together with cpb_size_value_minus1[i]) specifies the CPB size of the i-th CPB when the CPB operates at the access unit level.");
      if(sub_pic_hrd_params_present_flag)
        READBITS_M(cpb_size_du_scale, 4, "(together with cpb_size_du_value_minus1[ i ]) specifies the CPB size of the i-th CPB when the CPB operates at sub-picture level.");
      READBITS_M(initial_cpb_removal_delay_length_minus1, 5, "Specifies the length, in bits, of some coded symbols");
      READBITS_M(au_cpb_removal_delay_length_minus1, 5, "Specifies the length, in bits, of some coded symbols");
      READBITS_M(dpb_output_delay_length_minus1, 5, "Specifies the length, in bits, of some coded symbols");
    }
  }

  SubPicHrdPreferredFlag = false;
  SubPicHrdFlag = (SubPicHrdPreferredFlag && sub_pic_hrd_params_present_flag);
  LOGVAL(SubPicHrdPreferredFlag);
  LOGVAL(SubPicHrdFlag);

  if (maxNumSubLayersMinus1 >= 8)
    return reader.addErrorMessageChildItem("The value of maxNumSubLayersMinus1 must be in the range of 0 to 7");

  for(int i = 0; i <= maxNumSubLayersMinus1; i++)
  {
    READFLAG_M(fixed_pic_rate_general_flag[i], "Equal to 1 indicates that, when HighestTid is equal to i, the temporal distance between the HRD output times of consecutive pictures in output order is constrained as specified by the following syntax elements.");
    if (!fixed_pic_rate_general_flag[i])
      READFLAG_M(fixed_pic_rate_within_cvs_flag[i], "equal to 1 indicates that, when HighestTid is equal to i, the temporal distance between the HRD output times of consecutive pictures in output order is constrained as specified by the following syntax elements.");
    else
      fixed_pic_rate_within_cvs_flag[i] = 1;
    if (fixed_pic_rate_within_cvs_flag[i])
    {
      READUEV_M(elemental_duration_in_tc_minus1[i], "Specifies, when HighestTid is equal to i, the temporal distance, in clock ticks, between the elemental units that specify the HRD output times of consecutive pictures in output order");
      if (elemental_duration_in_tc_minus1[i] > 2047)
        return reader.addErrorMessageChildItem("The value of elemental_duration_in_tc_minus1[i] shall be in the range of 0 to 2047, inclusive.");
      low_delay_hrd_flag[i] = false;
    }
    else
      READFLAG_M(low_delay_hrd_flag[i], "Specifies the HRD operational mode");
    cpb_cnt_minus1[i] = 0;
    if (!low_delay_hrd_flag[i])
    {
      READUEV_M(cpb_cnt_minus1[i], "Specifies the number of alternative CPB specifications in the bitstream of the CVS when HighestTid is equal to i.");
      if (cpb_cnt_minus1[i] > 31)
        return reader.addErrorMessageChildItem("The value of cpb_cnt_minus1[i] shall be in the range of 0 to 31, inclusive.");
    }

    if(nal_hrd_parameters_present_flag)
      if (!nal_sub_hrd[i].parse_sub_layer_hrd_parameters(reader, i, cpb_cnt_minus1[i], sub_pic_hrd_params_present_flag, SubPicHrdFlag, bit_rate_scale, cpb_size_scale, cpb_size_du_scale))
        return false;
    if(vcl_hrd_parameters_present_flag)
      if (!vcl_sub_hrd[i].parse_sub_layer_hrd_parameters(reader, i, cpb_cnt_minus1[i], sub_pic_hrd_params_present_flag, SubPicHrdFlag, bit_rate_scale, cpb_size_scale, cpb_size_du_scale))
        return false;
  }
  
  return true;
}

bool parserAnnexBHEVC::pred_weight_table::parse_pred_weight_table(reader_helper &reader, sps *actSPS, slice *actSlice)
{
  reader_sub_level s(reader, "pred_weight_table()");
  
  READUEV(luma_log2_weight_denom);
  if (actSPS->ChromaArrayType != 0)
    READSEV(delta_chroma_log2_weight_denom);
  for(unsigned int i = 0; i <= actSlice->num_ref_idx_l0_active_minus1; i++)
    READFLAG_A(luma_weight_l0_flag, i);
  if(actSPS->ChromaArrayType != 0)
    for(unsigned int i = 0; i <= actSlice->num_ref_idx_l0_active_minus1; i++)
      READFLAG_A(chroma_weight_l0_flag, i);
  for(unsigned int i = 0; i <= actSlice->num_ref_idx_l0_active_minus1; i++)
  {
    if(luma_weight_l0_flag[i])
    {
      READSEV_A(delta_luma_weight_l0, i);
      READSEV_A(luma_offset_l0, i);
    }
    if(chroma_weight_l0_flag[i])
      for(int j = 0; j < 2; j++)
      {
        READSEV_A(delta_chroma_weight_l0, j);
        READSEV_A(delta_chroma_offset_l0, j);
      }
  }

  if(actSlice->slice_type == 0) // B
  {
    for(unsigned int i = 0; i <= actSlice->num_ref_idx_l1_active_minus1; i++)
      READFLAG_A(luma_weight_l1_flag, i);
    if(actSPS->ChromaArrayType != 0)
      for(unsigned int i = 0; i <= actSlice->num_ref_idx_l1_active_minus1; i++)
        READFLAG_A(chroma_weight_l1_flag, i);
    for(unsigned int i = 0; i <= actSlice->num_ref_idx_l1_active_minus1; i++)
    {
      if(luma_weight_l1_flag[i]) 
      {
        READSEV_A(delta_luma_weight_l1, i);
        READSEV_A(luma_offset_l1, i);
      }
      if(chroma_weight_l1_flag[i])
        for(int j = 0; j < 2; j++)
        {
          READSEV_A(delta_chroma_weight_l1, j);
          READSEV_A(delta_chroma_offset_l1, j);
        }
    }
  }

  return true;
}

unsigned int parserAnnexBHEVC::st_ref_pic_set::NumNegativePics[65];
unsigned int parserAnnexBHEVC::st_ref_pic_set::NumPositivePics[65];
int parserAnnexBHEVC::st_ref_pic_set::DeltaPocS0[65][16];
int parserAnnexBHEVC::st_ref_pic_set::DeltaPocS1[65][16];
bool parserAnnexBHEVC::st_ref_pic_set::UsedByCurrPicS0[65][16];
bool parserAnnexBHEVC::st_ref_pic_set::UsedByCurrPicS1[65][16];
unsigned int parserAnnexBHEVC::st_ref_pic_set::NumDeltaPocs[65];

bool parserAnnexBHEVC::st_ref_pic_set::parse_st_ref_pic_set(reader_helper &reader, unsigned int stRpsIdx, sps *actSPS)
{
  reader_sub_level s(reader, "st_ref_pic_set()");
  
  if (stRpsIdx > 64)
    return reader.addErrorMessageChildItem("Error while parsing short term ref pic set. The stRpsIdx must be in the range [0..64].");

  inter_ref_pic_set_prediction_flag = false;
  if(stRpsIdx != 0)
    READFLAG(inter_ref_pic_set_prediction_flag);

  if (inter_ref_pic_set_prediction_flag)
  {
    delta_idx_minus1 = 0;
    if (stRpsIdx == actSPS->num_short_term_ref_pic_sets)
      READUEV(delta_idx_minus1);
    READFLAG(delta_rps_sign);
    READUEV(abs_delta_rps_minus1);

    int RefRpsIdx = stRpsIdx - (delta_idx_minus1 + 1);                     // Rec. ITU-T H.265 v3 (04/2015) (7-57)
    int deltaRps = (1 - 2 * delta_rps_sign) * (abs_delta_rps_minus1 + 1);  // Rec. ITU-T H.265 v3 (04/2015) (7-58)
    LOGVAL(RefRpsIdx);
    LOGVAL(deltaRps);

    for(unsigned int j=0; j<=NumDeltaPocs[RefRpsIdx]; j++)
    {
      READFLAG_A(used_by_curr_pic_flag, j);
      use_delta_flag.append(true); // Infer to 1
      if(!used_by_curr_pic_flag.last())
        READFLAG(use_delta_flag[j]);
    }

    // Derive NumNegativePics Rec. ITU-T H.265 v3 (04/2015) (7-59)
    int i = 0;
    for(int j=(int)NumPositivePics[RefRpsIdx] - 1; j >= 0; j--)
    {
      int dPoc = DeltaPocS1[RefRpsIdx][j] + deltaRps;
      if(dPoc < 0 && use_delta_flag[NumNegativePics[RefRpsIdx] + j]) 
      { 
        DeltaPocS0[stRpsIdx][i] = dPoc;
        LOGSTRVAL(QString("DeltaPocS0[%1][%2]").arg(stRpsIdx).arg(i), dPoc);
        UsedByCurrPicS0[stRpsIdx][i++] = used_by_curr_pic_flag[NumNegativePics[RefRpsIdx] + j];
      }
    }
    if(deltaRps < 0 && use_delta_flag[NumDeltaPocs[RefRpsIdx]])
    { 
      DeltaPocS0[stRpsIdx][i] = deltaRps;
      LOGSTRVAL(QString("DeltaPocS0[%1][%2]").arg(stRpsIdx).arg(i), deltaRps);
      UsedByCurrPicS0[stRpsIdx][i++] = used_by_curr_pic_flag[NumDeltaPocs[RefRpsIdx]];
    }
    for(unsigned int j=0; j<NumNegativePics[RefRpsIdx]; j++)
    { 
      int dPoc = DeltaPocS0[RefRpsIdx][j] + deltaRps;
      if(dPoc < 0 && use_delta_flag[j])
      { 
        DeltaPocS0[stRpsIdx][i] = dPoc;
        LOGSTRVAL(QString("DeltaPocS0[%1][%2]").arg(stRpsIdx).arg(i), dPoc);
        UsedByCurrPicS0[stRpsIdx][i++] = used_by_curr_pic_flag[j];
      } 
    } 
    NumNegativePics[stRpsIdx] = i;
    LOGSTRVAL(QString("NumNegativePics[%1]").arg(stRpsIdx), i);

    // Derive NumPositivePics Rec. ITU-T H.265 v3 (04/2015) (7-60)
    i = 0;
    for(int j=(int)NumNegativePics[RefRpsIdx] - 1; j>=0; j--)
    { 
      int dPoc = DeltaPocS0[RefRpsIdx][j] + deltaRps;
      if(dPoc > 0 && use_delta_flag[j])
      { 
        DeltaPocS1[stRpsIdx][i] = dPoc;
        LOGSTRVAL(QString("DeltaPocS1[%1][%2]").arg(stRpsIdx).arg(i), dPoc);
        UsedByCurrPicS1[stRpsIdx][i++] = used_by_curr_pic_flag[j];
      }
    }
    if(deltaRps > 0 && use_delta_flag[NumDeltaPocs[RefRpsIdx]])
    {
      DeltaPocS1[stRpsIdx][i] = deltaRps;
      LOGSTRVAL(QString("DeltaPocS1[%1][%2]").arg(stRpsIdx).arg(i), deltaRps);
      UsedByCurrPicS1[stRpsIdx][i++] = used_by_curr_pic_flag[NumDeltaPocs[RefRpsIdx]];
    }
    for(unsigned int j=0; j<NumPositivePics[RefRpsIdx]; j++)
    { 
      int dPoc = DeltaPocS1[RefRpsIdx][j] + deltaRps;
      if(dPoc > 0 && use_delta_flag[NumNegativePics[RefRpsIdx] + j])
      { 
        DeltaPocS1[stRpsIdx][i] = dPoc;
        LOGSTRVAL(QString("DeltaPocS1[%1][%2]").arg(stRpsIdx).arg(i), dPoc);
        UsedByCurrPicS1[stRpsIdx][i++] = used_by_curr_pic_flag[NumNegativePics[RefRpsIdx] + j] ;
      }
    }
    NumPositivePics[stRpsIdx] = i;
    LOGSTRVAL(QString("NumPositivePics[%1]").arg(stRpsIdx), i);
  }
  else
  {
    READUEV(num_negative_pics);
    READUEV(num_positive_pics);
    for(unsigned int i = 0; i < num_negative_pics; i++)
    {
      READUEV_A(delta_poc_s0_minus1, i);
      READFLAG_A(used_by_curr_pic_s0_flag, i);

      if (i==0)
        DeltaPocS0[stRpsIdx][i] = -((int)delta_poc_s0_minus1.last() + 1); // (7-65)
      else
        DeltaPocS0[stRpsIdx][i] = DeltaPocS0[stRpsIdx][i-1] - (delta_poc_s0_minus1.last() + 1); // (7-67)
      LOGSTRVAL(QString("DeltaPocS0[%1][%2]").arg(stRpsIdx).arg(i), DeltaPocS0[stRpsIdx][i]);
      UsedByCurrPicS0[stRpsIdx][i] = used_by_curr_pic_s0_flag[i];
      LOGSTRVAL(QString("UsedByCurrPicS0[%1][%2]").arg(stRpsIdx).arg(i), UsedByCurrPicS0[stRpsIdx][i]);
    }
    for(unsigned int i = 0; i < num_positive_pics; i++)
    {
      READUEV_A(delta_poc_s1_minus1, i);
      READFLAG_A(used_by_curr_pic_s1_flag, i);

      if (i==0)
        DeltaPocS1[stRpsIdx][i] = delta_poc_s1_minus1.last() + 1; // (7-66)
      else
        DeltaPocS1[stRpsIdx][i] = DeltaPocS1[stRpsIdx][i-1] + (delta_poc_s1_minus1.last() + 1); // (7-68)
      LOGSTRVAL(QString("DeltaPocS1[%1][%2]").arg(stRpsIdx).arg(i), DeltaPocS1[stRpsIdx][i]);
      UsedByCurrPicS1[stRpsIdx][i] = used_by_curr_pic_s1_flag[i];
      LOGSTRVAL(QString("UsedByCurrPicS1[%1][%2]").arg(stRpsIdx).arg(i), UsedByCurrPicS1[stRpsIdx][i]);
    }

    NumNegativePics[stRpsIdx] = num_negative_pics;
    NumPositivePics[stRpsIdx] = num_positive_pics;
    LOGSTRVAL(QString("NumNegativePics[%1]").arg(stRpsIdx), num_negative_pics);
    LOGSTRVAL(QString("NumPositivePics[%1]").arg(stRpsIdx), num_positive_pics);
  }

  NumDeltaPocs[stRpsIdx] = NumNegativePics[stRpsIdx] + NumPositivePics[stRpsIdx]; // (7-69)
  return true;
}

// (7-55)
int parserAnnexBHEVC::st_ref_pic_set::NumPicTotalCurr(int CurrRpsIdx, slice *actSlice)
{
  int NumPicTotalCurr = 0;
  for(unsigned int i = 0; i < NumNegativePics[CurrRpsIdx]; i++)
    if(UsedByCurrPicS0[CurrRpsIdx][i])
      NumPicTotalCurr++ ;
  for(unsigned int i = 0; i < NumPositivePics[CurrRpsIdx]; i++)  
    if(UsedByCurrPicS1[CurrRpsIdx][i]) 
      NumPicTotalCurr++;
  for(unsigned int i = 0; i < actSlice->num_long_term_sps + actSlice->num_long_term_pics; i++) 
    if(actSlice->UsedByCurrPicLt[i])
      NumPicTotalCurr++;
  return NumPicTotalCurr;
}

bool parserAnnexBHEVC::vui_parameters::parse_vui_parameters(reader_helper &reader, sps *actSPS)
{
  reader_sub_level s(reader, "vui_parameters()");

  READFLAG(aspect_ratio_info_present_flag);
  if(aspect_ratio_info_present_flag)
  {
    QStringList aspect_ratio_idc_meaning = QStringList() << "Unspecified"
      << "1:1 (square)" << "12:11" << "10:11" << "16:11" << "40:33" << "24:11" << "20:11" << "32:11" 
      << "80:33" << "18:11" << "15:11" << "64:33" << "160:99" << "4:3" << "3:2" << "2:1" << "Reserved";
    READBITS_M(aspect_ratio_idc, 8, aspect_ratio_idc_meaning);
    if(aspect_ratio_idc == 255) // EXTENDED_SAR=255
    {
      READBITS(sar_width, 16);
      READBITS(sar_height, 16);
    }
  }

  READFLAG(overscan_info_present_flag);
  if(overscan_info_present_flag)
    READFLAG(overscan_appropriate_flag);

  READFLAG(video_signal_type_present_flag);
  if(video_signal_type_present_flag)
  {
    QStringList video_format_meaning = QStringList() << "Component" << "PAL" << "NTSC" << "SECAM" << "MAC" << "Unspecified video format" << "Unspecified video format" << "Unspecified video format";
    READBITS_M(video_format, 3, video_format_meaning);
    READFLAG(video_full_range_flag);

    READFLAG(colour_description_present_flag);
    if(colour_description_present_flag)
    {
      READBITS_M(colour_primaries, 8, get_colour_primaries_meaning());
      READBITS_M(transfer_characteristics, 8, get_transfer_characteristics_meaning());
      READBITS_M(matrix_coeffs, 8, get_matrix_coefficients_meaning());
    }
  }

  READFLAG(chroma_loc_info_present_flag);
  if(chroma_loc_info_present_flag)
  {
    READUEV(chroma_sample_loc_type_top_field);
    READUEV(chroma_sample_loc_type_bottom_field);
  }

  READFLAG(neutral_chroma_indication_flag);
  READFLAG(field_seq_flag);
  READFLAG(frame_field_info_present_flag);
  READFLAG(default_display_window_flag);
  if(default_display_window_flag)
  {
    READUEV(def_disp_win_left_offset);
    READUEV(def_disp_win_right_offset);
    READUEV(def_disp_win_top_offset);
    READUEV(def_disp_win_bottom_offset);
  }

  READFLAG(vui_timing_info_present_flag);
  if(vui_timing_info_present_flag)
  {
    // The VUI has timing information for us
    READBITS(vui_num_units_in_tick, 32);
    READBITS(vui_time_scale, 32);

    frameRate = (double)vui_time_scale / (double)vui_num_units_in_tick;
    LOGVAL(frameRate);

    READFLAG(vui_poc_proportional_to_timing_flag);
    if(vui_poc_proportional_to_timing_flag)
      READUEV(vui_num_ticks_poc_diff_one_minus1);
    READFLAG(vui_hrd_parameters_present_flag);
    if (vui_hrd_parameters_present_flag)
      if (!vui_hrd_parameters.parse_hrd_parameters(reader, 1, actSPS->sps_max_sub_layers_minus1))
        return false;
  }

  READFLAG(bitstream_restriction_flag);
  if (bitstream_restriction_flag)
  {
    READFLAG(tiles_fixed_structure_flag);
    READFLAG(motion_vectors_over_pic_boundaries_flag);
    READFLAG(restricted_ref_pic_lists_flag);
    READUEV(min_spatial_segmentation_idc);
    READUEV(max_bytes_per_pic_denom);
    READUEV(max_bits_per_min_cu_denom);
    READUEV(log2_max_mv_length_horizontal);
    READUEV(log2_max_mv_length_vertical);
  }

  return true;
}

bool parserAnnexBHEVC::scaling_list_data::parse_scaling_list_data(reader_helper &reader)
{
  reader_sub_level s(reader, "scaling_list_data()");
  
  for(int sizeId=0; sizeId<4; sizeId++)
  {
    for(int matrixId=0; matrixId<6; matrixId += (sizeId == 3) ? 3 : 1) 
    { 

      READFLAG(scaling_list_pred_mode_flag[sizeId][matrixId]);
      if (!scaling_list_pred_mode_flag[sizeId][matrixId])
      {
        READUEV(scaling_list_pred_matrix_id_delta[sizeId][matrixId]);
      }
      else
      {
        int nextCoef = 8;
        int coefNum = std::min(64, (1 << (4 + (sizeId << 1))));
        if(sizeId > 1)
        {
          READSEV(scaling_list_dc_coef_minus8[sizeId-2][matrixId]);
          nextCoef = scaling_list_dc_coef_minus8[sizeId-2][matrixId] + 8;
        }
        for (int i=0; i<coefNum; i++)
        {
          int scaling_list_delta_coef;
          READSEV(scaling_list_delta_coef);
          nextCoef = (nextCoef + scaling_list_delta_coef + 256) % 256;
          int scalingListVal = nextCoef;
          LOGSTRVAL(QString("scalingListVal[%1][%2][%3]").arg(sizeId).arg(matrixId).arg(i), scalingListVal);
        }
      }
    }
  }
  return true;
}

bool parserAnnexBHEVC::vps::parse_vps(const QByteArray &parameterSetData, TreeItem *root)
{
  nalPayload = parameterSetData;

  reader_helper reader(parameterSetData, root, "video_parameter_set_rbsp()");

  READBITS(vps_video_parameter_set_id, 4);
  READFLAG(vps_base_layer_internal_flag);
  READFLAG(vps_base_layer_available_flag);
  READBITS(vps_max_layers_minus1, 6);
  READBITS(vps_max_sub_layers_minus1, 3);
  READFLAG(vps_temporal_id_nesting_flag);
  READBITS(vps_reserved_0xffff_16bits, 16);

  // Parse the profile tier level
  if (!ptl.parse_profile_tier_level(reader, true, vps_max_sub_layers_minus1))
    return false;

  READFLAG(vps_sub_layer_ordering_info_present_flag);
  for(unsigned int i = (vps_sub_layer_ordering_info_present_flag ? 0 : vps_max_sub_layers_minus1); i <= vps_max_sub_layers_minus1; i++)
  {
    READUEV(vps_max_dec_pic_buffering_minus1[i]);
    READUEV(vps_max_num_reorder_pics[i]);
    READUEV(vps_max_latency_increase_plus1[i]);
  }

  READBITS(vps_max_layer_id, 6); // 0...63
  READUEV(vps_num_layer_sets_minus1); // 0 .. 1023

  for(unsigned int i=1; i <= vps_num_layer_sets_minus1; i++)
    for(unsigned int j=0; j <= vps_max_layer_id; j++)
      READFLAG_A(layer_id_included_flag[i], i);

  READFLAG(vps_timing_info_present_flag);
  if(vps_timing_info_present_flag)
  {
    // The VPS can provide timing info (the time between two POCs. So the refresh rate)
    READBITS(vps_num_units_in_tick,32);
    READBITS(vps_time_scale,32);
    READFLAG(vps_poc_proportional_to_timing_flag);
    if(vps_poc_proportional_to_timing_flag)
      READUEV(vps_num_ticks_poc_diff_one_minus1);

    // HRD parameters ...
    READUEV(vps_num_hrd_parameters);
    for(unsigned int i=0; i < vps_num_hrd_parameters; i++)
    {
      READUEV_A(hrd_layer_set_idx, i);

      if(i > 0)
        READFLAG_A(cprms_present_flag, i);

      // hrd_parameters...
      vps_hrd_parameters.append(hrd_parameters());
      if (!vps_hrd_parameters.last().parse_hrd_parameters(reader, cprms_present_flag[i], vps_max_sub_layers_minus1))
        return false;
    }

    frameRate = (double)vps_time_scale / (double)vps_num_units_in_tick;
    LOGVAL(frameRate);
  }

  READFLAG(vps_extension_flag);
  if (vps_extension_flag)
    LOGPARAM("vps_extension()", 0, "", "", "Not implemented yet...");

  // TODO:
  // Here comes the VPS extension.
  // This is specified in the annex F, multilayer and stuff.
  // This could be added and is definitely interesting.
  // ... later

  return true;
}

bool parserAnnexBHEVC::sps::parse_sps(const QByteArray &parameterSetData, TreeItem *root)
{
  nalPayload = parameterSetData;

  reader_helper reader(parameterSetData, root, "seq_parameter_set_rbsp()");

  READBITS(sps_video_parameter_set_id,4);
  READBITS(sps_max_sub_layers_minus1, 3);
  READFLAG(sps_temporal_id_nesting_flag);

  // parse profile tier level
  if (!ptl.parse_profile_tier_level(reader, true, sps_max_sub_layers_minus1))
    return false;

  /// Back to the seq_parameter_set_rbsp
  READUEV(sps_seq_parameter_set_id);
  QStringList chroma_format_idc_meaning = QStringList() << "moochrome" << "4:2:0" << "4:2:2" << "4:4:4" << "4:4:4";
  READUEV_M(chroma_format_idc, chroma_format_idc_meaning);
  if (chroma_format_idc == 3)
    READFLAG(separate_colour_plane_flag);
  ChromaArrayType = (separate_colour_plane_flag) ? 0 : chroma_format_idc;
  LOGVAL(ChromaArrayType);

  // Rec. ITU-T H.265 v3 (04/2015) - 6.2 - Table 6-1 
  SubWidthC = (chroma_format_idc == 1 || chroma_format_idc == 2) ? 2 : 1;
  SubHeightC = (chroma_format_idc == 1) ? 2 : 1;
  LOGVAL(SubWidthC);
  LOGVAL(SubHeightC);

  READUEV(pic_width_in_luma_samples);
  READUEV(pic_height_in_luma_samples);
  READFLAG(conformance_window_flag);

  if (conformance_window_flag) 
  {
    READUEV(conf_win_left_offset);
    READUEV(conf_win_right_offset);
    READUEV(conf_win_top_offset);
    READUEV(conf_win_bottom_offset);
  }

  READUEV(bit_depth_luma_minus8);
  READUEV(bit_depth_chroma_minus8);
  READUEV(log2_max_pic_order_cnt_lsb_minus4);

  READFLAG(sps_sub_layer_ordering_info_present_flag);
  for(unsigned int i = (sps_sub_layer_ordering_info_present_flag ? 0 : sps_max_sub_layers_minus1); i <= sps_max_sub_layers_minus1; i++)
  {
    READUEV_A(sps_max_dec_pic_buffering_minus1, i);
    READUEV_A(sps_max_num_reorder_pics, i);
    READUEV_A(sps_max_latency_increase_plus1, i);
  }

  READUEV(log2_min_luma_coding_block_size_minus3);
  READUEV(log2_diff_max_min_luma_coding_block_size);
  READUEV(log2_min_luma_transform_block_size_minus2);
  READUEV(log2_diff_max_min_luma_transform_block_size);
  READUEV(max_transform_hierarchy_depth_inter);
  READUEV(max_transform_hierarchy_depth_intra);
  READFLAG(scaling_list_enabled_flag);

  if(scaling_list_enabled_flag)
  {
    READFLAG(sps_scaling_list_data_present_flag);
    if(sps_scaling_list_data_present_flag)
      if (!sps_scaling_list_data.parse_scaling_list_data(reader))
        return false;
  }

  READFLAG(amp_enabled_flag);
  READFLAG(sample_adaptive_offset_enabled_flag);
  READFLAG(pcm_enabled_flag);

  if(pcm_enabled_flag)
  {
    READBITS(pcm_sample_bit_depth_luma_minus1, 4);
    READBITS(pcm_sample_bit_depth_chroma_minus1, 4);
    READUEV(log2_min_pcm_luma_coding_block_size_minus3);
    READUEV(log2_diff_max_min_pcm_luma_coding_block_size);
    READFLAG(pcm_loop_filter_disabled_flag);
  }

  READUEV(num_short_term_ref_pic_sets);
  for(unsigned int i=0; i<num_short_term_ref_pic_sets; i++)
  {
    sps_st_ref_pic_sets.append(st_ref_pic_set());
    if (!sps_st_ref_pic_sets.last().parse_st_ref_pic_set(reader, i, this))
      return false;
  }

  READFLAG(long_term_ref_pics_present_flag);
  if(long_term_ref_pics_present_flag)
  {
    READUEV(num_long_term_ref_pics_sps);
    for(unsigned int i=0; i<num_long_term_ref_pics_sps; i++)
    {
      int nrBits = log2_max_pic_order_cnt_lsb_minus4 + 4;
      READBITS_A(lt_ref_pic_poc_lsb_sps, nrBits, i);
      READFLAG_A(used_by_curr_pic_lt_sps_flag, i);
    }
  }

  READFLAG(sps_temporal_mvp_enabled_flag);
  READFLAG(strong_intra_smoothing_enabled_flag);
  READFLAG(vui_parameters_present_flag);
  if(vui_parameters_present_flag)
    if (!sps_vui_parameters.parse_vui_parameters(reader, this))
      return false;

  READFLAG(sps_extension_present_flag);
  if (sps_extension_present_flag)
  {
    READFLAG(sps_range_extension_flag);
    READFLAG(sps_multilayer_extension_flag);
    READFLAG(sps_3d_extension_flag);
    READBITS(sps_extension_5bits, 5);
  }

  // Now the extensions follow ...
  // This would also be interesting but later.
  if(sps_range_extension_flag)
    LOGPARAM("sps_range_extension()", 0, "", "", "Not implemented yet...");
  
  if(sps_multilayer_extension_flag)
    LOGPARAM("sps_multilayer_extension()", 0, "", "", "Not implemented yet...");
    
  if(sps_3d_extension_flag)
    LOGPARAM("sps_3d_extension()", 0, "", "", "Not implemented yet...");
    
  if (sps_extension_5bits != 0)
    LOGPARAM("sps_extension_data_flag()", 0, "", "", "Not implemented yet...");
    
  // Calculate some values - Rec. ITU-T H.265 v3 (04/2015) 7.4.3.2.1
  MinCbLog2SizeY = log2_min_luma_coding_block_size_minus3 + 3;              // (7-10)
  CtbLog2SizeY = MinCbLog2SizeY + log2_diff_max_min_luma_coding_block_size; // (7-11)
  CtbSizeY = 1 << CtbLog2SizeY;                                             // (7-13)
  PicWidthInCtbsY  = ceil((float)pic_width_in_luma_samples  / CtbSizeY);    // (7-15)
  PicHeightInCtbsY = ceil((float)pic_height_in_luma_samples / CtbSizeY);    // (7-17)
  PicSizeInCtbsY   = PicWidthInCtbsY * PicHeightInCtbsY;                    // (7-19)

  LOGVAL(MinCbLog2SizeY);
  LOGVAL(CtbLog2SizeY);
  LOGVAL(CtbSizeY);
  LOGVAL(PicWidthInCtbsY);
  LOGVAL(PicHeightInCtbsY);
  LOGVAL(PicSizeInCtbsY);

  return true;
}

bool parserAnnexBHEVC::pps::parse_pps(const QByteArray &parameterSetData, TreeItem *root)
{
  nalPayload = parameterSetData;

  reader_helper reader(parameterSetData, root, "pic_parameter_set_rbsp()");

  READUEV(pps_pic_parameter_set_id);
  READUEV(pps_seq_parameter_set_id);
  READFLAG(dependent_slice_segments_enabled_flag);
  READFLAG(output_flag_present_flag);
  READBITS(num_extra_slice_header_bits, 3);

  READFLAG(sign_data_hiding_enabled_flag);
  READFLAG(cabac_init_present_flag);
  READUEV(num_ref_idx_l0_default_active_minus1);
  READUEV(num_ref_idx_l1_default_active_minus1);
  READSEV(init_qp_minus26);
  READFLAG(constrained_intra_pred_flag);
  READFLAG(transform_skip_enabled_flag);
  READFLAG(cu_qp_delta_enabled_flag);
  if (cu_qp_delta_enabled_flag)
    READUEV(diff_cu_qp_delta_depth);
  READSEV(pps_cb_qp_offset);
  READSEV(pps_cr_qp_offset);
  READFLAG(pps_slice_chroma_qp_offsets_present_flag);
  READFLAG(weighted_pred_flag);
  READFLAG(weighted_bipred_flag);
  READFLAG(transquant_bypass_enabled_flag);
  READFLAG(tiles_enabled_flag);
  READFLAG(entropy_coding_sync_enabled_flag);
  if(tiles_enabled_flag)
  {
    READUEV(num_tile_columns_minus1);
    READUEV(num_tile_rows_minus1);
    READFLAG(uniform_spacing_flag);
    if (!uniform_spacing_flag)
    {
      for(unsigned int i = 0; i < num_tile_columns_minus1; i++)
        READUEV_A(column_width_minus1, i);
      for(unsigned int i = 0; i < num_tile_rows_minus1; i++)
        READUEV_A(row_height_minus1, i);
    }
    READFLAG(loop_filter_across_tiles_enabled_flag);
  }
  READFLAG(pps_loop_filter_across_slices_enabled_flag);
  READFLAG(deblocking_filter_control_present_flag);
  if (deblocking_filter_control_present_flag)
  {
    READFLAG(deblocking_filter_override_enabled_flag);
    READFLAG(pps_deblocking_filter_disabled_flag);
    if (!pps_deblocking_filter_disabled_flag)
    {
      READSEV(pps_beta_offset_div2);
      READSEV(pps_tc_offset_div2);
    }
  }
  READFLAG(pps_scaling_list_data_present_flag);
  if (pps_scaling_list_data_present_flag)
    if (!pps_scaling_list_data.parse_scaling_list_data(reader))
      return false;
  READFLAG(lists_modification_present_flag);
  READUEV(log2_parallel_merge_level_minus2);
  READFLAG(slice_segment_header_extension_present_flag);
  READFLAG(pps_extension_present_flag);
  if (pps_extension_present_flag)
  {
    READFLAG(pps_range_extension_flag);
    READFLAG(pps_multilayer_extension_flag);
    READFLAG(pps_3d_extension_flag);
    READBITS(pps_extension_5bits, 5);
  }

  // Now the extensions follow ...

  if(pps_range_extension_flag)
    if (!range_extension.parse_pps_range_extension(reader, this))
      return false;

  // This would also be interesting but later.
  if(pps_multilayer_extension_flag)
    LOGPARAM("pps_multilayer_extension()", 0, "", "", "Not implemented yet...");
    
  if(pps_3d_extension_flag)
    LOGPARAM("pps_3d_extension()", 0, "", "", "Not implemented yet...");
    
  if (pps_extension_5bits != 0)
    LOGPARAM("pps_extension_data_flag()", 0, "", "", "Not implemented yet...");
    
  // There is more to parse but we are not interested in this information (for now)

  if (entropy_coding_sync_enabled_flag && tiles_enabled_flag)
    parallelism = MIXED_TYPE;
  else if (entropy_coding_sync_enabled_flag)
    parallelism = WAVEFRONT;
  else if (tiles_enabled_flag)
    parallelism = TILE;
  else
    parallelism = SLICE;

  return true;
}

bool parserAnnexBHEVC::pps_range_extension::parse_pps_range_extension(reader_helper &reader, pps *actPPS)
{
  reader_sub_level s(reader, "pps_range_extension()");
  
  if(actPPS->transform_skip_enabled_flag)
    READUEV(log2_max_transform_skip_block_size_minus2);
  READFLAG(cross_component_prediction_enabled_flag);
  READFLAG(chroma_qp_offset_list_enabled_flag);
  if (chroma_qp_offset_list_enabled_flag)
  {
    READUEV(diff_cu_chroma_qp_offset_depth);
    READUEV(chroma_qp_offset_list_len_minus1);
    for(unsigned int i = 0; i <= chroma_qp_offset_list_len_minus1; i++) 
    {
      READSEV_A(cb_qp_offset_list, i);
      READSEV_A(cr_qp_offset_list, i);
    }
  }
  READUEV(log2_sao_offset_scale_luma);
  READUEV(log2_sao_offset_scale_chroma);

  return true;
}

bool parserAnnexBHEVC::ref_pic_lists_modification::parse_ref_pic_lists_modification(reader_helper &reader, slice *actSlice, int NumPicTotalCurr)
{
  reader_sub_level s(reader, "ref_pic_lists_modification()");
  
  int nrBits = ceil(log2(NumPicTotalCurr));

  READFLAG(ref_pic_list_modification_flag_l0);
  if (ref_pic_list_modification_flag_l0)
    for(unsigned int i = 0; i <= actSlice->num_ref_idx_l0_active_minus1; i++)
      READBITS_A(list_entry_l0, nrBits, i);

  if(actSlice->slice_type == 0) // B
  {
    READFLAG(ref_pic_list_modification_flag_l1);
    if (ref_pic_list_modification_flag_l1)
      for(unsigned int i = 0; i <= actSlice->num_ref_idx_l1_active_minus1; i++)
        READBITS_A(list_entry_l1, nrBits, i);
  }

  return true;
}

// Initialize static member. Only true for the first slice instance
bool parserAnnexBHEVC::slice::bFirstAUInDecodingOrder = true;
int parserAnnexBHEVC::slice::prevTid0Pic_slice_pic_order_cnt_lsb = 0;
int parserAnnexBHEVC::slice::prevTid0Pic_PicOrderCntMsb = 0;

parserAnnexBHEVC::slice::slice(const nal_unit_hevc &nal) : nal_unit_hevc(nal)
{
  PicOrderCntVal = -1;
  PicOrderCntMsb = -1;

  // When not present, the value of dependent_slice_segment_flag is inferred to be equal to 0.
  dependent_slice_segment_flag = false;
  pic_output_flag = true;
  short_term_ref_pic_set_sps_flag = false;
  short_term_ref_pic_set_idx = 0;
  num_long_term_sps = 0;
  num_long_term_pics = 0;
  deblocking_filter_override_flag = false;
  slice_temporal_mvp_enabled_flag = false;
  collocated_from_l0_flag = true;
  slice_sao_luma_flag = false;
  slice_sao_chroma_flag = false;
  num_entry_point_offsets = 0;

  globalPOC = -1;
}

// T-REC-H.265-201410 - 7.3.6.1 slice_segment_header()
bool parserAnnexBHEVC::slice::parse_slice(const QByteArray &sliceHeaderData, const sps_map &active_SPS_list, const pps_map &active_PPS_list, QSharedPointer<slice> firstSliceInSegment, TreeItem *root)
{
  reader_helper reader(sliceHeaderData, root, "slice_segment_header()");

  READFLAG(first_slice_segment_in_pic_flag);

  if (isIRAP())
    READFLAG(no_output_of_prior_pics_flag);

  READUEV(slice_pic_parameter_set_id);
  // The value of slice_pic_parameter_set_id shall be in the range of 0 to 63, inclusive. (Max 11 bits read)
  if (slice_pic_parameter_set_id >= 63) 
    return reader.addErrorMessageChildItem("The variable slice_pic_parameter_set_id is out of range.");

  // Get the active PPS
  if (!active_PPS_list.contains(slice_pic_parameter_set_id))
    return reader.addErrorMessageChildItem("The signaled PPS was not found in the bitstream.");
  actPPS = active_PPS_list.value(slice_pic_parameter_set_id);

  // Get the active SPS
  if (!active_SPS_list.contains(actPPS->pps_seq_parameter_set_id))
    return reader.addErrorMessageChildItem("The signaled SPS was not found in the bitstream.");
  actSPS = active_SPS_list.value(actPPS->pps_seq_parameter_set_id);

  if (!first_slice_segment_in_pic_flag)
  {
    if (actPPS->dependent_slice_segments_enabled_flag)
      READFLAG(dependent_slice_segment_flag);
    int nrBits = ceil(log2(actSPS->PicSizeInCtbsY));  // 7.4.7.1
    READBITS(slice_segment_address, nrBits);
  }

  if (!dependent_slice_segment_flag)
  {
    for (unsigned int i=0; i < actPPS->num_extra_slice_header_bits; i++)
      READFLAG_A(slice_reserved_flag, i);

    QStringList slice_type_meaning = QStringList() << "B-Slice" << "P-Slice" << "I-Slice";
    READUEV_M(slice_type, slice_type_meaning); // Max 3 bits read. 0-B 1-P 2-I
    if (actPPS->output_flag_present_flag) 
      READFLAG(pic_output_flag);

    if (actSPS->separate_colour_plane_flag) 
      READBITS(colour_plane_id, 2);

    if(nal_type != IDR_W_RADL && nal_type != IDR_N_LP)
    {
      READBITS(slice_pic_order_cnt_lsb, actSPS->log2_max_pic_order_cnt_lsb_minus4 + 4); // Max 16 bits read
      READFLAG(short_term_ref_pic_set_sps_flag);

      //Decoding of the reference picture sets works differently. 
      //This has to be re-thought ...

      if(!short_term_ref_pic_set_sps_flag)
      {
        if (!st_rps.parse_st_ref_pic_set(reader, actSPS->num_short_term_ref_pic_sets, actSPS.data()))
          return false;
      }
      else if(actSPS->num_short_term_ref_pic_sets > 1)
      {
        int nrBits = ceil(log2(actSPS->num_short_term_ref_pic_sets));
        READBITS(short_term_ref_pic_set_idx, nrBits);

        // The short term ref pic set is the one with the given index from the SPS
        if ((int)short_term_ref_pic_set_idx >= actSPS->sps_st_ref_pic_sets.length())
          return reader.addErrorMessageChildItem("Error parsing slice header. The specified short term ref pic list could not be found in the SPS.");
        st_rps = actSPS->sps_st_ref_pic_sets[short_term_ref_pic_set_idx];

      }
      if(actSPS->long_term_ref_pics_present_flag)
      {
        if(actSPS->num_long_term_ref_pics_sps > 0)
          READUEV(num_long_term_sps);
        READUEV(num_long_term_pics);
        for(unsigned int i = 0; i < num_long_term_sps + num_long_term_pics; i++)
        {
          if(i < num_long_term_sps)
          {
            if(actSPS->num_long_term_ref_pics_sps > 1)
            {
              int nrBits = ceil(log2(actSPS->num_long_term_ref_pics_sps));
              READBITS_A(lt_idx_sps, nrBits, i);
            }

            UsedByCurrPicLt.append(actSPS->used_by_curr_pic_lt_sps_flag[lt_idx_sps.last()]);
          }
          else
          {
            int nrBits = actSPS->log2_max_pic_order_cnt_lsb_minus4 + 4;
            READBITS_A(poc_lsb_lt, nrBits, i);
            READFLAG_A(used_by_curr_pic_lt_flag, i);

            UsedByCurrPicLt.append(used_by_curr_pic_lt_flag.last());
          }  

          READFLAG_A(delta_poc_msb_present_flag, i);
          if(delta_poc_msb_present_flag.last())
            READUEV_A(delta_poc_msb_cycle_lt, i);
        }
      }
      if(actSPS->sps_temporal_mvp_enabled_flag)
        READFLAG(slice_temporal_mvp_enabled_flag);
    }

    if(actSPS->sample_adaptive_offset_enabled_flag)
    {
      READFLAG(slice_sao_luma_flag);
      if (actSPS->ChromaArrayType != 0)
        READFLAG(slice_sao_chroma_flag);
    }

    if(slice_type == 1 || slice_type == 0) // 0-B 1-P 2-I
    {
      // Infer if not present
      num_ref_idx_l0_active_minus1 = actPPS->num_ref_idx_l0_default_active_minus1;
      num_ref_idx_l1_active_minus1 = actPPS->num_ref_idx_l1_default_active_minus1;

      READFLAG(num_ref_idx_active_override_flag);
      if (num_ref_idx_active_override_flag)
      {
        READUEV(num_ref_idx_l0_active_minus1);
        if (slice_type == 0)
          READUEV(num_ref_idx_l1_active_minus1);
      }

      int CurrRpsIdx = (short_term_ref_pic_set_sps_flag) ? short_term_ref_pic_set_idx : actSPS->num_short_term_ref_pic_sets;
      int NumPicTotalCurr = st_rps.NumPicTotalCurr(CurrRpsIdx, this);
      if(actPPS->lists_modification_present_flag && NumPicTotalCurr > 1)
        if (!slice_rpl_mod.parse_ref_pic_lists_modification(reader, this, NumPicTotalCurr))
          return false;

      if(slice_type == 0) // B
        READFLAG(mvd_l1_zero_flag);
      if(actPPS->cabac_init_present_flag)
        READFLAG(cabac_init_flag);
      if(slice_temporal_mvp_enabled_flag)
      {
        if(slice_type == 0) // B
          READFLAG(collocated_from_l0_flag);
        if((collocated_from_l0_flag && num_ref_idx_l0_active_minus1 > 0) || (!collocated_from_l0_flag && num_ref_idx_l1_active_minus1 > 0))
          READUEV(collocated_ref_idx);
      }
      if((actPPS->weighted_pred_flag && slice_type == 1) || (actPPS->weighted_bipred_flag && slice_type == 0))
        if (!slice_pred_weight_table.parse_pred_weight_table(reader, actSPS.data(), this))
          return false;

      READUEV(five_minus_max_num_merge_cand);
    }
    READSEV(slice_qp_delta);
    if(actPPS->pps_slice_chroma_qp_offsets_present_flag)
    {
      READSEV(slice_cb_qp_offset);
      READSEV(slice_cr_qp_offset);
    }
    if(actPPS->range_extension.chroma_qp_offset_list_enabled_flag)
      READFLAG(cu_chroma_qp_offset_enabled_flag);
    if(actPPS->deblocking_filter_override_enabled_flag)
      READFLAG(deblocking_filter_override_flag);
    if(deblocking_filter_override_flag)
    {
      READFLAG(slice_deblocking_filter_disabled_flag);
      if(!slice_deblocking_filter_disabled_flag)
      {
        READSEV(slice_beta_offset_div2);
        READSEV(slice_tc_offset_div2);
      }
    }
    else
      slice_deblocking_filter_disabled_flag = actPPS->pps_deblocking_filter_disabled_flag;

    if(actPPS->pps_loop_filter_across_slices_enabled_flag && (slice_sao_luma_flag || slice_sao_chroma_flag || !slice_deblocking_filter_disabled_flag))
      READFLAG(slice_loop_filter_across_slices_enabled_flag);
  } 
  else // dependent_slice_segment_flag is true -- inferr values from firstSliceInSegment
  {
    if (firstSliceInSegment == nullptr)
      return reader.addErrorMessageChildItem("Dependent slice without a first slice in the segment.");

    slice_pic_order_cnt_lsb = firstSliceInSegment->slice_pic_order_cnt_lsb;
  }

  if(actPPS->tiles_enabled_flag || actPPS->entropy_coding_sync_enabled_flag)
  {
    READUEV(num_entry_point_offsets);
    if (num_entry_point_offsets > 0)
    {
      READUEV(offset_len_minus1);
      if (offset_len_minus1 > 31)
        return reader.addErrorMessageChildItem("offset_len_minus1 shall be 0..31");

      for(unsigned int i = 0; i < num_entry_point_offsets; i++)
      {
        int nrBits = offset_len_minus1 + 1;
        READBITS_A(entry_point_offset_minus1, nrBits, i);
      }
    }
  }

  if(actPPS->slice_segment_header_extension_present_flag)
  {
    READUEV(slice_segment_header_extension_length);
    for(unsigned int i = 0; i < slice_segment_header_extension_length; i++)
      READBITS_A(slice_segment_header_extension_data_byte, 8, i);
  }

  // End of the slice header - byte_alignment()

  // Calculate the picture order count
  int MaxPicOrderCntLsb = 1 << (actSPS->log2_max_pic_order_cnt_lsb_minus4 + 4);
  LOGVAL(MaxPicOrderCntLsb);

  // If the current picture is an IDR picture, a BLA picture, the first picture in the bitstream in decoding order, or the first
  // picture that follows an end of sequence NAL unit in decoding order, the variable NoRaslOutputFlag is set equal to 1.
  NoRaslOutputFlag = false;
  if (nal_type == IDR_W_RADL || nal_type == IDR_N_LP || nal_type == BLA_W_LP)
    NoRaslOutputFlag = true;
  else if (bFirstAUInDecodingOrder) 
  {
    NoRaslOutputFlag = true;
    bFirstAUInDecodingOrder = false;
  }

  // T-REC-H.265-201410 - 8.3.1 Decoding process for picture order count

  int prevPicOrderCntLsb = 0;
  int prevPicOrderCntMsb = 0;
  // When the current picture is not an IRAP picture with NoRaslOutputFlag equal to 1, ...
  if (!(isIRAP() && NoRaslOutputFlag)) 
  {
    // the variables prevPicOrderCntLsb and prevPicOrderCntMsb are derived as follows:

    prevPicOrderCntLsb = prevTid0Pic_slice_pic_order_cnt_lsb;
    prevPicOrderCntMsb = prevTid0Pic_PicOrderCntMsb;
  }
  LOGVAL(prevPicOrderCntLsb);
  LOGVAL(prevPicOrderCntMsb);

  // The variable PicOrderCntMsb of the current picture is derived as follows:
  if (isIRAP() && NoRaslOutputFlag) 
  {
    // If the current picture is an IRAP picture with NoRaslOutputFlag equal to 1, PicOrderCntMsb is set equal to 0.
    PicOrderCntMsb = 0;
  }
  else 
  {
    // Otherwise, PicOrderCntMsb is derived as follows: (8-1)
    if(((int)slice_pic_order_cnt_lsb < prevPicOrderCntLsb) && (((int)prevPicOrderCntLsb - (int)slice_pic_order_cnt_lsb) >= ((int)MaxPicOrderCntLsb / 2)))
      PicOrderCntMsb = prevPicOrderCntMsb + MaxPicOrderCntLsb;
    else if (((int)slice_pic_order_cnt_lsb > prevPicOrderCntLsb) && (((int)slice_pic_order_cnt_lsb - prevPicOrderCntLsb) > ((int)MaxPicOrderCntLsb / 2))) 
      PicOrderCntMsb = prevPicOrderCntMsb - MaxPicOrderCntLsb;
    else 
      PicOrderCntMsb = prevPicOrderCntMsb;
  }
  LOGVAL(PicOrderCntMsb);

  // PicOrderCntVal is derived as follows: (8-2)
  PicOrderCntVal = PicOrderCntMsb + slice_pic_order_cnt_lsb;
  LOGVAL(PicOrderCntVal);

  if (nuh_temporal_id_plus1 - 1 == 0 && !isRASL() && !isRADL()) 
  {
    // Let prevTid0Pic be the previous picture in decoding order that has TemporalId 
    // equal to 0 and that is not a RASL picture, a RADL picture or an SLNR picture.

    // Set these for the next slice
    prevTid0Pic_slice_pic_order_cnt_lsb = slice_pic_order_cnt_lsb;
    prevTid0Pic_PicOrderCntMsb = PicOrderCntMsb;
  }

  return true;
}

QByteArray parserAnnexBHEVC::nal_unit_hevc::getNALHeader() const
{ 
  int out = ((int)nal_unit_type_id << 9) + (nuh_layer_id << 3) + nuh_temporal_id_plus1;
  char c[2] = { (char)(out >> 8), (char)out };
  return QByteArray(c, 2);
}

bool parserAnnexBHEVC::nal_unit_hevc::parse_nal_unit_header(const QByteArray &parameterSetData, TreeItem *root)
{
  // Create a sub byte parser to access the bits
  reader_helper reader(parameterSetData, root, "nal_unit_header()");

  READZEROBITS(1, "forbidden_zero_bit");

  // Read nal unit type
  QStringList nal_unit_type_id_meaning = QStringList()
    << "TRAIL_N Coded slice segment of a non-TSA, non-STSA trailing picture slice_segment_layer_rbsp( )"
    << "TRAIL_R Coded slice segment of a non-TSA, non-STSA trailing picture slice_segment_layer_rbsp( )"
    << "TSA_N Coded slice segment of a TSA picture slice_segment_layer_rbsp( )"
    << "TSA_R Coded slice segment of a TSA picture slice_segment_layer_rbsp( )"
    << "STSA_N Coded slice segment of an STSA picture slice_segment_layer_rbsp( )"
    << "STSA_R Coded slice segment of an STSA picture slice_segment_layer_rbsp( )"
    << "RADL_N Coded slice segment of a RADL picture slice_segment_layer_rbsp( )"
    << "RADL_R Coded slice segment of a RADL picture slice_segment_layer_rbsp( )"
    << "RASL_N Coded slice segment of a RASL picture slice_segment_layer_rbsp( )"
    << "RASL_R Coded slice segment of a RASL picture slice_segment_layer_rbsp( )"
    << "RSV_VCL_N10 Reserved non-IRAP SLNR VCL NAL unit types"
    << "RSV_VCL_N12 Reserved non-IRAP SLNR VCL NAL unit types"
    << "RSV_VCL_N14 Reserved non-IRAP SLNR VCL NAL unit types"
    << "RSV_VCL_R11 Reserved non-IRAP sub-layer reference VCL NAL unit types"
    << "RSV_VCL_R13  Reserved non-IRAP sub-layer reference VCL NAL unit types"
    << "RSV_VCL_R15 Reserved non-IRAP sub-layer reference VCL NAL unit types"
    << "BLA_W_LP Coded slice segment of a BLA picture slice_segment_layer_rbsp( )"
    << "BLA_W_RADL Coded slice segment of a BLA picture slice_segment_layer_rbsp( )"
    << "BLA_N_LP Coded slice segment of a BLA picture slice_segment_layer_rbsp( )"
    << "IDR_W_RADL Coded slice segment of an IDR picture slice_segment_layer_rbsp( )"
    << "IDR_N_LP Coded slice segment of an IDR picture slice_segment_layer_rbsp( )"
    << "CRA_NUT Coded slice segment of a CRA picture slice_segment_layer_rbsp( )"
    << "RSV_IRAP_VCL22 Reserved IRAP VCL NAL unit types"
    << "RSV_IRAP_VCL23 Reserved IRAP VCL NAL unit types"
    << "RSV_VCL24 Reserved non-IRAP VCL NAL unit types"
    << "RSV_VCL25 Reserved non-IRAP VCL NAL unit types"
    << "RSV_VCL26 Reserved non-IRAP VCL NAL unit types"
    << "RSV_VCL27 Reserved non-IRAP VCL NAL unit types"
    << "RSV_VCL28 Reserved non-IRAP VCL NAL unit types"
    << "RSV_VCL29 Reserved non-IRAP VCL NAL unit types"
    << "RSV_VCL30 Reserved non-IRAP VCL NAL unit types"
    << "RSV_VCL31 Reserved non-IRAP VCL NAL unit types"
    << "VPS_NUT Video parameter set video_parameter_set_rbsp( )"
    << "SPS_NUT Sequence parameter set seq_parameter_set_rbsp( )"
    << "PPS_NUT Picture parameter set pic_parameter_set_rbsp( )"
    << "AUD_NUT Access unit delimiter access_unit_delimiter_rbsp( )"
    << "EOS_NUT End of sequence end_of_seq_rbsp( )"
    << "EOB_NUT End of bitstream end_of_bitstream_rbsp( )"
    << "FD_NUT Filler data filler_data_rbsp( )"
    << "PREFIX_SEI_NUT Supplemental enhancement information sei_rbsp( )"
    << "SUFFIX_SEI_NUT Supplemental enhancement information sei_rbsp( )"
    << "RSV_NVCL41 Reserved"
    << "RSV_NVCL42 Reserved"
    << "RSV_NVCL43 Reserved"
    << "RSV_NVCL44 Reserved"
    << "RSV_NVCL45 Reserved"
    << "RSV_NVCL46 Reserved"
    << "RSV_NVCL47 Reserved"
    << "Unspecified";
  READBITS_M(nal_unit_type_id, 6, nal_unit_type_id_meaning);
  READBITS(nuh_layer_id, 6);
  READBITS(nuh_temporal_id_plus1, 3);

  // Set the nal unit type
  nal_type = (nal_unit_type_id > UNSPECIFIED) ? UNSPECIFIED : (nal_unit_type)nal_unit_type_id;

  return true;
}

bool parserAnnexBHEVC::nal_unit_hevc::isIRAP()
{ 
  return (nal_type == BLA_W_LP       || nal_type == BLA_W_RADL ||
    nal_type == BLA_N_LP       || nal_type == IDR_W_RADL ||
    nal_type == IDR_N_LP       || nal_type == CRA_NUT    ||
    nal_type == RSV_IRAP_VCL22 || nal_type == RSV_IRAP_VCL23); 
}

bool parserAnnexBHEVC::nal_unit_hevc::isSLNR() 
{ 
  return (nal_type == TRAIL_N     || nal_type == TSA_N       ||
    nal_type == STSA_N      || nal_type == RADL_N      ||
    nal_type == RASL_N      || nal_type == RSV_VCL_N10 ||
    nal_type == RSV_VCL_N12 || nal_type == RSV_VCL_N14); 
}

bool parserAnnexBHEVC::nal_unit_hevc::isRADL() { 
  return (nal_type == RADL_N || nal_type == RADL_R); 
}

bool parserAnnexBHEVC::nal_unit_hevc::isRASL() 
{ 
  return (nal_type == RASL_N || nal_type == RASL_R); 
}

bool parserAnnexBHEVC::nal_unit_hevc::isSlice() 
{ 
  return (nal_type == IDR_W_RADL || nal_type == IDR_N_LP   || nal_type == CRA_NUT  ||
    nal_type == BLA_W_LP   || nal_type == BLA_W_RADL || nal_type == BLA_N_LP ||
    nal_type == TRAIL_N    || nal_type == TRAIL_R    || nal_type == TSA_N    ||
    nal_type == TSA_R      || nal_type == STSA_N     || nal_type == STSA_R   ||
    nal_type == RADL_N     || nal_type == RADL_R     || nal_type == RASL_N   ||
    nal_type == RASL_R); 
}

int parserAnnexBHEVC::sei::parse_sei_header(const QByteArray &sliceHeaderData, TreeItem *root)
{
  reader_helper reader(sliceHeaderData, root, "sei_message()");

  payloadType = 0;
  {
    QMap<int, QString> variableNames;
    variableNames.insert(255, "ff_byte");
    variableNames.insert(-1, "last_payload_type_byte");
    unsigned int byte;
    do
    {
      if (!reader.readBits(8, byte, variableNames))
        return -1;

      payloadType += byte;
    } while (byte == 255);
  }

  if (nal_type == PREFIX_SEI_NUT)
  {
    if (payloadType == 0)
      payloadTypeName = "buffering_period";
    else if (payloadType == 1)
      payloadTypeName = "pic_timing";
    else if (payloadType == 2)
      payloadTypeName = "pan_scan_rect";
    else if (payloadType == 3)
      payloadTypeName = "filler_payload";
    else if (payloadType == 4)
      payloadTypeName = "user_data_registered_itu_t_t35";
    else if (payloadType == 5)
      payloadTypeName = "user_data_unregistered";
    else if (payloadType == 6)
      payloadTypeName = "recovery_point";
    else if (payloadType == 9)
      payloadTypeName = "scene_info";
    else if (payloadType == 15)
      payloadTypeName = "picture_snapshot";
    else if (payloadType == 16)
      payloadTypeName = "progressive_refinement_segment_start";
    else if (payloadType == 17)
      payloadTypeName = "progressive_refinement_segment_end";
    else if (payloadType == 19)
      payloadTypeName = "film_grain_characteristics";
    else if (payloadType == 22)
      payloadTypeName = "post_filter_hint";
    else if (payloadType == 23)
      payloadTypeName = "tone_mapping_info";
    else if (payloadType == 45)
      payloadTypeName = "frame_packing_arrangement";
    else if (payloadType == 47)
      payloadTypeName = "display_orientation";
    else if (payloadType == 56)
      payloadTypeName = "green_metadata"; /* specified in ISO/IEC 23001-11 */
    else if (payloadType == 128)
      payloadTypeName = "structure_of_pictures_info";
    else if (payloadType == 129)
      payloadTypeName = "active_parameter_sets";
    else if (payloadType == 130)
      payloadTypeName = "decoding_unit_info";
    else if (payloadType == 131)
      payloadTypeName = "temporal_sub_layer_zero_index";
    else if (payloadType == 133)
      payloadTypeName = "scalable_nesting";
    else if (payloadType == 134)
      payloadTypeName = "region_refresh_info";
    else if (payloadType == 135)
      payloadTypeName = "no_display";
    else if (payloadType == 136)
      payloadTypeName = "time_code";
    else if (payloadType == 137)
      payloadTypeName = "mastering_display_colour_volume";
    else if (payloadType == 138)
      payloadTypeName = "segmented_rect_frame_packing_arrangement";
    else if (payloadType == 139)
      payloadTypeName = "temporal_motion_constrained_tile_sets";
    else if (payloadType == 140)
      payloadTypeName = "chroma_resampling_filter_hint";
    else if (payloadType == 141)
      payloadTypeName = "knee_function_info";
    else if (payloadType == 142)
      payloadTypeName = "colour_remapping_info";
    else if (payloadType == 143)
      payloadTypeName = "deinterlaced_field_identification";
    else if (payloadType == 144)
      payloadTypeName = "content_light_level_info";
    else if (payloadType == 145)
      payloadTypeName = "dependent_rap_indication";
    else if (payloadType == 146)
      payloadTypeName = "coded_region_completion";
    else if (payloadType == 147)
      payloadTypeName = "alternative_transfer_characteristics";
    else if (payloadType == 148)
      payloadTypeName = "ambient_viewing_environment";
    else if (payloadType == 160)
      payloadTypeName = "layers_not_present"; /* specified in Annex F */
    else if (payloadType == 161)
      payloadTypeName = "inter_layer_constrained_tile_sets"; /* specified in Annex F */
    else if (payloadType == 162)
      payloadTypeName = "bsp_nesting"; /* specified in Annex F */
    else if (payloadType == 163)
      payloadTypeName = "bsp_initial_arrival_time"; /* specified in Annex F */
    else if (payloadType == 164)
      payloadTypeName = "sub_bitstream_property"; /* specified in Annex F */
    else if (payloadType == 165)
      payloadTypeName = "alpha_channel_info"; /* specified in Annex F */
    else if (payloadType == 166)
      payloadTypeName = "overlay_info"; /* specified in Annex F */
    else if (payloadType == 167)
      payloadTypeName = "temporal_mv_prediction_constraints"; /* specified in Annex F */
    else if (payloadType == 168)
      payloadTypeName = "frame_field_info"; /* specified in Annex F */
    else if (payloadType == 176)
      payloadTypeName = "three_dimensional_reference_displays_info"; /* specified in Annex G */
    else if (payloadType == 177)
      payloadTypeName = "depth_representation_info"; /* specified in Annex G */
    else if (payloadType == 178)
      payloadTypeName = "multiview_scene_info"; /* specified in Annex G */
    else if (payloadType == 179)
      payloadTypeName = "multiview_acquisition_info"; /* specified in Annex G */
    else if (payloadType == 180)
      payloadTypeName = "multiview_view_position"; /* specified in Annex G */
    else if (payloadType == 181)
      payloadTypeName = "alternative_depth_info"; /* specified in Annex I */
    else
      payloadTypeName = "reserved_sei_message";
  }
  else /* nal_unit_type == SUFFIX_SEI_NUT */
  {
    if (payloadType == 3)
      payloadTypeName = "filler_payload";
    else if (payloadType == 4)
      payloadTypeName = "user_data_registered_itu_t_t35";
    else if (payloadType == 5)
      payloadTypeName = "user_data_unregistered";
    else if (payloadType == 17)
      payloadTypeName = "progressive_refinement_segment_end";
    else if (payloadType == 22)
      payloadTypeName = "post_filter_hint";
    else if (payloadType == 132)
      payloadTypeName = "decoded_picture_hash";
    else if (payloadType == 146)
      payloadTypeName = "coded_region_completion";
    else
      payloadTypeName = "reserved_sei_message";
  }

  LOGVAL_M(payloadType, payloadTypeName);

  payloadSize = 0;
  {
    QMap<int, QString> variableNames;
    variableNames.insert(255, "ff_byte");
    variableNames.insert(-1, "last_payload_size_byte");
    unsigned int byte;
    do
    {
      if (!reader.readBits(8, byte, variableNames))
        return -1;

      payloadSize += byte;
    } while (byte == 255);
  }
  LOGVAL(payloadSize);

  return reader.nrBytesRead();
}

parserAnnexB::sei_parsing_return_t parserAnnexBHEVC::sei::parser_sei_bytes(QByteArray &data, TreeItem *root)
{
  if (root == nullptr)
    return SEI_PARSING_OK;

  // Create a new TreeItem root for the item
  TreeItem *const itemTree = new TreeItem("raw_bytes()", root);

  for (int i=0; i<data.length(); i++)
  {
    unsigned char c = data[i];
    QString binary;
    for (int j=7; j>=0; j--)
      if (c & (1<<j))
        binary.append("1");
      else
        binary.append("0");

    new TreeItem(QString("data[%1]").arg(i), c, QString("u(8)"), binary, itemTree);
  }

  return SEI_PARSING_OK;
}

parserAnnexB::sei_parsing_return_t parserAnnexBHEVC::user_data_sei::parse_user_data_sei(QByteArray &sei_data, TreeItem *root)
{
  user_data_UUID = sei_data.mid(0, 16).toHex();
  user_data_message = sei_data.mid(16);

  if (!root)
    return SEI_PARSING_OK;

  if (sei_data.mid(16, 4) == "x265")
  {
    // This seems to be x264 user data. These contain the encoder settings which might be useful
    // Create a new TreeItem root for the item
    // The macros will use this variable to add all the parsed variables
    TreeItem *const itemTree = new TreeItem("x265 user data", root);
    new TreeItem("UUID", user_data_UUID, "u(128)", "", "random ID number generated according to ISO-11578", itemTree);

    QStringList list = user_data_message.split(QRegExp("[\r\n\t ]+"), QString::SkipEmptyParts);
    bool options = false;
    QString aggregate_string;
    for (QString val : list)
    {
      if (options)
      {
        QStringList option = val.split("=");
        if (option.length() == 2)
          new TreeItem(option[0], option[1], "", "", "", itemTree);
      }
      else
      {
        if (val == "-")
        {
          if (aggregate_string != " -" && aggregate_string != "-" && !aggregate_string.isEmpty())
            new TreeItem("Info", aggregate_string, "", "", "", itemTree);
          aggregate_string = "";
        }
        else if (val == "options:")
        {
          options = true;
          if (aggregate_string != " -" && aggregate_string != "-" && !aggregate_string.isEmpty())
            new TreeItem("Info", aggregate_string, "", "", "", itemTree);
        }
        else
          aggregate_string += " " + val;
      }
    }
  }
  else
  {
    // Just log the data as a string
    TreeItem *const itemTree = new TreeItem("custom user data", root);
    new TreeItem("UUID", user_data_UUID, "u(128)", "", "random ID number generated according to ISO-11578", itemTree);
    new TreeItem("User Data", QString(user_data_message), "", "", "", itemTree);
  }
  return SEI_PARSING_OK;
}

bool parserAnnexBHEVC::alternative_transfer_characteristics_sei::parse_internal(QByteArray &data, TreeItem *root)
{
  reader_helper reader(data, root, "alternative transfer characteristics");
  READBITS_M(preferred_transfer_characteristics, 8, get_transfer_characteristics_meaning());
  return true;
}

parserAnnexB::sei_parsing_return_t parserAnnexBHEVC::active_parameter_sets_sei::parse_active_parameter_sets_sei(QByteArray &data, const vps_map &active_VPS_list, TreeItem *root)
{
  reader.init(data, root, "active parameter sets");
  if (!parse_vps_id())
    return SEI_PARSING_ERROR;
  if (is_reparse_needed(active_VPS_list))
    return SEI_PARSING_WAIT_FOR_PARAMETER_SETS;
  if (!parse_internal(active_VPS_list))
    return SEI_PARSING_ERROR;
  return SEI_PARSING_OK;
}

bool parserAnnexBHEVC::active_parameter_sets_sei::parse_vps_id()
{
  READBITS(active_video_parameter_set_id, 4);
  return true;
}

bool parserAnnexBHEVC::active_parameter_sets_sei::parse_internal(const vps_map &active_VPS_list)
{
  if (is_reparse_needed(active_VPS_list))
    return false;

  if (!active_VPS_list.contains(active_video_parameter_set_id))
    return reader.addErrorMessageChildItem("The signaled VPS was not found in the bitstream.");
  QSharedPointer<vps> actVPS = active_VPS_list.value(active_video_parameter_set_id);

  READFLAG(self_contained_cvs_flag);
  READFLAG(no_parameter_set_update_flag);
  READUEV(num_sps_ids_minus1);
  for (unsigned int i=0; i<=num_sps_ids_minus1; i++)
  {
    READUEV_A(active_seq_parameter_set_id, i);
  }
  unsigned int MaxLayersMinus1 = std::min((unsigned int)62, actVPS->vps_max_layers_minus1);
  for (unsigned int i = (actVPS->vps_base_layer_internal_flag ? 1 : 0); i <= MaxLayersMinus1; i++)
    READUEV_A(layer_sps_idx, i);
  return true;
}

parserAnnexBHEVC::sei_parsing_return_t parserAnnexBHEVC::buffering_period_sei::parse_buffering_period_sei(QByteArray &data, const sps_map &active_SPS_list, TreeItem *root)
{
  reader.init(data, root, "buffering period");
  if (!parse_sps_id())
    return SEI_PARSING_ERROR;
  if (is_reparse_needed(active_SPS_list))
    return SEI_PARSING_WAIT_FOR_PARAMETER_SETS;
  if (!parse_internal(active_SPS_list))
    return SEI_PARSING_ERROR;
  return SEI_PARSING_OK;
}

bool parserAnnexBHEVC::buffering_period_sei::parse_sps_id()
{
  READUEV(bp_seq_parameter_set_id);
  if (bp_seq_parameter_set_id > 15)
    return reader.addErrorMessageChildItem("The value of bp_seq_parameter_set_id shall be in the range of 0 to 15, inclusive.");
  return true;
}

bool parserAnnexBHEVC::buffering_period_sei::parse_internal(const sps_map &active_SPS_list)
{
  if (is_reparse_needed(active_SPS_list))
    return false;

  if (!active_SPS_list.contains(bp_seq_parameter_set_id))
    return reader.addErrorMessageChildItem("The signaled SPS was not found in the bitstream.");
  QSharedPointer<sps> actSPS = active_SPS_list.value(bp_seq_parameter_set_id);

  // Get the hrd parameters. TODO: It this really always correct?
  hrd_parameters hrd;
  if (actSPS->vui_parameters_present_flag && actSPS->sps_vui_parameters.vui_hrd_parameters_present_flag)
    hrd = actSPS->sps_vui_parameters.vui_hrd_parameters;
  else
    return reader.addErrorMessageChildItem("Could not found the needed HRD parameters.");

  if (!hrd.sub_pic_hrd_params_present_flag)
    READFLAG(irap_cpb_params_present_flag);
  if (irap_cpb_params_present_flag)
  {
    int nrBits = hrd.au_cpb_removal_delay_length_minus1 + 1;
    READBITS(cpb_delay_offset, nrBits);
    nrBits = hrd.dpb_output_delay_length_minus1 + 1;
    READBITS(dpb_delay_offset, nrBits);
  }
  READFLAG(concatenation_flag);
  int nrBits = hrd.au_cpb_removal_delay_length_minus1 + 1;
  READBITS(au_cpb_removal_delay_delta_minus1, nrBits);

  bool NalHrdBpPresentFlag = hrd.nal_hrd_parameters_present_flag;
  // TODO: Really not sure if this is correct
  int CpbCnt = hrd.cpb_cnt_minus1[0] + 1;
  if (NalHrdBpPresentFlag)
  {
    for (int i=0; i<CpbCnt; i++)
    {
      const int nrBits = hrd.initial_cpb_removal_delay_length_minus1 + 1;
      READBITS_A(nal_initial_cpb_removal_delay, nrBits, i);
      READBITS_A(nal_initial_cpb_removal_offset, nrBits, i);
      if (hrd.sub_pic_hrd_params_present_flag || irap_cpb_params_present_flag)
      {
        READBITS_A(nal_initial_alt_cpb_removal_delay, nrBits, i);
        READBITS_A(nal_initial_alt_cpb_removal_offset, nrBits, i);
      }
    }
  }
  bool VclHrdBpPresentFlag = hrd.vcl_hrd_parameters_present_flag;
  if (VclHrdBpPresentFlag)
  {
    for (int i=0; i<CpbCnt; i++)
    {
      const int nrBits = hrd.initial_cpb_removal_delay_length_minus1 + 1;
      READBITS_A(vcl_initial_cpb_removal_delay, nrBits, i);
      READBITS_A(vcl_initial_cpb_removal_offset, nrBits, i);
      if (hrd.sub_pic_hrd_params_present_flag || irap_cpb_params_present_flag)
      {
        READBITS_A(vcl_initial_alt_cpb_removal_delay, nrBits, i);
        READBITS_A(vcl_initial_alt_cpb_removal_offset, nrBits, i);
      }
    }
  }
  if (reader.payload_extension_present())
    READFLAG(use_alt_cpb_params_flag);

  return true;
}

parserAnnexBHEVC::sei_parsing_return_t parserAnnexBHEVC::pic_timing_sei::parse_pic_timing_sei(QByteArray &sliceHeaderData, const vps_map &active_VPS_list, const sps_map &active_SPS_list, TreeItem *root)
{
  rootItem = root;
  sei_data_storage = sliceHeaderData;
  if (is_reparse_needed(active_VPS_list, active_SPS_list))
    return SEI_PARSING_WAIT_FOR_PARAMETER_SETS;
  if (!parse_internal(active_VPS_list, active_SPS_list))
    return SEI_PARSING_ERROR;
  return SEI_PARSING_OK;
}

bool parserAnnexBHEVC::pic_timing_sei::is_reparse_needed(const vps_map &active_VPS_list, const sps_map &active_SPS_list)
{
  // TODO: Is this really ID 0? The standard does not really say which one (or I did not find it).
  if (!active_SPS_list.contains(0))
    return true;
  if (!active_VPS_list.contains(0))
    return true;
  return false;
}

bool parserAnnexBHEVC::pic_timing_sei::parse_internal(const vps_map &active_VPS_list, const sps_map &active_SPS_list)
{
  if (is_reparse_needed(active_VPS_list, active_SPS_list))
    return false;

  reader_helper reader(sei_data_storage, rootItem, "picture timing");

  QSharedPointer<sps> actSPS = active_SPS_list.value(0);
  QSharedPointer<vps> actVPS = active_VPS_list.value(0);

  if (actSPS->sps_vui_parameters.frame_field_info_present_flag)
  {
    QStringList pic_struct_meaning = QStringList()
      << "(progressive) Frame"
      << "Top field"
      << "Bottom field"
      << "Top field, bottom field, in that order"
      << "Bottom field, top field, in that order"
      << "Top field, bottom field, top field repeated, in that order"
      << "Bottom field, top field, bottom field repeated, in that order"
      << "Frame doubling"
      << "Frame tripling"
      << "Top field paired with previous bottom field in output order"
      << "Bottom field paired with previous top field in output order"
      << "Top field paired with next bottom field in output order"
      << "Bottom field paired with next top field in output order"
      << "Reserved for future use by ITU-T | ISO/IEC";
    READBITS_M(pic_struct, 4, pic_struct_meaning);
    QStringList source_scan_type_meaning = QStringList()  << "interlaced" << "progressive" << "unknown or unspecified" << "reserved for future use by ITU-T | ISO/IEC";
    READBITS_M(source_scan_type, 2, source_scan_type_meaning);
    READFLAG(duplicate_flag);
  }

  // Get the hrd parameters. TODO: It this really always correct?
  hrd_parameters hrd;
  if (actSPS->vui_parameters_present_flag && actSPS->sps_vui_parameters.vui_hrd_parameters_present_flag)
    hrd = actSPS->sps_vui_parameters.vui_hrd_parameters;
  else if (actVPS->vps_timing_info_present_flag && actVPS->vps_num_hrd_parameters > 0)
    // What if there are multiple sets?
    hrd = actVPS->vps_hrd_parameters[0];

  // true if nal_hrd_parameters_present_flag or vcl_hrd_parameters_present_flag is present in the bitstream and is equal to 1.
  bool CpbDpbDelaysPresentFlag = (hrd.nal_hrd_parameters_present_flag || hrd.vcl_hrd_parameters_present_flag);

  if (CpbDpbDelaysPresentFlag)
  {
    int nr_bits = hrd.au_cpb_removal_delay_length_minus1 + 1;
    READBITS(au_cpb_removal_delay_minus1, nr_bits);
    nr_bits = hrd.dpb_output_delay_length_minus1 + 1;
    READBITS(pic_dpb_output_delay, nr_bits);
    if(hrd.sub_pic_hrd_params_present_flag)
    {
      nr_bits = hrd.dpb_output_delay_du_length_minus1 + 1;
      READBITS(pic_dpb_output_du_delay, nr_bits);
    }
    if(hrd.sub_pic_hrd_params_present_flag && hrd.sub_pic_cpb_params_in_pic_timing_sei_flag)
    {
      READUEV(num_decoding_units_minus1);
      if (num_decoding_units_minus1 > actSPS->PicSizeInCtbsY - 1)
        return reader.addErrorMessageChildItem("The value of num_decoding_units_minus1 shall be in the range of 0 to PicSizeInCtbsY - 1, inclusive.");
      READFLAG(du_common_cpb_removal_delay_flag);
      if (du_common_cpb_removal_delay_flag)
      {
        nr_bits = hrd.du_cpb_removal_delay_increment_length_minus1 + 1;
        READBITS(du_common_cpb_removal_delay_increment_minus1, nr_bits);
      }
      for (unsigned int i=0; i<=num_decoding_units_minus1; i++)
      {
        READUEV_A(num_nalus_in_du_minus1, i);
        if (!du_common_cpb_removal_delay_flag && i < num_decoding_units_minus1)
        {
          nr_bits = hrd.du_cpb_removal_delay_increment_length_minus1 + 1;
          READBITS_A(du_cpb_removal_delay_increment_minus1, nr_bits, i);
        }
      }
    }
  }

  return true;
}

QStringList parserAnnexBHEVC::get_colour_primaries_meaning()
{
  QStringList colour_primaries_meaning = QStringList() 
    << "Reserved For future use by ITU-T | ISO/IEC"
    << "Rec. ITU-R BT.709-6 / BT.1361 / IEC 61966-2-1 (sRGB or sYCC)"
    << "Unspecified"
    << "Reserved For future use by ITU - T | ISO / IEC"
    << "Rec. ITU-R BT.470-6 System M (historical) (NTSC)"
    << "Rec. ITU-R BT.470-6 System B, G (historical) / BT.601 / BT.1358 / BT.1700 PAL and 625 SECAM"
    << "Rec. ITU-R BT.601-6 525 / BT.1358 525 / BT.1700 NTSC"
    << "Society of Motion Picture and Television Engineers 240M (1999)"
    << "Generic film (colour filters using Illuminant C)"
    << "Rec. ITU-R BT.2020-2 Rec. ITU-R BT.2100-0"
    << "SMPTE ST 428-1 (CIE 1931 XYZ)"
    << "SMPTE RP 431-2 (2011)"
    << "SMPTE EG 432-1 (2010)"
    << "Reserved For future use by ITU - T | ISO / IEC"
    << "Reserved For future use by ITU - T | ISO / IEC"
    << "Reserved For future use by ITU - T | ISO / IEC"
    << "Reserved For future use by ITU - T | ISO / IEC"
    << "Reserved For future use by ITU - T | ISO / IEC"
    << "Reserved For future use by ITU - T | ISO / IEC"
    << "Reserved For future use by ITU - T | ISO / IEC"
    << "Reserved For future use by ITU - T | ISO / IEC"
    << "Reserved For future use by ITU - T | ISO / IEC"
    << "EBU Tech. 3213-E (1975)"
    << "Reserved For future use by ITU - T | ISO / IEC";
  return colour_primaries_meaning;
}

QStringList parserAnnexBHEVC::get_transfer_characteristics_meaning()
{
  QStringList transfer_characteristics_meaning = QStringList()
    << "Reserved For future use by ITU-T | ISO/IEC"
    << "Rec. ITU-R BT.709-6 Rec.ITU - R BT.1361-0 conventional colour gamut system"
    << "Unspecified"
    << "Reserved For future use by ITU-T | ISO / IEC"
    << "Rec. ITU-R BT.470-6 System M (historical) (NTSC)"
    << "Rec. ITU-R BT.470-6 System B, G (historical)"
    << "Rec. ITU-R BT.601-6 525 or 625, Rec.ITU - R BT.1358 525 or 625, Rec.ITU - R BT.1700 NTSC Society of Motion Picture and Television Engineers 170M(2004)"
    << "Society of Motion Picture and Television Engineers 240M (1999)"
    << "Linear transfer characteristics"
    << "Logarithmic transfer characteristic (100:1 range)"
    << "Logarithmic transfer characteristic (100 * Sqrt( 10 ) : 1 range)"
    << "IEC 61966-2-4"
    << "Rec. ITU-R BT.1361 extended colour gamut system"
    << "IEC 61966-2-1 (sRGB or sYCC)"
    << "Rec. ITU-R BT.2020-2 for 10 bit system"
    << "Rec. ITU-R BT.2020-2 for 12 bit system"
    << "SMPTE ST 2084 for 10, 12, 14 and 16-bit systems Rec. ITU-R BT.2100-0 perceptual quantization (PQ) system"
    << "SMPTE ST 428-1"
    << "Association of Radio Industries and Businesses (ARIB) STD-B67 Rec. ITU-R BT.2100-0 hybrid log-gamma (HLG) system"
    << "Reserved For future use by ITU-T | ISO/IEC";
  return transfer_characteristics_meaning;
}

QStringList parserAnnexBHEVC::get_matrix_coefficients_meaning()
{
  QStringList matrix_coefficients_meaning = QStringList()
    << "The identity matrix. RGB IEC 61966-2-1 (sRGB)"
    << "Rec. ITU-R BT.709-6, Rec. ITU-R BT.1361-0"
    << "Image characteristics are unknown or are determined by the application"
    << "For future use by ITU-T | ISO/IEC"
    << "United States Federal Communications Commission Title 47 Code of Federal Regulations (2003) 73.682 (a) (20)"
    << "Rec. ITU-R BT.470-6 System B, G (historical), Rec. ITU-R BT.601-6 625, Rec. ITU-R BT.1358 625, Rec. ITU-R BT.1700 625 PAL and 625 SECAM"
    << "Rec. ITU-R BT.601-6 525, Rec. ITU-R BT.1358 525, Rec. ITU-R BT.1700 NTSC, Society of Motion Picture and Television Engineers 170M (2004)"
    << "Society of Motion Picture and Television Engineers 240M (1999)"
    << "YCgCo"
    << "Rec. ITU-R BT.2020-2 non-constant luminance system"
    << "Rec. ITU-R BT.2020-2 constant luminance system"
    << "SMPTE ST 2085 (2015)"
    << "Chromaticity-derived non-constant luminance system"
    << "Chromaticity-derived constant luminance system"
    << "Rec. ITU-R BT.2100-0 ICTCP"
    << "For future use by ITU-T | ISO/IEC";
  return matrix_coefficients_meaning;
}
