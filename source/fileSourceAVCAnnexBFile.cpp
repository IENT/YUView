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

// Read "numBits" bits into the variable "into". 
#define READBITS(into,numBits) {QString code; into=reader.readBits(numBits, &code); if (itemTree) new TreeItem(#into,into,QString("u(v) -> u(%1)").arg(numBits),code, itemTree);}
// Read a flag (1 bit) into the variable "into".
#define READFLAG(into) {into=(reader.readBits(1)!=0); if (itemTree) new TreeItem(#into,into,QString("u(1)"),(into!=0)?"1":"0",itemTree);}
#define READFLAG_A(into,i) {bool b=(reader.readBits(1)!=0); into.append(b); if (itemTree) new TreeItem(QString(#into)+QString("[%1]").arg(i),b,QString("u(1)"),b?"1":"0",itemTree);}
// Read a unsigned ue(v) code from the bitstream into the variable "into"
#define READUEV(into) {QString code; into=reader.readUE_V(&code); if (itemTree) new TreeItem(#into,into,QString("ue(v)"),code,itemTree);}
// Read a signed se(v) code from the bitstream into the variable "into"
#define READSEV(into) {QString code; into=reader.readSE_V(&code); if (itemTree) new TreeItem(#into,into,QString("se(v)"),code,itemTree);}

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

  if (nal_avc.nal_type == SPS)
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
  else if (nal_avc.nal_type == PPS) 
  {
    //// A picture parameter set
    //pps *new_pps = new pps(nal_avc);
    //new_pps->parse_pps(getRemainingNALBytes(), nalRoot);
    //  
    //// Add pps (replace old one if existed)
    //active_PPS_list.insert(new_pps->pps_pic_parameter_set_id, new_pps);

    //// Also add pps to list of all nals
    //nalUnitList.append(new_pps);

    //// Add the PPS ID
    //specificDescription = QString(" PPS_NUT ID %1").arg(new_pps->pps_pic_parameter_set_id);
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
  //else if (nal_avc.nal_type == PREFIX_SEI_NUT || nal_avc.nal_type == SUFFIX_SEI_NUT)
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
    nalRoot->itemData.append(QString("NAL %1: %2").arg(nal_avc.nal_idx).arg(nal_unit_type_toString.value(nal_avc.nal_type)) + specificDescription);
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
  nal_type = (nal_unit_type_id > UNSPECIFIED || nal_unit_type_id < 0) ? UNSPECIFIED : (nal_unit_type)nal_unit_type_id;
}

fileSourceAVCAnnexBFile::sps::sps(const nal_unit_avc &nal) : nal_unit_avc(nal)
{
  mb_adaptive_frame_field_flag = false;
  frame_crop_left_offset = 0;
  frame_crop_right_offset = 0;
  frame_crop_top_offset = 0;
  frame_crop_bottom_offset = 0;
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

  if( profile_idc == 100 || profile_idc == 110 || profile_idc == 122 || profile_idc == 244 || profile_idc ==  44 || profile_idc ==  83 || 
      profile_idc ==  86 || profile_idc == 118 || profile_idc == 128 || profile_idc == 138 || profile_idc == 139 || profile_idc == 134 )
  {
    READUEV(chroma_format_idc);
    if(chroma_format_idc == 3)
    {
      READFLAG(residual_colour_transform_flag);
    }
    READUEV(bit_depth_luma_minus8);
    READUEV(bit_depth_chroma_minus8);
    READFLAG(qpprime_y_zero_transform_bypass_flag);
    READFLAG(seq_scaling_matrix_present_flag);
    if(seq_scaling_matrix_present_flag)
    {
      for(int i=0; i<((chroma_format_idc != 3) ? 8 : 12); i++)
      {
        READFLAG(seq_scaling_list_present_flag[i]);
        if(seq_scaling_list_present_flag[i])
        {
          if(i < 6)
            read_scaling_list(reader, ScalingList4x4[i], 16, &UseDefaultScalingMatrix4x4Flag[i], itemTree);
          else
            read_scaling_list(reader, ScalingList8x8[i-6], 64, &UseDefaultScalingMatrix8x8Flag[i-6], itemTree);
        }
      }
    }
  }

  READUEV(log2_max_frame_num_minus4);
  READUEV(pic_order_cnt_type);
  if(pic_order_cnt_type == 0)
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
  if(!frame_mbs_only_flag)
    READFLAG(mb_adaptive_frame_field_flag);

  READFLAG(direct_8x8_inference_flag);
  READFLAG(frame_cropping_flag)
  if(frame_cropping_flag)
  {
    READUEV(frame_crop_left_offset);
    READUEV(frame_crop_right_offset);
    READUEV(frame_crop_top_offset);
    READUEV(frame_crop_bottom_offset);
  }
  READFLAG(vui_parameters_present_flag);
  if (vui_parameters_present_flag)
    read_vui_parameters(reader, itemTree);
}

void fileSourceAVCAnnexBFile::sps::read_scaling_list(sub_byte_reader &reader, int *scalingList, int sizeOfScalingList, bool *useDefaultScalingMatrixFlag, TreeItem *itemTree)
{
  int lastScale = 8;
  int nextScale = 8;
  for(int j=0; j<sizeOfScalingList; j++)
  {
    if(nextScale != 0)
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

void fileSourceAVCAnnexBFile::sps::read_vui_parameters(sub_byte_reader &reader, TreeItem *itemTree)
{
  // TODO...
}

QByteArray fileSourceAVCAnnexBFile::nal_unit_avc::getNALHeader() const
{
  // TODO: 
  // if( nal_unit_type = = 14 | | nal_unit_type = = 20 | | nal_unit_type = = 21 ) ...
  char out = ((int)nal_ref_idc << 5) + nal_type;
  char c[5] = { 0, 0, 0, 1, out };
  return QByteArray(c, 5);
}
