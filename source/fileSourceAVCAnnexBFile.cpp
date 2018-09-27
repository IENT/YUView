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

#include "fileSourceAVCAnnexBFile.h"

#include <cmath>
#include <QDebug>

// Read "numBits" bits into the variable "into". 
#define READBITS(into,numBits) {QString code; into=reader.readBits(numBits, &code); if (itemTree) new TreeItem(#into,into,QString("u(v) -> u(%1)").arg(numBits),code, itemTree);}
#define READBITS_M(into,numBits,meanings) {QString code; into=reader.readBits(numBits, &code); if (itemTree) new TreeItem(#into,into,QString("u(v) -> u(%1)").arg(numBits),code, meanings,itemTree);}
#define READBITS_A(into,numBits,i) {QString code; int v=reader.readBits(numBits,&code); into.append(v); if (itemTree) new TreeItem(QString(#into)+QString("[%1]").arg(i),v,QString("u(v) -> u(%1)").arg(numBits),code, itemTree);}
// Read a flag (1 bit) into the variable "into".
#define READFLAG(into) {into=(reader.readBits(1)!=0); if (itemTree) new TreeItem(#into,into,QString("u(1)"),(into!=0)?"1":"0",itemTree);}
#define READFLAG_M(into,meanings) {into=(reader.readBits(1)!=0); if (itemTree) new TreeItem(#into,into,QString("u(1)"),(into!=0)?"1":"0",meanings,itemTree);}
#define READFLAG_A(into,i) {bool b=(reader.readBits(1)!=0); into.append(b); if (itemTree) new TreeItem(QString(#into)+QString("[%1]").arg(i),b,QString("u(1)"),b?"1":"0",itemTree);}
// Read a unsigned ue(v) code from the bitstream into the variable "into"
#define READUEV(into) {QString code; into=reader.readUE_V(&code); if (itemTree) new TreeItem(#into,into,QString("ue(v)"),code,itemTree);}
#define READUEV_M(into,meanings) {QString code; into=reader.readUE_V(&code); if (itemTree) new TreeItem(#into,into,QString("ue(v)"),code,meanings,itemTree);}
#define READUEV_A(arr,i) {QString code; int v=reader.readUE_V(&code); arr.append(v); if (itemTree) new TreeItem(QString(#arr)+QString("[%1]").arg(i),v,QString("ue(v)"),code,itemTree);}
#define READUEV_APP(arr) {QString code; int v=reader.readUE_V(&code); arr.append(v); if (itemTree) new TreeItem(QString(#arr),v,QString("ue(v)"),code,itemTree);}
// Read a signed se(v) code from the bitstream into the variable "into"
#define READSEV(into) {QString code; into=reader.readSE_V(&code); if (itemTree) new TreeItem(#into,into,QString("se(v)"),code,itemTree);}
#define READSEV_A(into,i) {QString code; int v=reader.readSE_V(&code); into.append(v); if (itemTree) new TreeItem(QString(#into)+QString("[%1]").arg(i),v,QString("se(v)"),code,itemTree);}
// Do not actually read anything but also put the value into the tree as a calculated value
#define LOGVAL(val) {if (itemTree) new TreeItem(#val,val,QString("calc"),QString(),itemTree);}
#define LOGVAL_M(val,meaning) {if (itemTree) new TreeItem(#val,val,QString("calc"),QString(),meaning,itemTree);}
// Log a custom message (add a cutom item in the tree)
#define LOGPARAM(name, val, coding, code, meaning) {if (itemTree) new TreeItem(name, val, coding, code, meaning, itemTree);}

double fileSourceAVCAnnexBFile::getFramerate() const
{
  return 0.0;
}

void fileSourceAVCAnnexBFile::parseAndAddNALUnit(int nalID)
{
  // Save the position of the first byte of the start code
  quint64 curFilePos = tell() - 3;

  // Read two bytes (the nal header)
  QByteArray nalHeaderBytes;
  nalHeaderBytes.append(getCurByte());
  gotoNextByte();

  // Create a new TreeItem root for the NAL unit. We don't set data (a name) for this item
  // yet. We want to parse the item and then set a good description.
  QString specificDescription;
  TreeItem *nalRoot = nullptr;
  if (!nalUnitModel.rootItem.isNull())
    nalRoot = new TreeItem(nalUnitModel.rootItem.data());

  // Create a nal_unit and read the header
  nal_unit_avc nal_avc(curFilePos, nalID);
  nal_avc.parse_nal_unit_header(nalHeaderBytes, nalRoot);

  if (nal_avc.nal_unit_type == SPS)
  {
    // A sequence parameter set
    auto new_sps = QSharedPointer<sps>(new sps(nal_avc));
    new_sps->parse_sps(getRemainingNALBytes(), nalRoot);
      
    // Add sps (replace old one if existed)
    active_SPS_list.insert(new_sps->seq_parameter_set_id, new_sps);

    // Also add sps to list of all nals
    nalUnitList.append(new_sps);

    // Add the SPS ID
    specificDescription = QString(" SPS_NUT ID %1").arg(new_sps->seq_parameter_set_id);

    if (new_sps->vui_parameters.nal_hrd_parameters_present_flag || new_sps->vui_parameters.vcl_hrd_parameters_present_flag)
      CpbDpbDelaysPresentFlag = true;
  }
  else if (nal_avc.nal_unit_type == PPS) 
  {
    // A picture parameter set
    auto new_pps = QSharedPointer<pps>(new pps(nal_avc));
    new_pps->parse_pps(getRemainingNALBytes(), nalRoot, active_SPS_list);
      
    // Add pps (replace old one if existed)
    active_PPS_list.insert(new_pps->pic_parameter_set_id, new_pps);

    // Also add pps to list of all nals
    nalUnitList.append(new_pps);

    // Add the PPS ID
    specificDescription = QString(" PPS_NUT ID %1").arg(new_pps->pic_parameter_set_id);
  }
  else if (nal_avc.isSlice())
  {
    // Create a new slice unit
    auto new_slice = QSharedPointer<slice_header>(new slice_header(nal_avc));
    new_slice->parse_slice_header(getRemainingNALBytes(), active_SPS_list, active_PPS_list, last_picture_first_slice, nalRoot);

    if (!new_slice->bottom_field_flag && 
       (last_picture_first_slice.isNull() || new_slice->TopFieldOrderCnt != last_picture_first_slice->TopFieldOrderCnt) &&
        new_slice->first_mb_in_slice == 0)
      last_picture_first_slice = new_slice;

    // Add the POC of the slice
    specificDescription = QString(" POC %1").arg(new_slice->globalPOC);

    // Get the poc and add it to the POC list
    if (new_slice->globalPOC >= 0)
      addPOCToList(new_slice->globalPOC);

    if (nal_avc.isRandomAccess())
    {
      if (new_slice->first_mb_in_slice == 0)
        // This is the first slice of a random access pont. Add it to the list.
        nalUnitList.append(new_slice);
    }
  }
  else if (nal_avc.nal_unit_type == SEI)
  {
    // Create a new SEI
    auto new_sei = QSharedPointer<sei>(new sei(nal_avc));
    QByteArray sei_data = getRemainingNALBytes();
    int nrBytes = new_sei->parse_sei_message(sei_data, nalRoot);
    sei_data.remove(0, nrBytes);

    specificDescription = QString(" payloadType %1").arg(new_sei->payloadType);
    if (!new_sei->payloadTypeName.isEmpty())
      specificDescription += " - " + new_sei->payloadTypeName;

    if (new_sei->payloadType == 0)
    {
      auto new_buffering_period_sei = QSharedPointer<buffering_period_sei>(new buffering_period_sei(new_sei));
      new_buffering_period_sei->parse_buffering_period_sei(sei_data, active_SPS_list, nalRoot);
    }
    else if (new_sei->payloadType == 1)
    {
      auto new_pic_timing_sei = QSharedPointer<pic_timing_sei>(new pic_timing_sei(new_sei));
      new_pic_timing_sei->parse_pic_timing_sei(sei_data, active_SPS_list, CpbDpbDelaysPresentFlag, nalRoot);
    }
    else if (new_sei->payloadType == 5)
    {
      auto new_user_data_sei = QSharedPointer<user_data_sei>(new user_data_sei(new_sei));
      new_user_data_sei->parse_user_data_sei(sei_data, nalRoot);
    }
  }

  if (nalRoot)
    // Set a useful name of the TreeItem (the root for this NAL)
    nalRoot->itemData.append(QString("NAL %1: %2").arg(nal_avc.nal_idx).arg(nal_unit_type_toString.value(nal_avc.nal_unit_type)) + specificDescription);
}

const QStringList fileSourceAVCAnnexBFile::nal_unit_type_toString = QStringList()
   << "UNSPECIFIED" << "CODED_SLICE_NON_IDR" << "CODED_SLICE_DATA_PARTITION_A" << "CODED_SLICE_DATA_PARTITION_B" << "CODED_SLICE_DATA_PARTITION_C" 
   << "CODED_SLICE_IDR" << "SEI" << "SPS" << "PPS" << "AUD" << "END_OF_SEQUENCE" << "END_OF_STREAM" << "FILLER" << "SPS_EXT" << "PREFIX_NAL" 
   << "SUBSET_SPS" << "DEPTH_PARAMETER_SET" << "RESERVED_17" << "RESERVED_18" << "CODED_SLICE_AUX" << "CODED_SLICE_EXTENSION" << "CODED_SLICE_EXTENSION_DEPTH_MAP" 
   << "RESERVED_22" << "RESERVED_23" << "UNSPCIFIED_24" << "UNSPCIFIED_25" << "UNSPCIFIED_26" << "UNSPCIFIED_27" << "UNSPCIFIED_28" << "UNSPCIFIED_29" 
   << "UNSPCIFIED_30" << "UNSPCIFIED_31";

void fileSourceAVCAnnexBFile::nal_unit_avc::parse_nal_unit_header(const QByteArray &parameterSetData, TreeItem *root)
{
  // Create a sub byte parser to access the bits
  sub_byte_reader reader(parameterSetData);

  // Create a new TreeItem root for the item
  // The macros will use this variable to add all the parsed variables
  TreeItem *const itemTree = root ? new TreeItem("nal_unit_header()", root) : nullptr;

  // Read forbidden_zeor_bit
  int forbidden_zero_bit;
  READFLAG(forbidden_zero_bit);
  if (forbidden_zero_bit != 0)
    throw std::logic_error("The nal unit header forbidden zero bit was not zero.");

  // Read nal unit type
  READBITS(nal_ref_idc, 2);

  QStringList nal_unit_type_id_meaning = QStringList()
    << "Unspecified"
    << "Coded slice of a non-IDR picture slice_layer_without_partitioning_rbsp()"
    << "Coded slice data partition A slice_data_partition_a_layer_rbsp( )"
    << "Coded slice data partition B slice_data_partition_b_layer_rbsp( )"
    << "Coded slice data partition C slice_data_partition_c_layer_rbsp( )"
    << "Coded slice of an IDR picture slice_layer_without_partitioning_rbsp( )"
    << "Supplemental enhancement information (SEI) sei_rbsp( )"
    << "Sequence parameter set seq_parameter_set_rbsp( )"
    << "Picture parameter set pic_parameter_set_rbsp( )"
    << "Access unit delimiter access_unit_delimiter_rbsp( )"
    << "End of sequence end_of_seq_rbsp( )"
    << "End of stream end_of_stream_rbsp( )"
    << "Filler data filler_data_rbsp( )"
    << "Sequence parameter set extension seq_parameter_set_extension_rbsp( )"
    << "Prefix NAL unit prefix_nal_unit_rbsp( )"
    << "Subset sequence parameter set subset_seq_parameter_set_rbsp( )"
    << "Depth parameter set depth_parameter_set_rbsp( )"
    << "Reserved"
    << "Reserved"
    << "Coded slice of an auxiliary coded picture without partitioning slice_layer_without_partitioning_rbsp( )"
    << "Coded slice extension slice_layer_extension_rbsp( )"
    << "Coded slice extension for a depth view component or a 3D-AVC texture view component slice_layer_extension_rbsp( )"
    << "Reserved"
    << "Reserved"
    << "Unspecified";
  READBITS_M(nal_unit_type_id, 5, nal_unit_type_id_meaning);

  // Set the nal unit type
  nal_unit_type = (nal_unit_type_id > UNSPCIFIED_31 || nal_unit_type_id < 0) ? UNSPECIFIED : (nal_unit_type_enum)nal_unit_type_id;
}

fileSourceAVCAnnexBFile::sps::sps(const nal_unit_avc &nal) : nal_unit_avc(nal)
{
  chroma_format_idc = 1;
  bit_depth_luma_minus8 = 0;
  bit_depth_chroma_minus8 = 0;
  qpprime_y_zero_transform_bypass_flag = false;
  seq_scaling_matrix_present_flag = false;
  mb_adaptive_frame_field_flag = false;
  frame_crop_left_offset = 0;
  frame_crop_right_offset = 0;
  frame_crop_top_offset = 0;
  frame_crop_bottom_offset = 0;
  separate_colour_plane_flag = false;
  MaxPicOrderCntLsb = 0;
  ExpectedDeltaPerPicOrderCntCycle = 0;
}

void fileSourceAVCAnnexBFile::read_scaling_list(sub_byte_reader &reader, int *scalingList, int sizeOfScalingList, bool *useDefaultScalingMatrixFlag, TreeItem *itemTree)
{
  int lastScale = 8;
  int nextScale = 8;
  for(int j=0; j<sizeOfScalingList; j++)
  {
    if (nextScale != 0)
    {
        int delta_scale;
        READSEV(delta_scale);
        nextScale = (lastScale + delta_scale + 256) % 256;
        *useDefaultScalingMatrixFlag = (j == 0 && nextScale == 0);
    }
    scalingList[j] = (nextScale == 0) ? lastScale : nextScale;
    lastScale = scalingList[j];
  }
}

void fileSourceAVCAnnexBFile::sps::parse_sps(const QByteArray &parameterSetData, TreeItem *root)
{
  nalPayload = parameterSetData;
  sub_byte_reader reader(parameterSetData);

  // Create a new TreeItem root for the item
  // The macros will use this variable to add all the parsed variables
  TreeItem *const itemTree = root ? new TreeItem("seq_parameter_set_rbsp()", root) : nullptr;
    
  READBITS(profile_idc, 8);
  READFLAG(constraint_set0_flag);
  READFLAG(constraint_set1_flag);
  READFLAG(constraint_set2_flag);
  READFLAG(constraint_set3_flag);
  READFLAG(constraint_set4_flag);
  READFLAG(constraint_set5_flag);
  READBITS(reserved_zero_2bits, 2);
  READBITS(level_idc, 8);
  READUEV(seq_parameter_set_id);

  if ( profile_idc == 100 || profile_idc == 110 || profile_idc == 122 || profile_idc == 244 || profile_idc ==  44 || profile_idc ==  83 || 
      profile_idc ==  86 || profile_idc == 118 || profile_idc == 128 || profile_idc == 138 || profile_idc == 139 || profile_idc == 134 )
  {
    QStringList chroma_format_idc_meaning = QStringList() << "moochrome" << "4:2:0" << "4:2:2" << "4:4:4" << "4:4:4";
    READUEV_M(chroma_format_idc, chroma_format_idc_meaning);
    if (chroma_format_idc > 3)
      throw std::logic_error("The value of chroma_format_idc shall be in the range of 0 to 3, inclusive.");
    if (chroma_format_idc == 3)
    {
      READFLAG(separate_colour_plane_flag);
    }
    READUEV(bit_depth_luma_minus8);
    READUEV(bit_depth_chroma_minus8);
    READFLAG(qpprime_y_zero_transform_bypass_flag);
    READFLAG(seq_scaling_matrix_present_flag);
    if (seq_scaling_matrix_present_flag)
    {
      for(int i=0; i<((chroma_format_idc != 3) ? 8 : 12); i++)
      {
        READFLAG(seq_scaling_list_present_flag[i]);
        if (seq_scaling_list_present_flag[i])
        {
          if (i < 6)
            read_scaling_list(reader, ScalingList4x4[i], 16, &UseDefaultScalingMatrix4x4Flag[i], itemTree);
          else
            read_scaling_list(reader, ScalingList8x8[i-6], 64, &UseDefaultScalingMatrix8x8Flag[i-6], itemTree);
        }
      }
    }
  }

  READUEV(log2_max_frame_num_minus4);
  READUEV(pic_order_cnt_type);
  if (pic_order_cnt_type == 0)
  {
    READUEV(log2_max_pic_order_cnt_lsb_minus4)
    if (log2_max_pic_order_cnt_lsb_minus4 > 12)
      throw std::logic_error("The value of log2_max_pic_order_cnt_lsb_minus4 shall be in the range of 0 to 12, inclusive.");
    MaxPicOrderCntLsb = 1 << (log2_max_pic_order_cnt_lsb_minus4 + 4);
  }
  else if (pic_order_cnt_type == 1)
  {
    READFLAG(delta_pic_order_always_zero_flag);
    READSEV(offset_for_non_ref_pic);
    READSEV(offset_for_top_to_bottom_field);
    READUEV(num_ref_frames_in_pic_order_cnt_cycle);
    for(int i=0; i<num_ref_frames_in_pic_order_cnt_cycle; i++)
      READSEV(offset_for_ref_frame[i])
  }

  READUEV(max_num_ref_frames);
  READFLAG(gaps_in_frame_num_value_allowed_flag);
  READUEV(pic_width_in_mbs_minus1);
  READUEV(pic_height_in_map_units_minus1);
  READFLAG(frame_mbs_only_flag);
  if (!frame_mbs_only_flag)
    READFLAG(mb_adaptive_frame_field_flag);

  READFLAG(direct_8x8_inference_flag);
  if (!frame_mbs_only_flag && !direct_8x8_inference_flag)
    throw std::logic_error("When frame_mbs_only_flag is equal to 0, direct_8x8_inference_flag shall be equal to 1.");

  READFLAG(frame_cropping_flag)
  if (frame_cropping_flag)
  {
    READUEV(frame_crop_left_offset);
    READUEV(frame_crop_right_offset);
    READUEV(frame_crop_top_offset);
    READUEV(frame_crop_bottom_offset);
  }
  
  // Calculate some values
  BitDepthY = 8 + bit_depth_luma_minus8;
  QpBdOffsetY = 6 * bit_depth_luma_minus8;
  BitDepthC = 8 + bit_depth_chroma_minus8;
  QpBdOffsetC = 6 * bit_depth_chroma_minus8;
  PicWidthInMbs = pic_width_in_mbs_minus1 + 1;
  PicHeightInMapUnits = pic_height_in_map_units_minus1 + 1;
  FrameHeightInMbs = frame_mbs_only_flag ? PicHeightInMapUnits : PicHeightInMapUnits * 2;
  PicHeightInMbs = FrameHeightInMbs;
  PicSizeInMbs = PicWidthInMbs * PicHeightInMbs;
  
  PicSizeInMapUnits = PicWidthInMbs * PicHeightInMapUnits;
  if (!separate_colour_plane_flag)
    ChromaArrayType = chroma_format_idc;
  else
    ChromaArrayType = 0;

  bool field_pic_flag = false;  // For now, assume field_pic_flag false
  MbaffFrameFlag = (mb_adaptive_frame_field_flag && !field_pic_flag);
  if (pic_order_cnt_type == 1)
  {
    // (7-12)
    for (int i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++ ) 
      ExpectedDeltaPerPicOrderCntCycle += offset_for_ref_frame[i];
  }
  // 7-10
  MaxFrameNum = 1 << (log2_max_frame_num_minus4 + 4);
  
  // Parse the VUI
  READFLAG(vui_parameters_present_flag);
  if (vui_parameters_present_flag)
    vui_parameters.read(reader, itemTree, BitDepthY, BitDepthC, chroma_format_idc);
}

fileSourceAVCAnnexBFile::sps::vui_parameters_struct::vui_parameters_struct()
{
  aspect_ratio_idc = 0;
  video_format = 5;
  video_full_range_flag = false;
  colour_primaries = 2;
  transfer_characteristics = 2;
  matrix_coefficients = 2;
  chroma_sample_loc_type_top_field = 0;
  chroma_sample_loc_type_bottom_field = 0;
  fixed_frame_rate_flag = false;
  nal_hrd_parameters_present_flag = false;
  vcl_hrd_parameters_present_flag = false;
}

void fileSourceAVCAnnexBFile::sps::vui_parameters_struct::read(sub_byte_reader &reader, TreeItem *itemTree, int BitDepthY, int BitDepthC, int chroma_format_idc)
{
  READFLAG(aspect_ratio_info_present_flag);
  if (aspect_ratio_info_present_flag) 
  {
    QStringList aspect_ratio_idc_meaning = QStringList() 
      << "1:1 (square)" << "12:11" << "10:11" << "16:11" << "40:33" << "24:11" << "20:11" << "32:11" 
      << "80:33" << "18:11" << "15:11" << "64:33" << "160:99" << "4:3" << "3:2" << "2:1" << "Reserved";
    READBITS_M(aspect_ratio_idc, 8, aspect_ratio_idc_meaning);
    if (aspect_ratio_idc == 255) // Extended_SAR
    {
      READBITS(sar_width, 16);
      READBITS(sar_height, 16);
    }
  }
  READFLAG(overscan_info_present_flag);
  if (overscan_info_present_flag)
    READFLAG(overscan_appropriate_flag);
  READFLAG(video_signal_type_present_flag);
  if (video_signal_type_present_flag)
  {
    QStringList video_format_meaning = QStringList()
    << "Component" << "PAL" << "NTSC" << "SECAM" << "MAC" << "Unspecified video format" << "Reserved";
    READBITS_M(video_format, 3, video_format_meaning);
    READFLAG(video_full_range_flag);
    READFLAG(colour_description_present_flag);
    if (colour_description_present_flag)
    {
      QStringList colour_primaries_meaning = QStringList() 
        << "Reserved For future use by ITU-T | ISO/IEC"
        << "Rec. ITU-R BT.709-5 / BT.1361 / IEC 61966-2-1 (sRGB or sYCC)"
        << "Unspecified"
        << "Reserved For future use by ITU - T | ISO / IEC"
        << "Rec. ITU-R BT.470-6 System M (historical) (NTSC)"
        << "Rec. ITU-R BT.470-6 System B, G (historical) / BT.601 / BT.1358 / BT.1700 PAL and 625 SECAM"
        << "Rec. ITU-R BT.601-6 525 / BT.1358 525 / BT.1700 NTSC"
        << "Society of Motion Picture and Television Engineers 240M (1999)"
        << "Generic film (colour filters using Illuminant C)"
        << "Rec. ITU-R BT.2020"
        << "Reserved For future use by ITU-T | ISO/IEC";
      READBITS_M(colour_primaries, 8, colour_primaries_meaning);
      
      QStringList transfer_characteristics_meaning = QStringList()
        << "Reserved For future use by ITU-T | ISO/IEC"
        << "Rec. ITU-R BT.709-5 Rec.ITU - R BT.1361 conventional colour gamut system"
        << "Unspecified"
        << "Reserved For future use by ITU - T | ISO / IEC"
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
        << "Rec. ITU-R BT.2020 for 10 bit system"
        << "Rec. ITU-R BT.2020 for 12 bit system"
        << "Reserved For future use by ITU-T | ISO/IEC";
      READBITS_M(transfer_characteristics, 8, transfer_characteristics_meaning);

      QStringList matrix_coefficients_meaning = QStringList()
        << "RGB IEC 61966-2-1 (sRGB)"
        << "Rec. ITU-R BT.709-5, Rec. ITU-R BT.1361"
        << "Image characteristics are unknown or are determined by the application"
        << "For future use by ITU-T | ISO/IEC"
        << "United States Federal Communications Commission Title 47 Code of Federal Regulations (2003) 73.682 (a) (20)"
        << "Rec. ITU-R BT.470-6 System B, G (historical), Rec. ITU-R BT.601-6 625, Rec. ITU-R BT.1358 625, Rec. ITU-R BT.1700 625 PAL and 625 SECAM"
        << "Rec. ITU-R BT.601-6 525, Rec. ITU-R BT.1358 525, Rec. ITU-R BT.1700 NTSC, Society of Motion Picture and Television Engineers 170M (2004)"
        << "Society of Motion Picture and Television Engineers 240M (1999)"
        << "YCgCo"
        << "Rec. ITU-R BT.2020 non-constant luminance system"
        << "Rec. ITU-R BT.2020 constant luminance system"
        << "For future use by ITU-T | ISO/IEC";
      READBITS_M(matrix_coefficients, 8, matrix_coefficients_meaning);
      if ((BitDepthC != BitDepthY || chroma_format_idc != 3) && matrix_coefficients == 0)
        throw std::logic_error("matrix_coefficients shall not be equal to 0 unless both of the following conditions are true: 1 BitDepthC is equal to BitDepthY, 2 chroma_format_idc is equal to 3 (4:4:4).");
    }
  }
  READFLAG(chroma_loc_info_present_flag);
  if (chroma_format_idc != 1 && !chroma_loc_info_present_flag)
    throw std::logic_error("When chroma_format_idc is not equal to 1, chroma_loc_info_present_flag should be equal to 0.");
  if (chroma_loc_info_present_flag)
  {
    READUEV(chroma_sample_loc_type_top_field);
    READUEV(chroma_sample_loc_type_bottom_field);
    if (chroma_sample_loc_type_top_field > 5 || chroma_sample_loc_type_bottom_field > 5)
      throw std::logic_error("The value of chroma_sample_loc_type_top_field and chroma_sample_loc_type_bottom_field shall be in the range of 0 to 5, inclusive.");
  }
  READFLAG(timing_info_present_flag);
  if (timing_info_present_flag)
  {
    READBITS(num_units_in_tick, 32);
    READBITS(time_scale, 32);
    if (time_scale == 0)
      throw std::logic_error("time_scale shall be greater than 0.");
    READFLAG(fixed_frame_rate_flag);
  }
  
  READFLAG(nal_hrd_parameters_present_flag);
  if (nal_hrd_parameters_present_flag)
    nal_hrd.read(reader, itemTree);
  READFLAG(vcl_hrd_parameters_present_flag);
  if (vcl_hrd_parameters_present_flag)
    vcl_hrd.read(reader, itemTree);

  if (nal_hrd_parameters_present_flag && vcl_hrd_parameters_present_flag)
  {
    if (nal_hrd.initial_cpb_removal_delay_length_minus1 != vcl_hrd.initial_cpb_removal_delay_length_minus1)
      throw std::logic_error("Shall be equal.");
    if (nal_hrd.cpb_removal_delay_length_minus1 != vcl_hrd.cpb_removal_delay_length_minus1)
      throw std::logic_error("Shall be equal.");
    if (nal_hrd.dpb_output_delay_length_minus1 != vcl_hrd.dpb_output_delay_length_minus1)
      throw std::logic_error("Shall be equal.");
    if (nal_hrd.time_offset_length != vcl_hrd.time_offset_length)
      throw std::logic_error("Shall be equal.");
  }

  if (nal_hrd_parameters_present_flag || vcl_hrd_parameters_present_flag)
    READFLAG(low_delay_hrd_flag);
  READFLAG(pic_struct_present_flag);
  READFLAG(bitstream_restriction_flag);
  if (bitstream_restriction_flag)
  {
    READFLAG(motion_vectors_over_pic_boundaries_flag);
    READUEV(max_bytes_per_pic_denom);
    READUEV(max_bits_per_mb_denom);
    READUEV(log2_max_mv_length_horizontal);
    READUEV(log2_max_mv_length_vertical);
    READUEV(max_num_reorder_frames);
    READUEV(max_dec_frame_buffering);
  }
}

fileSourceAVCAnnexBFile::sps::vui_parameters_struct::hrd_parameters_struct::hrd_parameters_struct()
{
  cpb_removal_delay_length_minus1 = 23;
  time_offset_length = 24;
}

void fileSourceAVCAnnexBFile::sps::vui_parameters_struct::hrd_parameters_struct::read(sub_byte_reader &reader, TreeItem *itemTree)
{
  READUEV(cpb_cnt_minus1);
  if (cpb_cnt_minus1 > 31)
    throw std::logic_error("The value of cpb_cnt_minus1 shall be in the range of 0 to 31, inclusive.");
  READBITS(bit_rate_scale, 4);
  READBITS(cpb_size_scale, 4);
  for (int SchedSelIdx = 0; SchedSelIdx <= cpb_cnt_minus1; SchedSelIdx++)
  {
    READUEV_A(bit_rate_value_minus1, SchedSelIdx);
    quint32 val_max = 4294967294; // 2^32-2
    if (bit_rate_value_minus1[SchedSelIdx] > val_max)
      throw std::logic_error("bit_rate_value_minus1[ SchedSelIdx ] shall be in the range of 0 to 2^32-2, inclusive.");
    READUEV_A(cpb_size_value_minus1, SchedSelIdx);
    if (cpb_size_value_minus1[SchedSelIdx] > val_max)
      throw std::logic_error("cpb_size_value_minus1[ SchedSelIdx ] shall be in the range of 0 to 2^32-2, inclusive.");
    READFLAG_A(cbr_flag, SchedSelIdx);
  }
  READBITS(initial_cpb_removal_delay_length_minus1, 5);
  READBITS(cpb_removal_delay_length_minus1, 5);
  READBITS(dpb_output_delay_length_minus1, 5);
  READBITS(time_offset_length, 5);
}

fileSourceAVCAnnexBFile::pps::pps(const nal_unit_avc &nal) : nal_unit_avc(nal)
{
  transform_8x8_mode_flag = false;
  pic_scaling_matrix_present_flag = false;
}

void fileSourceAVCAnnexBFile::pps::parse_pps(const QByteArray &parameterSetData, TreeItem *root, const QMap<int, QSharedPointer<sps>> &p_active_SPS_list)
{
  nalPayload = parameterSetData;
  sub_byte_reader reader(parameterSetData);

  // Create a new TreeItem root for the item
  // The macros will use this variable to add all the parsed variables
  TreeItem *const itemTree = root ? new TreeItem("pic_parameter_set_rbsp()", root) : nullptr;
  
  READUEV(pic_parameter_set_id);
  if (pic_parameter_set_id > 255)
    throw std::logic_error("The value of pic_parameter_set_id shall be in the range of 0 to 255, inclusive.");
  READUEV(seq_parameter_set_id);
  if (seq_parameter_set_id > 31)
    throw std::logic_error("The value of seq_parameter_set_id shall be in the range of 0 to 31, inclusive.");

  // Get the referenced sps
  if (!p_active_SPS_list.contains(seq_parameter_set_id))
    throw std::logic_error("The signaled SPS was not found in the bitstream.");
  auto refSPS = p_active_SPS_list.value(seq_parameter_set_id);

  QStringList entropy_coding_mode_flag_meaning = QStringList() << "CAVLC" << "CABAC";
  READFLAG_M(entropy_coding_mode_flag, entropy_coding_mode_flag_meaning);
  READFLAG(bottom_field_pic_order_in_frame_present_flag);
  READUEV(num_slice_groups_minus1);
  if (num_slice_groups_minus1 > 0)
  {
    READUEV(slice_group_map_type);
    if (slice_group_map_type > 6)
      throw std::logic_error("The value of slice_group_map_type shall be in the range of 0 to 6, inclusive.");
    if (slice_group_map_type == 0)
      for(int iGroup = 0; iGroup <= num_slice_groups_minus1; iGroup++ )
      {
        READUEV(run_length_minus1[iGroup]);
      }
    else if (slice_group_map_type == 2)
      for(int iGroup = 0; iGroup < num_slice_groups_minus1; iGroup++) 
      {
        READUEV(top_left[iGroup]);
        READUEV(bottom_right[iGroup]);
      }
    else if (slice_group_map_type == 3 || slice_group_map_type == 4 || slice_group_map_type == 5)
    {
      READFLAG(slice_group_change_direction_flag);
      READUEV(slice_group_change_rate_minus1);
      SliceGroupChangeRate = slice_group_change_rate_minus1 + 1;
    } 
    else if (slice_group_map_type == 6)
    {
      READUEV(pic_size_in_map_units_minus1);
      for(int i = 0; i <= pic_size_in_map_units_minus1; i++)
      {
        int nrBits = ceil(log2(num_slice_groups_minus1 + 1));
        READBITS_A(slice_group_id, nrBits, i);
      }
    }
  }
  READUEV(num_ref_idx_l0_default_active_minus1);
  if (num_ref_idx_l0_default_active_minus1 > 31)
    throw std::logic_error("The value of num_ref_idx_l0_default_active_minus1 shall be in the range of 0 to 31, inclusive.");
  READUEV(num_ref_idx_l1_default_active_minus1);
  if (num_ref_idx_l1_default_active_minus1 > 31)
    throw std::logic_error("The value of num_ref_idx_l1_default_active_minus1 shall be in the range of 0 to 31, inclusive.");
  READFLAG(weighted_pred_flag);
  READBITS(weighted_bipred_idc, 2);
  if (weighted_bipred_idc > 2)
    throw std::logic_error("The value of weighted_bipred_idc shall be in the range of 0 to 2, inclusive.");
  READSEV(pic_init_qp_minus26);
  if (pic_init_qp_minus26 < -(26 + refSPS->QpBdOffsetY) || pic_init_qp_minus26 > 25)
    throw std::logic_error("The value of pic_init_qp_minus26 shall be in the range of -(26 + QpBdOffsetY ) to +25, inclusive.");
  READSEV(pic_init_qs_minus26);
  if (pic_init_qs_minus26 < -26 || pic_init_qs_minus26 > 25)
    throw std::logic_error("The value of pic_init_qs_minus26 shall be in the range of -26 to +25, inclusive.");
  READSEV(chroma_qp_index_offset);
  if (chroma_qp_index_offset < -12 || chroma_qp_index_offset > 12)
    throw std::logic_error("The value of chroma_qp_index_offset shall be in the range of -12 to +12, inclusive.");
  READFLAG(deblocking_filter_control_present_flag);
  READFLAG(constrained_intra_pred_flag);
  READFLAG(redundant_pic_cnt_present_flag);
  if (reader.more_rbsp_data())
  {
    READFLAG(transform_8x8_mode_flag);
    READFLAG(pic_scaling_matrix_present_flag);
    if (pic_scaling_matrix_present_flag)
      for(int i = 0; i < 6 + ((refSPS->chroma_format_idc != 3 ) ? 2 : 6) * transform_8x8_mode_flag; i++) 
      {
        READFLAG(pic_scaling_list_present_flag[i]);
        if (pic_scaling_list_present_flag[i])
        {
          if (i < 6)
            read_scaling_list(reader, ScalingList4x4[i], 16, &UseDefaultScalingMatrix4x4Flag[i], itemTree);
          else
            read_scaling_list(reader, ScalingList8x8[i-6], 64, &UseDefaultScalingMatrix8x8Flag[i-6], itemTree);  
        }
      }
    READSEV(second_chroma_qp_index_offset);
    if (second_chroma_qp_index_offset < -12 || second_chroma_qp_index_offset > 12)
      throw std::logic_error("The value of second_chroma_qp_index_offset shall be in the range of -12 to +12, inclusive.");
  }

  // rbsp_trailing_bits( )
}

fileSourceAVCAnnexBFile::slice_header::slice_header(const nal_unit_avc &nal) : nal_unit_avc(nal)
{
  field_pic_flag = false;
  bottom_field_flag = false;
  delta_pic_order_cnt_bottom = 0;
  redundant_pic_cnt = 0;

  prevPicOrderCntMsb = -1;
  prevPicOrderCntLsb = -1;
  PicOrderCntMsb = -1;
  TopFieldOrderCnt = -1;
  BottomFieldOrderCnt = -1;
  FrameNumOffset = -1;
  globalPOC_highestGlobalPOCLastGOP = -1;
}

void fileSourceAVCAnnexBFile::slice_header::parse_slice_header(const QByteArray &sliceHeaderData, const QMap<int, QSharedPointer<sps>> &p_active_SPS_list, const QMap<int, QSharedPointer<pps>> &p_active_PPS_list, QSharedPointer<slice_header> prev_pic, TreeItem *root)
{
  sub_byte_reader reader(sliceHeaderData);

  // Create a new TreeItem root for the item
  // The macros will use this variable to add all the parsed variables
  TreeItem *const itemTree = root ? new TreeItem("slice_header()", root) : nullptr;
  
  READUEV(first_mb_in_slice);
  QStringList slice_type_id_meaning = QStringList() 
    << "P (P slice)" << "B (B slice)" << "I (I slice)" << "SP (SP slice)" << "SI (SI slice)" << "P (P slice)" << "B (B slice)" << "I (I slice)" << "SP (SP slice)" << "SI (SI slice)";
  READUEV_M(slice_type_id, slice_type_id_meaning);
  slice_type = (slice_type_enum)(slice_type_id % 5);
  slice_type_fixed = slice_type_id > 4;
  IdrPicFlag = (nal_unit_type == CODED_SLICE_IDR);

  READUEV(pic_parameter_set_id);
  // Get the referenced SPS and PPS
  if (!p_active_PPS_list.contains(pic_parameter_set_id))
    throw std::logic_error("The signaled PPS was not found in the bitstream.");
  auto refPPS = p_active_PPS_list.value(pic_parameter_set_id);
  if (!p_active_SPS_list.contains(refPPS->seq_parameter_set_id))
    throw std::logic_error("The signaled SPS was not found in the bitstream.");
  auto refSPS = p_active_SPS_list.value(refPPS->seq_parameter_set_id);

  if (refSPS->separate_colour_plane_flag)
  {
    READBITS(colour_plane_id, 2);
  }
  int nrBits = refSPS->log2_max_frame_num_minus4 + 4;
  READBITS(frame_num, nrBits);
  if (!refSPS->frame_mbs_only_flag)
  {
    READFLAG(field_pic_flag);
    if (field_pic_flag)
    {
      READFLAG(bottom_field_flag)
      // Update 
      refSPS->MbaffFrameFlag = (refSPS->mb_adaptive_frame_field_flag && !field_pic_flag);
      refSPS->PicHeightInMbs = refSPS->FrameHeightInMbs / 2;
      refSPS->PicSizeInMbs = refSPS->PicWidthInMbs * refSPS->PicHeightInMbs;
    }
  }

  // Since the MbaffFrameFlag flag is now finally known, we can check the range of first_mb_in_slice
  if (refSPS->MbaffFrameFlag)
  {
    if (first_mb_in_slice > refSPS->PicSizeInMbs - 1)
      throw std::logic_error("first_mb_in_slice shall be in the range of 0 to PicSizeInMbs - 1, inclusive");
  }
  else
  {
    if (first_mb_in_slice > refSPS->PicSizeInMbs / 2 - 1)
      throw std::logic_error("first_mb_in_slice shall be in the range of 0 to PicSizeInMbs / 2 - 1, inclusive.");
  }

  if (IdrPicFlag)
  {
    READUEV(idr_pic_id);
    if (idr_pic_id > 65535)
      throw std::logic_error("The value of idr_pic_id shall be in the range of 0 to 65535, inclusive.");
  }
  if (refSPS->pic_order_cnt_type == 0)
  {
    int nrBits = refSPS->log2_max_pic_order_cnt_lsb_minus4 + 4;
    READBITS(pic_order_cnt_lsb, nrBits);
    if (pic_order_cnt_lsb > refSPS->MaxPicOrderCntLsb - 1)
      throw std::logic_error("The value of the pic_order_cnt_lsb shall be in the range of 0 to MaxPicOrderCntLsb - 1, inclusive.");
    if (refPPS->bottom_field_pic_order_in_frame_present_flag && !field_pic_flag)
      READSEV(delta_pic_order_cnt_bottom);
  }
  if (refSPS->pic_order_cnt_type == 1 && !refSPS->delta_pic_order_always_zero_flag) 
  {
    READSEV(delta_pic_order_cnt[0]);
    if (refPPS->bottom_field_pic_order_in_frame_present_flag && !field_pic_flag)
      READSEV(delta_pic_order_cnt[1]);
  }
  if (refPPS->redundant_pic_cnt_present_flag)
    READUEV(redundant_pic_cnt);
  if (slice_type == SLICE_B)
    READFLAG(direct_spatial_mv_pred_flag);
  if (slice_type == SLICE_P || slice_type == SLICE_SP || slice_type == SLICE_B)
  {
    READFLAG(num_ref_idx_active_override_flag);
    if (num_ref_idx_active_override_flag)
    {
      READUEV(num_ref_idx_l0_active_minus1);
      if (slice_type == SLICE_B)
        READUEV(num_ref_idx_l1_active_minus1);
    }
    else
    {
      num_ref_idx_l0_active_minus1 = refPPS->num_ref_idx_l0_default_active_minus1;
      if (slice_type == SLICE_B)
        num_ref_idx_l1_active_minus1 = refPPS->num_ref_idx_l1_default_active_minus1;
    }
  }
  if (nal_unit_type == CODED_SLICE_EXTENSION || nal_unit_type == CODED_SLICE_EXTENSION_DEPTH_MAP)
    ref_pic_list_mvc_modification.read(reader, itemTree, slice_type); /* specified in Annex H */
  else
    ref_pic_list_modification.read(reader, itemTree, slice_type);
  if ((refPPS->weighted_pred_flag && (slice_type == SLICE_P || slice_type == SLICE_SP)) || 
     (refPPS->weighted_bipred_idc == 1 && slice_type == SLICE_B))
    pred_weight_table.read(reader, itemTree, slice_type, refSPS->ChromaArrayType, num_ref_idx_l0_active_minus1, num_ref_idx_l1_active_minus1);
  if (nal_ref_idc != 0)
    dec_ref_pic_marking.read(reader, itemTree, IdrPicFlag);
  if (refPPS->entropy_coding_mode_flag && slice_type != SLICE_I && slice_type != SLICE_SI)
  {
    READUEV(cabac_init_idc);
    if (cabac_init_idc > 2)
      throw std::logic_error("The value of cabac_init_idc shall be in the range of 0 to 2, inclusive.");
  }
  READSEV(slice_qp_delta);
  if (slice_type == SLICE_SP || slice_type == SLICE_SI)
  {
    if (slice_type == SLICE_SP)
      READFLAG(sp_for_switch_flag);
    READSEV(slice_qs_delta);
  }
  if (refPPS->deblocking_filter_control_present_flag)
  {
    READUEV(disable_deblocking_filter_idc);
    if (disable_deblocking_filter_idc != 1)
    {
      READSEV(slice_alpha_c0_offset_div2);
      READSEV(slice_beta_offset_div2);
    }
  }
  if (refPPS->num_slice_groups_minus1 > 0 && refPPS->slice_group_map_type >= 3 && refPPS->slice_group_map_type <= 5)
  {
    int nrBits = ceil(log2(refSPS->PicSizeInMapUnits % refPPS->SliceGroupChangeRate + 1));
    READBITS(slice_group_change_cycle, nrBits);
  }

  // Calculate the POC
  if (refSPS->pic_order_cnt_type == 0)
  {
    // 8.2.1.1 Decoding process for picture order count type 0
    // In: PicOrderCntMsb (of previous pic)
    // Out: TopFieldOrderCnt, BottomFieldOrderCnt
    if (IdrPicFlag)
    {
      prevPicOrderCntMsb = 0;
      prevPicOrderCntLsb = 0;
    }
    else
    {
      if (prev_pic.isNull())
        throw std::logic_error("This is not an IDR picture but there is no previous picture available. Can not calculate POC.");

      if (first_mb_in_slice == 0)
      {
        // If the previous reference picture in decoding order included a memory_management_control_operation equal to 5, the following applies:
        if (prev_pic->dec_ref_pic_marking.memory_management_control_operation_list.contains(5))
        {
          if (!prev_pic->bottom_field_flag)
          {
            prevPicOrderCntMsb = 0;
            prevPicOrderCntLsb = prev_pic->TopFieldOrderCnt;
          }
          else
          {
            prevPicOrderCntMsb = 0;
            prevPicOrderCntLsb = 0;
          }
        }
        else
        {
          prevPicOrderCntMsb = prev_pic->PicOrderCntMsb;
          prevPicOrderCntLsb = prev_pic->pic_order_cnt_lsb;
        }
      }
      else
      {
        // Just copy the values from the previous slice
        prevPicOrderCntMsb = prev_pic->prevPicOrderCntMsb;
        prevPicOrderCntLsb = prev_pic->prevPicOrderCntLsb;
      }
    }

    if((pic_order_cnt_lsb < prevPicOrderCntLsb) && ((prevPicOrderCntLsb - pic_order_cnt_lsb) >= (refSPS->MaxPicOrderCntLsb / 2)))
      PicOrderCntMsb = prevPicOrderCntMsb + refSPS->MaxPicOrderCntLsb;
    else if ((pic_order_cnt_lsb > prevPicOrderCntLsb) && ((pic_order_cnt_lsb - prevPicOrderCntLsb) > (refSPS->MaxPicOrderCntLsb / 2)))
      PicOrderCntMsb = prevPicOrderCntMsb - refSPS->MaxPicOrderCntLsb;
    else 
      PicOrderCntMsb = prevPicOrderCntMsb;

    if (!bottom_field_flag)
      TopFieldOrderCnt = PicOrderCntMsb + pic_order_cnt_lsb;
    else
      if(!field_pic_flag) 
        BottomFieldOrderCnt = TopFieldOrderCnt + delta_pic_order_cnt_bottom;
      else 
        BottomFieldOrderCnt = PicOrderCntMsb + pic_order_cnt_lsb;
  }
  else
  {
    if (!IdrPicFlag && prev_pic.isNull())
      throw std::logic_error("This is not an IDR picture but there is no previous picture available. Can not calculate POC.");

    int prevFrameNum = -1;
    if (!prev_pic.isNull())
      prevFrameNum = prev_pic->frame_num;

    int prevFrameNumOffset = 0;
    if (!IdrPicFlag)
    {
      // If the previous reference picture in decoding order included a memory_management_control_operation equal to 5, the following applies:
      if (prev_pic->dec_ref_pic_marking.memory_management_control_operation_list.contains(5))
        prevFrameNumOffset = 0;
      else
        prevFrameNumOffset = prev_pic->FrameNumOffset;
    }

    // (8-6), (8-11)
    if(IdrPicFlag)
      FrameNumOffset = 0;
    else if (prevFrameNum > frame_num)
      FrameNumOffset = prevFrameNumOffset + refSPS->MaxFrameNum;
    else 
      FrameNumOffset = prevFrameNumOffset;

    if (refSPS->pic_order_cnt_type == 1)
    {
      // 8.2.1.2 Decoding process for picture order count type 1
      // in : FrameNumOffset (of previous pic)
      // out: TopFieldOrderCnt, BottomFieldOrderCnt

      // (8-7)
      int absFrameNum;
      if (refSPS->num_ref_frames_in_pic_order_cnt_cycle != 0)
        absFrameNum = FrameNumOffset + frame_num;
      else 
        absFrameNum = 0;
      if (nal_ref_idc == 0 && absFrameNum > 0) 
        absFrameNum = absFrameNum - 1;
      int expectedPicOrderCnt = 0;
      if (absFrameNum > 0)
      {
        // (8-8)
        int picOrderCntCycleCnt = (absFrameNum - 1) / refSPS->num_ref_frames_in_pic_order_cnt_cycle;
        int frameNumInPicOrderCntCycle = (absFrameNum - 1) % refSPS->num_ref_frames_in_pic_order_cnt_cycle;
        expectedPicOrderCnt = picOrderCntCycleCnt * refSPS->ExpectedDeltaPerPicOrderCntCycle;
        for (int i = 0; i <= frameNumInPicOrderCntCycle; i++)
          expectedPicOrderCnt = expectedPicOrderCnt + refSPS->offset_for_ref_frame[i];
      }
      if (nal_ref_idc == 0)
        expectedPicOrderCnt = expectedPicOrderCnt + refSPS->offset_for_non_ref_pic;
      // (8-19)
      if (!field_pic_flag)
      { 
        TopFieldOrderCnt = expectedPicOrderCnt + delta_pic_order_cnt[0];
        BottomFieldOrderCnt = TopFieldOrderCnt + refSPS->offset_for_top_to_bottom_field + delta_pic_order_cnt[1];
      }
      else if (!bottom_field_flag)
        TopFieldOrderCnt = expectedPicOrderCnt + delta_pic_order_cnt[0];
      else 
        BottomFieldOrderCnt = expectedPicOrderCnt + refSPS->offset_for_top_to_bottom_field + delta_pic_order_cnt[0];
    }
    else if (refSPS->pic_order_cnt_type == 2)
    {
      // 8.2.1.3 Decoding process for picture order count type 2
      // out: TopFieldOrderCnt, BottomFieldOrderCnt

      // (8-12)
      int tempPicOrderCnt = 0;
      if (!IdrPicFlag)
      {
        if (nal_ref_idc == 0)
          tempPicOrderCnt = 2 * (FrameNumOffset + frame_num) - 1;
        else 
          tempPicOrderCnt = 2 * (FrameNumOffset + frame_num);
      }
      // (8-13)
      if (!field_pic_flag) 
      { 
        TopFieldOrderCnt = tempPicOrderCnt;
        BottomFieldOrderCnt = tempPicOrderCnt;
      } 
      else if (bottom_field_flag)
        BottomFieldOrderCnt = tempPicOrderCnt;
      else 
        TopFieldOrderCnt = tempPicOrderCnt;
    }
  }
  
  if (prev_pic.isNull())
  {
    globalPOC = 0;
    globalPOC_lastIDR = 0;
    globalPOC_highestGlobalPOCLastGOP = 0;
  }
  else
  {
    if (first_mb_in_slice != 0)
    {
      globalPOC = prev_pic->globalPOC;
      globalPOC_lastIDR = prev_pic->globalPOC_lastIDR;
      globalPOC_highestGlobalPOCLastGOP = prev_pic->globalPOC_highestGlobalPOCLastGOP;
    }
    else if (IdrPicFlag)
    {
      globalPOC = prev_pic->globalPOC_highestGlobalPOCLastGOP + 2;
      globalPOC_lastIDR = globalPOC;
      globalPOC_highestGlobalPOCLastGOP = globalPOC;
    }
    else
    {
      globalPOC = prev_pic->globalPOC_lastIDR + TopFieldOrderCnt;
      if (globalPOC > globalPOC_highestGlobalPOCLastGOP)
        globalPOC_highestGlobalPOCLastGOP = globalPOC;
      globalPOC_lastIDR = prev_pic->globalPOC_lastIDR;
    }
  }
}

void fileSourceAVCAnnexBFile::slice_header::ref_pic_list_mvc_modification_struct::read(sub_byte_reader & reader, TreeItem * itemTree, slice_type_enum slice_type)
{
  if (slice_type != SLICE_I && slice_type != SLICE_SI)
  {
    READFLAG(ref_pic_list_modification_flag_l0);
    int modification_of_pic_nums_idc;
    if (ref_pic_list_modification_flag_l0)
      do 
      {
        READUEV(modification_of_pic_nums_idc);
        modification_of_pic_nums_idc_l0.append(modification_of_pic_nums_idc);
        if (modification_of_pic_nums_idc == 0 || modification_of_pic_nums_idc == 1)
        {
          int abs_diff_pic_num_minus1;
          READUEV(abs_diff_pic_num_minus1);
          abs_diff_pic_num_minus1_l0.append(abs_diff_pic_num_minus1);
        }
        else if (modification_of_pic_nums_idc == 2)
        {
          int long_term_pic_num;
          READUEV(long_term_pic_num);
          long_term_pic_num_l0.append(long_term_pic_num);
        }
        else if (modification_of_pic_nums_idc == 4 || modification_of_pic_nums_idc == 5)
        {
          int abs_diff_view_idx_minus1;
          READUEV(abs_diff_view_idx_minus1);
          abs_diff_view_idx_minus1_l0.append(abs_diff_view_idx_minus1);
        }
       } while (modification_of_pic_nums_idc != 3);
  }
  if (slice_type == SLICE_B) 
  {
    READFLAG(ref_pic_list_modification_flag_l1);
    int modification_of_pic_nums_idc;
    if (ref_pic_list_modification_flag_l1)
    do 
    {
      READUEV(modification_of_pic_nums_idc);
      modification_of_pic_nums_idc_l1.append(modification_of_pic_nums_idc);
      if (modification_of_pic_nums_idc == 0 || modification_of_pic_nums_idc == 1)
      {
        int abs_diff_pic_num_minus1;
        READUEV(abs_diff_pic_num_minus1);
        abs_diff_pic_num_minus1_l1.append(abs_diff_pic_num_minus1);
      }
      else if (modification_of_pic_nums_idc == 2)
      {
        int long_term_pic_num;
        READUEV(long_term_pic_num);
        long_term_pic_num_l1.append(long_term_pic_num);
      }
      else if (modification_of_pic_nums_idc == 4 || modification_of_pic_nums_idc == 5)
      {
        int abs_diff_view_idx_minus1;
        READUEV(abs_diff_view_idx_minus1);
        abs_diff_view_idx_minus1_l1.append(abs_diff_view_idx_minus1);
      }
      } while(modification_of_pic_nums_idc != 3);
  }
}

void fileSourceAVCAnnexBFile::slice_header::ref_pic_list_modification_struct::read(sub_byte_reader & reader, TreeItem * itemTree, slice_type_enum slice_type)
{
  if (slice_type != SLICE_I && slice_type != SLICE_SI)
  {
    READFLAG(ref_pic_list_modification_flag_l0);
    int modification_of_pic_nums_idc;
    if (ref_pic_list_modification_flag_l0)
    do 
    { 
      READUEV(modification_of_pic_nums_idc);
      modification_of_pic_nums_idc_l0.append(modification_of_pic_nums_idc);
      if (modification_of_pic_nums_idc == 0 || modification_of_pic_nums_idc == 1)
      {
        int abs_diff_pic_num_minus1;
        READUEV(abs_diff_pic_num_minus1);
        abs_diff_pic_num_minus1_l0.append(abs_diff_pic_num_minus1);
      }
      else if (modification_of_pic_nums_idc == 2)
      {
        int long_term_pic_num;
        READUEV(long_term_pic_num);
        long_term_pic_num_l0.append(long_term_pic_num);
      }
    } while (modification_of_pic_nums_idc != 3);
  }
  if (slice_type == 1) 
  {
    READFLAG(ref_pic_list_modification_flag_l1);
    int modification_of_pic_nums_idc;
    if (ref_pic_list_modification_flag_l1)
    do 
    {
      READUEV(modification_of_pic_nums_idc);
      modification_of_pic_nums_idc_l1.append(modification_of_pic_nums_idc);
      if (modification_of_pic_nums_idc == 0 || modification_of_pic_nums_idc == 1)
      {
        int abs_diff_pic_num_minus1;
        READUEV(abs_diff_pic_num_minus1);
        abs_diff_pic_num_minus1_l1.append(abs_diff_pic_num_minus1);
      }
      else if (modification_of_pic_nums_idc == 2)
      {
        int long_term_pic_num;
        READUEV(long_term_pic_num);
        long_term_pic_num_l1.append(long_term_pic_num);
      }
    } while (modification_of_pic_nums_idc != 3);
  }
}

void fileSourceAVCAnnexBFile::slice_header::pred_weight_table_struct::read(sub_byte_reader & reader, TreeItem * itemTree, slice_type_enum slice_type, int ChromaArrayType, int num_ref_idx_l0_active_minus1, int num_ref_idx_l1_active_minus1)
{
  READUEV(luma_log2_weight_denom);
  if (ChromaArrayType != 0)
    READUEV(chroma_log2_weight_denom);
  for (int i=0; i<=num_ref_idx_l0_active_minus1; i++)
  {
    bool luma_weight_l0_flag;
    READFLAG(luma_weight_l0_flag);
    luma_weight_l0_flag_list.append(luma_weight_l0_flag);
    if(luma_weight_l0_flag) 
    {
      READSEV_A(luma_weight_l0, i);
      READSEV_A(luma_offset_l0, i);
    }
    if (ChromaArrayType != 0) 
    {
      bool chroma_weight_l0_flag;
      READFLAG(chroma_weight_l0_flag);
      chroma_weight_l0_flag_list.append(chroma_weight_l0_flag);
      if (chroma_weight_l0_flag)
        for (int j=0; j<2; j++) 
        {
          READSEV_A(chroma_weight_l0[j], i);
          READSEV_A(chroma_offset_l0[j], i);
        }
    }
  }
  if (slice_type == SLICE_B)
    for (int i=0; i<=num_ref_idx_l1_active_minus1; i++)
    {
      bool luma_weight_l1_flag;
      READFLAG(luma_weight_l1_flag);
      luma_weight_l1_flag_list.append(luma_weight_l1_flag);
      if (luma_weight_l1_flag)
      {
        READSEV_A(luma_weight_l1, i);
        READSEV_A(luma_offset_l1, i);
      }
      if (ChromaArrayType != 0) 
      {
        bool chroma_weight_l1_flag;
        READFLAG(chroma_weight_l1_flag);
        chroma_weight_l1_flag_list.append(chroma_weight_l1_flag);
        if (chroma_weight_l1_flag)
          for (int j=0; j<2; j++)
          {
            READSEV_A(chroma_weight_l1[j], i);
            READSEV_A(chroma_offset_l1[j], i);
          }
      }
    }
}


void fileSourceAVCAnnexBFile::slice_header::dec_ref_pic_marking_struct::read(sub_byte_reader & reader, TreeItem * itemTree, bool IdrPicFlag)
{
  if (IdrPicFlag)
  {
    READFLAG(no_output_of_prior_pics_flag);
    READFLAG(long_term_reference_flag);
  } 
  else
  {
    READFLAG(adaptive_ref_pic_marking_mode_flag);
    int memory_management_control_operation;
    if (adaptive_ref_pic_marking_mode_flag)
    do 
    {
      READUEV(memory_management_control_operation)
      memory_management_control_operation_list.append(memory_management_control_operation);
      if (memory_management_control_operation == 1 || memory_management_control_operation == 3)
        READUEV_APP(difference_of_pic_nums_minus1)
      if (memory_management_control_operation == 2)
        READUEV_APP(long_term_pic_num);
      if (memory_management_control_operation == 3 || memory_management_control_operation == 6)
        READUEV_APP(long_term_frame_idx);
      if (memory_management_control_operation == 4)
        READUEV_APP(max_long_term_frame_idx_plus1);
    } while (memory_management_control_operation != 0);
  }
}

QByteArray fileSourceAVCAnnexBFile::nal_unit_avc::getNALHeader() const
{
  // TODO: 
  // if ( nal_unit_type = = 14 | | nal_unit_type = = 20 | | nal_unit_type = = 21 ) ...
  char out = ((int)nal_ref_idc << 5) + nal_unit_type;
  char c[5] = { 0, 0, 0, 1, out };
  return QByteArray(c, 5);
}

int fileSourceAVCAnnexBFile::sei::parse_sei_message(QByteArray &sliceHeaderData, TreeItem *root)
{
  sub_byte_reader reader(sliceHeaderData);

  // Create a new TreeItem root for the item
  // The macros will use this variable to add all the parsed variables
  TreeItem *const itemTree = root ? new TreeItem("sei_message()", root) : nullptr;

  payloadType = 0;

  // Read byte by byte
  int byte;
  QString code;
  byte = reader.readBits(8, &code);
  while (byte == 255) // 0xFF
  {
    payloadType += 255;

    if (itemTree) 
      new TreeItem("ff_byte", byte, QString("f(8)"), code, itemTree);

    // Read the next byte
    code.clear();
    byte = reader.readBits(8, &code);
  }

  // The next byte is not 255 (0xFF)
  last_payload_type_byte = byte;
  if (itemTree) 
    new TreeItem("last_payload_type_byte", byte, QString("u(8)"), code, itemTree);

  payloadType += last_payload_type_byte;

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
  else if (payloadType == 7)
    payloadTypeName = "dec_ref_pic_marking_repetition";
  else if (payloadType == 8)
    payloadTypeName = "spare_pic";
  else if (payloadType == 9)
    payloadTypeName = "scene_info";
  else if (payloadType == 10)
    payloadTypeName = "sub_seq_info";
  else if (payloadType == 11)
    payloadTypeName = "sub_seq_layer_characteristics";
  else if (payloadType == 12)
    payloadTypeName = "sub_seq_characteristics";
  else if (payloadType == 13)
    payloadTypeName = "full_frame_freeze";
  else if (payloadType == 14)
    payloadTypeName = "full_frame_freeze_release";
  else if (payloadType == 15)
    payloadTypeName = "full_frame_snapshot";
  else if (payloadType == 16)
    payloadTypeName = "progressive_refinement_segment_start";
  else if (payloadType == 17)
    payloadTypeName = "progressive_refinement_segment_end";
  else if (payloadType == 18)
    payloadTypeName = "motion_constrained_slice_group_set";
  else if (payloadType == 19)
    payloadTypeName = "film_grain_characteristics";
  else if (payloadType == 20)
    payloadTypeName = "deblocking_filter_display_preference";
  else if (payloadType == 21)
    payloadTypeName = "stereo_video_info";
  else if (payloadType == 22)
    payloadTypeName = "post_filter_hint";
  else if (payloadType == 23)
    payloadTypeName = "tone_mapping_info";
  else if (payloadType == 24)
    payloadTypeName = "scalability_info"; /* specified in Annex G */
  else if (payloadType == 25)
    payloadTypeName = "sub_pic_scalable_layer"; /* specified in Annex G */
  else if (payloadType == 26)
    payloadTypeName = "non_required_layer_rep"; /* specified in Annex G */
  else if (payloadType == 27)
    payloadTypeName = "priority_layer_info"; /* specified in Annex G */
  else if (payloadType == 28)
    payloadTypeName = "layers_not_present"; /* specified in Annex G */
  else if (payloadType == 29)
    payloadTypeName = "layer_dependency_change"; /* specified in Annex G */
  else if (payloadType == 30)
    payloadTypeName = "scalable_nesting"; /* specified in Annex G */
  else if (payloadType == 31)
    payloadTypeName = "base_layer_temporal_hrd"; /* specified in Annex G */
  else if (payloadType == 32)
    payloadTypeName = "quality_layer_integrity_check"; /* specified in Annex G */
  else if (payloadType == 33)
    payloadTypeName = "redundant_pic_property"; /* specified in Annex G */
  else if (payloadType == 34)
    payloadTypeName = "tl0_dep_rep_index"; /* specified in Annex G */
  else if (payloadType == 35)
    payloadTypeName = "tl_switching_point"; /* specified in Annex G */
  else if (payloadType == 36)
    payloadTypeName = "parallel_decoding_info"; /* specified in Annex H */
  else if (payloadType == 37)
    payloadTypeName = "mvc_scalable_nesting"; /* specified in Annex H */
  else if (payloadType == 38)
    payloadTypeName = "view_scalability_info"; /* specified in Annex H */
  else if (payloadType == 39)
    payloadTypeName = "multiview_scene_info"; /* specified in Annex H */
  else if (payloadType == 40)
    payloadTypeName = "multiview_acquisition_info"; /* specified in Annex H */
  else if (payloadType == 41)
    payloadTypeName = "non_required_view_component"; /* specified in Annex H */
  else if (payloadType == 42)
    payloadTypeName = "view_dependency_change"; /* specified in Annex H */
  else if (payloadType == 43)
    payloadTypeName = "operation_points_not_present"; /* specified in Annex H */
  else if (payloadType == 44)
    payloadTypeName = "base_view_temporal_hrd"; /* specified in Annex H */
  else if (payloadType == 45)
    payloadTypeName = "frame_packing_arrangement";
  else if (payloadType == 46)
    payloadTypeName = "multiview_view_position"; /* specified in Annex H */
  else if (payloadType == 47)
    payloadTypeName = "display_orientation";
  else if (payloadType == 48)
    payloadTypeName = "mvcd_scalable_nesting"; /* specified in Annex I */
  else if (payloadType == 49)
    payloadTypeName = "mvcd_view_scalability_info"; /* specified in Annex I */
  else if (payloadType == 50)
    payloadTypeName = "depth_representation_info"; /* specified in Annex I */
  else if (payloadType == 51)
    payloadTypeName = "three_dimensional_reference_displays_info"; /* specified in Annex I */
  else if (payloadType == 52)
    payloadTypeName = "depth_timing"; /* specified in Annex I */
  else if (payloadType == 53)
    payloadTypeName = "depth_sampling_info"; /* specified in Annex I */
  else if (payloadType == 54)
    payloadTypeName = "constrained_depth_parameter_set_identifier"; /* specified in Annex J */
  else
    payloadTypeName = "reserved_sei_message";

  LOGVAL_M(payloadType,payloadTypeName);
  
  payloadSize = 0;

  // Read the next byte
  code.clear();
  byte = reader.readBits(8, &code);
  while (byte == 255) // 0xFF
  {
    payloadSize += 255;

    if (itemTree) 
      new TreeItem("ff_byte", byte, QString("f(8)"), code, itemTree);

    // Read the next byte
    code.clear();
    byte = reader.readBits(8, &code);
  }

  // The next byte is not 255
  last_payload_size_byte = byte;
  if (itemTree) 
    new TreeItem("last_payload_size_byte", byte, QString("u(8)"), code, itemTree);

  payloadSize += last_payload_size_byte;
  LOGVAL(payloadSize);

  return reader.nrBytesRead();
}

void fileSourceAVCAnnexBFile::buffering_period_sei::parse_buffering_period_sei(QByteArray &sliceHeaderData, const QMap<int, QSharedPointer<sps>> &p_active_SPS_list, TreeItem *root)
{
  sub_byte_reader reader(sliceHeaderData);

  // Create a new TreeItem root for the item
  // The macros will use this variable to add all the parsed variables
  TreeItem *const itemTree = root ? new TreeItem("buffering_period()", root) : nullptr;

  READUEV(seq_parameter_set_id);
  if (!p_active_SPS_list.contains(seq_parameter_set_id))
    throw std::logic_error("The signaled SPS was not found in the bitstream.");
  auto refSPS = p_active_SPS_list.value(seq_parameter_set_id);

  bool NalHrdBpPresentFlag = refSPS->vui_parameters.nal_hrd_parameters_present_flag;
  if(NalHrdBpPresentFlag)
  {
    int cpb_cnt_minus1 = refSPS->vui_parameters.nal_hrd.cpb_cnt_minus1;
    for(int SchedSelIdx = 0; SchedSelIdx <= cpb_cnt_minus1; SchedSelIdx++)
    {
      int nrBits = refSPS->vui_parameters.nal_hrd.initial_cpb_removal_delay_length_minus1 + 1;
      READBITS_A(initial_cpb_removal_delay, nrBits, SchedSelIdx);
      READBITS_A(initial_cpb_removal_delay_offset, nrBits, SchedSelIdx);
    }
  }
  bool VclHrdBpPresentFlag = refSPS->vui_parameters.vcl_hrd_parameters_present_flag;
  if (VclHrdBpPresentFlag)
  {
    int cpb_cnt_minus1 = refSPS->vui_parameters.vcl_hrd.cpb_cnt_minus1;
    for(int SchedSelIdx = 0; SchedSelIdx <= cpb_cnt_minus1; SchedSelIdx++)
    {
      READBITS_A(initial_cpb_removal_delay, 5, SchedSelIdx);
      READBITS_A(initial_cpb_removal_delay_offset, 5, SchedSelIdx);
    }
  }
}

void fileSourceAVCAnnexBFile::pic_timing_sei::parse_pic_timing_sei(QByteArray &sliceHeaderData, const QMap<int, QSharedPointer<sps>> &p_active_SPS_list, bool CpbDpbDelaysPresentFlag, TreeItem *root)
{
  sub_byte_reader reader(sliceHeaderData);

  // Create a new TreeItem root for the item
  // The macros will use this variable to add all the parsed variables
  TreeItem *const itemTree = root ? new TreeItem("pic_timing()", root) : nullptr;

  // TODO: Is this really the correct sps? I did not really understand everything.
  auto refSPS = p_active_SPS_list[0];

  if (CpbDpbDelaysPresentFlag)
  {
    int nrBits_removal_delay, nrBits_output_delay;
    bool NalHrdBpPresentFlag = refSPS->vui_parameters.nal_hrd_parameters_present_flag;
    if (NalHrdBpPresentFlag)
    {
      nrBits_removal_delay = refSPS->vui_parameters.nal_hrd.cpb_removal_delay_length_minus1 + 1;
      nrBits_output_delay = refSPS->vui_parameters.nal_hrd.dpb_output_delay_length_minus1 + 1;
    }
    bool VclHrdBpPresentFlag = refSPS->vui_parameters.vcl_hrd_parameters_present_flag;
    if (VclHrdBpPresentFlag)
    {
      nrBits_removal_delay = refSPS->vui_parameters.vcl_hrd.cpb_removal_delay_length_minus1 + 1;
      nrBits_output_delay = refSPS->vui_parameters.vcl_hrd.dpb_output_delay_length_minus1 + 1;
    }

    if (NalHrdBpPresentFlag || VclHrdBpPresentFlag)
    {
      READBITS(cpb_removal_delay, nrBits_removal_delay);
      READBITS(dpb_output_delay, nrBits_output_delay);
    }
  }

  if (refSPS->vui_parameters.pic_struct_present_flag)
  {
    QStringList pic_struct_meaings = QStringList() 
      << "(progressive) frame"
     << "top field"
     << "bottom field"
     << "top field, bottom field, in that order"
     << "bottom field, top field, in that order"
     << "top field, bottom field, top field repeated, in that order"
     << "bottom field, top field, bottom field repeated, in that order"
     << "frame doubling"
     << "frame tripling"
     << "reserved";
    READBITS_M(pic_struct, 4, pic_struct_meaings);
    int NumClockTS = 0;
    if (pic_struct < 3)
      NumClockTS = 1;
    else if (pic_struct < 5 || pic_struct == 7)
      NumClockTS = 2;
    else if (pic_struct < 9)
      NumClockTS = 3;
    for (int i=0; i<NumClockTS ; i++)
    {
      READFLAG_A(clock_timestamp_flag, i);
      if (clock_timestamp_flag[i])
      {
        QStringList ct_type_meanings = QStringList() << "progressive" << "interlaced" << "unknown" << "reserved";
        READBITS_M(ct_type[i], 2, ct_type_meanings);
        READFLAG(nuit_field_based_flag[i]);
        QStringList counting_type_meaning = QStringList()
          << "no dropping of n_frames count values and no use of time_offset"
          << "no dropping of n_frames count values"
          << "dropping of individual zero values of n_frames count"
          << "dropping of individual MaxFPS - 1 values of n_frames count"
          << "dropping of the two lowest (value 0 and 1) n_frames counts when seconds_value is equal to 0 and minutes_value is not an integer multiple of 10"
          << "dropping of unspecified individual n_frames count values"
          << "dropping of unspecified numbers of unspecified n_frames count values"
          << "reserved";
        READBITS_M(counting_type[i], 5, counting_type_meaning);
        READFLAG(full_timestamp_flag[i]);
        READFLAG(discontinuity_flag[i]);
        READFLAG(cnt_dropped_flag[i]);
        READBITS(n_frames[i], 8);
        if (full_timestamp_flag[i])
        {
          READBITS(seconds_value[i], 6); /* 0..59 */
          READBITS(minutes_value[i], 6); /* 0..59 */
          READBITS(hours_value[i], 5);   /* 0..23 */
        } 
        else 
        {
          READFLAG(seconds_flag[i]);
          if (seconds_flag[i])
          {
            READBITS(seconds_value[i], 6); /* 0..59 */
            READFLAG(minutes_flag[i]);
            if (minutes_flag[i])
            {
              READBITS(minutes_value[i], 6); /* 0..59 */
              READFLAG(hours_flag[i]);
              if (hours_flag[i])
                READBITS(hours_value[i], 5);   /* 0..23 */
            }
          }
        }
        if (refSPS->vui_parameters.nal_hrd.time_offset_length > 0)
        {
          int nrBits = refSPS->vui_parameters.nal_hrd.time_offset_length;
          READBITS(time_offset[i], nrBits);
        }
      }
    }
  }
}

void fileSourceAVCAnnexBFile::user_data_sei::parse_user_data_sei(QByteArray &sliceHeaderData, TreeItem *root)
{
  if (sliceHeaderData.mid(16, 4) == "x264")
  {
    // This seems to be x264 user data. These contain the encoder settings which might be useful
    user_data_UUID = sliceHeaderData.mid(0, 16).toHex();
    user_data_message = sliceHeaderData.mid(16);

    // Create a new TreeItem root for the item
    // The macros will use this variable to add all the parsed variables
    TreeItem *const itemTree = root ? new TreeItem("x264 user data", root) : nullptr;
    LOGPARAM("UUID", user_data_UUID, "u(128)", "", "random ID number generated according to ISO-11578");

    QStringList list = user_data_message.split(QRegExp("[\r\n\t ]+"), QString::SkipEmptyParts);
    bool options = false;
    QString aggregate_string;
    for (QString val : list)
    {
      if (options)
      {
        QStringList option = val.split("=");
        if (option.length() == 2)
        {
          LOGPARAM(option[0], option[1], "", "", "");
        }
      }
      else
      {
        if (val == "-")
        {
          if (aggregate_string != " -" && aggregate_string != "-" && !aggregate_string.isEmpty())
            LOGPARAM("Info", aggregate_string, "", "", "")
          aggregate_string = "";
        }
        else if (val == "options:")
        {
          options = true;
          if (aggregate_string != " -" && aggregate_string != "-" && !aggregate_string.isEmpty())
            LOGPARAM("Info", aggregate_string, "", "", "")
        }
        else
          aggregate_string += " " + val;
      }
    }
  }
}

QList<QByteArray> fileSourceAVCAnnexBFile::seekToFrameNumber(int iFrameNr)
{
  return QList<QByteArray>();
}