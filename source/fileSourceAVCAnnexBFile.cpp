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

// Read "numBits" bits into the variable "into". 
#define READBITS(into,numBits) {QString code; into=reader.readBits(numBits, &code); if (itemTree) new TreeItem(#into,into,QString("u(v) -> u(%1)").arg(numBits),code, itemTree);}
#define READBITS_A(into,numBits,i) {QString code; int v=reader.readBits(numBits,&code); into.append(v); if (itemTree) new TreeItem(QString(#into)+QString("[%1]").arg(i),v,QString("u(v) -> u(%1)").arg(numBits),code, itemTree);}
// Read a flag (1 bit) into the variable "into".
#define READFLAG(into) {into=(reader.readBits(1)!=0); if (itemTree) new TreeItem(#into,into,QString("u(1)"),(into!=0)?"1":"0",itemTree);}
#define READFLAG_A(into,i) {bool b=(reader.readBits(1)!=0); into.append(b); if (itemTree) new TreeItem(QString(#into)+QString("[%1]").arg(i),b,QString("u(1)"),b?"1":"0",itemTree);}
// Read a unsigned ue(v) code from the bitstream into the variable "into"
#define READUEV(into) {QString code; into=reader.readUE_V(&code); if (itemTree) new TreeItem(#into,into,QString("ue(v)"),code,itemTree);}
#define READUEV_A(arr,i) {QString code; int v=reader.readUE_V(&code); arr.append(v); if (itemTree) new TreeItem(QString(#arr)+QString("[%1]").arg(i),v,QString("ue(v)"),code,itemTree);}
#define READUEV_APP(arr) {QString code; int v=reader.readUE_V(&code); arr.append(v); if (itemTree) new TreeItem(QString(#arr),v,QString("ue(v)"),code,itemTree);}
// Read a signed se(v) code from the bitstream into the variable "into"
#define READSEV(into) {QString code; into=reader.readSE_V(&code); if (itemTree) new TreeItem(#into,into,QString("se(v)"),code,itemTree);}
#define READSEV_A(into,i) {QString code; int v=reader.readSE_V(&code); into.append(v); if (itemTree) new TreeItem(QString(#into)+QString("[%1]").arg(i),v,QString("se(v)"),code,itemTree);}

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
    sps *new_sps = new sps(nal_avc);
    new_sps->parse_sps(getRemainingNALBytes(), nalRoot);
      
    // Add sps (replace old one if existed)
    active_SPS_list.insert(new_sps->seq_parameter_set_id, new_sps);

    // Also add sps to list of all nals
    nalUnitList.append(new_sps);

    // Add the SPS ID
    specificDescription = QString(" SPS_NUT ID %1").arg(new_sps->seq_parameter_set_id);
  }
  else if (nal_avc.nal_unit_type == PPS) 
  {
    // A picture parameter set
    pps *new_pps = new pps(nal_avc);
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
    //// Create a new slice unit
    //slice *newSlice = new slice(nal_avc);
    //newSlice->parse_slice(getRemainingNALBytes(), active_SPS_list, active_PPS_list, lastFirstSliceSegmentInPic, nalRoot);

    //// Add the POC of the slice
    //specificDescription = QString(" POC %1").arg(newSlice->PicOrderCntVal);

    //if (newSlice->first_slice_segment_in_pic_flag)
    //  lastFirstSliceSegmentInPic = newSlice;

    //// Get the poc and add it to the POC list
    //if (newSlice->PicOrderCntVal >= 0 && newSlice->pic_output_flag && !isRandomAccessSkip)
    //  addPOCToList(newSlice->PicOrderCntVal);

    //if (nal_avc.isIRAP())
    //{
    //  newSlice->poc = newSlice->PicOrderCntVal;
    //  if (newSlice->first_slice_segment_in_pic_flag)
    //    // This is the first slice of a random access pont. Add it to the list.
    //    nalUnitList.append(newSlice);
    //  else
    //    delete newSlice;
    //}
  }
  //else if (nal_avc.nal_unit_type == PREFIX_SEI_NUT || nal_avc.nal_unit_type == SUFFIX_SEI_NUT)
  //{
  //  // An SEI message
  //  sei *new_sei = new sei(nal_avc);
  //  new_sei->parse_sei_message(getRemainingNALBytes(), nalRoot);

  //  specificDescription = QString(" payloadType %1").arg(new_sei->payloadType);

  //  // We don't use the SEI message
  //  delete new_sei;
  //}

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
  READBITS(nal_unit_type_id, 5);

  // Set the nal unit type
  nal_unit_type = (nal_unit_type_id > UNSPECIFIED || nal_unit_type_id < 0) ? UNSPECIFIED : (nal_unit_type_enum)nal_unit_type_id;
}

fileSourceAVCAnnexBFile::sps::sps(const nal_unit_avc &nal) : nal_unit_avc(nal)
{
  mb_adaptive_frame_field_flag = false;
  frame_crop_left_offset = 0;
  frame_crop_right_offset = 0;
  frame_crop_top_offset = 0;
  frame_crop_bottom_offset = 0;
  separate_colour_plane_flag = false;
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
    READUEV(chroma_format_idc);
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
    READUEV(log2_max_pic_order_cnt_lsb_minus4)
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
  READFLAG(frame_cropping_flag)
  if (frame_cropping_flag)
  {
    READUEV(frame_crop_left_offset);
    READUEV(frame_crop_right_offset);
    READUEV(frame_crop_top_offset);
    READUEV(frame_crop_bottom_offset);
  }
  READFLAG(vui_parameters_present_flag);
  if (vui_parameters_present_flag)
    vui_parameters.read(reader, itemTree);

  // Calculate some values
  PicWidthInMbs = pic_width_in_mbs_minus1 + 1;
  PicHeightInMapUnits = pic_height_in_map_units_minus1 + 1;
  PicSizeInMapUnits = PicWidthInMbs * PicHeightInMapUnits;
  if (separate_colour_plane_flag)
    ChromaArrayType = chroma_format_idc;
  else
    ChromaArrayType = 0;
}

void fileSourceAVCAnnexBFile::sps::vui_parameters_struct::read(sub_byte_reader &reader, TreeItem *itemTree)
{
  READFLAG(aspect_ratio_info_present_flag);
  if (aspect_ratio_info_present_flag) 
  {
    READBITS(aspect_ratio_idc, 8);
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
    READBITS(video_format, 3);
    READFLAG(video_full_range_flag);
    READFLAG(colour_description_present_flag);
    if (colour_description_present_flag)
    {
      READBITS(colour_primaries, 8);
      READBITS(transfer_characteristics, 8);
      READBITS(matrix_coefficients, 8);
    }
  }
  READFLAG(chroma_loc_info_present_flag);
  if (chroma_loc_info_present_flag)
  {
    READUEV(chroma_sample_loc_type_top_field);
    READUEV(chroma_sample_loc_type_bottom_field);
  }
  READFLAG(timing_info_present_flag);
  if (timing_info_present_flag)
  {
    READBITS(num_units_in_tick, 32);
    READBITS(time_scale, 32);
    READFLAG(fixed_frame_rate_flag);
  }
  READFLAG(nal_hrd_parameters_present_flag);
  if (nal_hrd_parameters_present_flag)
    nal_hrd.read(reader, itemTree);
  READFLAG(vcl_hrd_parameters_present_flag);
  if (vcl_hrd_parameters_present_flag)
    vcl_hrd.read(reader, itemTree);
  if (nal_hrd_parameters_present_flag || vcl_hrd_parameters_present_flag)
    READFLAG(low_delay_hrd_flag);
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

void fileSourceAVCAnnexBFile::sps::vui_parameters_struct::hrd_parameters_struct::read(sub_byte_reader &reader, TreeItem *itemTree)
{
  READUEV(cpb_cnt_minus1);
  READBITS(bit_rate_scale, 4);
  READBITS(cpb_size_scale, 4);
  for (int SchedSelIdx = 0; SchedSelIdx <= cpb_cnt_minus1; SchedSelIdx++)
  {
    READUEV_A(bit_rate_value_minus1, SchedSelIdx);
    READUEV_A(cpb_size_value_minus1, SchedSelIdx);
    READFLAG_A(cbr_flag, SchedSelIdx);
  }
  READBITS(initial_cpb_removal_delay_length_minus1, 5);
  READBITS(cpb_removal_delay_length_minus1, 5);
  READBITS(dpb_output_delay_length_minus1, 5);
  READBITS(time_offset_length, 5);
}

fileSourceAVCAnnexBFile::pps::pps(const nal_unit_avc &nal) : nal_unit_avc(nal)
{
}

void fileSourceAVCAnnexBFile::pps::parse_pps(const QByteArray &parameterSetData, TreeItem *root, const QMap<int, sps*> &p_active_SPS_list)
{
  nalPayload = parameterSetData;
  sub_byte_reader reader(parameterSetData);

  // Create a new TreeItem root for the item
  // The macros will use this variable to add all the parsed variables
  TreeItem *const itemTree = root ? new TreeItem("pic_parameter_set_rbsp()", root) : nullptr;
  
  READUEV(pic_parameter_set_id);
  READUEV(seq_parameter_set_id);

  // Get the referenced sps
  if (!p_active_SPS_list.contains(seq_parameter_set_id))
    throw std::logic_error("The signaled SPS was not found in the bitstream.");
  sps *refSPS = p_active_SPS_list.value(seq_parameter_set_id);

  READFLAG(entropy_coding_mode_flag);
  READFLAG(bottom_field_pic_order_in_frame_present_flag);
  READUEV(num_slice_groups_minus1);
  if (num_slice_groups_minus1 > 0)
  {
    READUEV(slice_group_map_type);
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
  READUEV(num_ref_idx_l1_default_active_minus1);
  READFLAG(weighted_pred_flag);
  READBITS(weighted_bipred_idc, 2);
  READSEV(pic_init_qp_minus26);
  READSEV(pic_init_qs_minus26);
  READSEV(chroma_qp_index_offset);
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
  }

  // rbsp_trailing_bits( )
}

fileSourceAVCAnnexBFile::slice_header::slice_header(const nal_unit_avc &nal) : nal_unit_avc(nal)
{
}

void fileSourceAVCAnnexBFile::slice_header::parse_slice_header(const QByteArray &sliceHeaderData, const QMap<int, sps*> &p_active_SPS_list, const QMap<int, pps*> &p_active_PPS_list, TreeItem *root)
{
  sub_byte_reader reader(sliceHeaderData);

  // Create a new TreeItem root for the item
  // The macros will use this variable to add all the parsed variables
  TreeItem *const itemTree = root ? new TreeItem("pic_parameter_set_rbsp()", root) : nullptr;
  
  READUEV(first_mb_in_slice);
  READUEV(slice_type_id);
  slice_type = (slice_type_enum)(slice_type_id % 5);
  slice_type_fixed = slice_type_id > 4;
  IdrPicFlag = (nal_unit_type == CODED_SLICE_IDR);

  READUEV(pic_parameter_set_id);
  // Get the referenced SPS and PPS
  if (!p_active_PPS_list.contains(pic_parameter_set_id))
    throw std::logic_error("The signaled PPS was not found in the bitstream.");
  pps *refPPS = p_active_PPS_list.value(pic_parameter_set_id);
  if (!p_active_SPS_list.contains(refPPS->seq_parameter_set_id))
    throw std::logic_error("The signaled SPS was not found in the bitstream.");
  sps *refSPS = p_active_SPS_list.value(refPPS->seq_parameter_set_id);

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
      READFLAG(bottom_field_flag)
  }
  if (IdrPicFlag)
    READUEV(idr_pic_id);
  if (refSPS->pic_order_cnt_type == 0)
  {
    int nrBits = refSPS->log2_max_pic_order_cnt_lsb_minus4 + 4;
    READBITS(pic_order_cnt_lsb, nrBits);
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
    READUEV(cabac_init_idc);
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

QList<QByteArray> fileSourceAVCAnnexBFile::seekToFrameNumber(int iFrameNr)
{
  return QList<QByteArray>();
}