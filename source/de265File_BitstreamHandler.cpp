/*  YUView - YUV player with advanced analytics toolset
*   Copyright (C) 2015  Institut für Nachrichtentechnik
*                       RWTH Aachen University, GERMANY
*
*   YUView is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   YUView is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with YUView.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "de265File_BitstreamHandler.h"
#include <assert.h>

// Read "numBits" bits into the variable "into" and return false if -1 was returned by the reading function.
#define READBITS(into,numBits) into = file->readBits(numBits); if (into==-1) return false;
// Read one bit and set into to true or false. Return false if -1 was returned by the reading function.
#define READFLAG(into) {int val = file->readBits(1); if (val==-1) return false; into = (val != 0);}
// Read an UEV code. Return false if -1 was returned by the reading function.
#define READUEV(into) into = file->readUE_V(); if (into==-1) return false;

// Read "numBits" bits and ignore them. Return false if -1 was returned by the reading function.
#define IGNOREBITS(numBits) {int val = file->readBits(numBits); if (val==-1) return false;}
// Same as IGNOREBITS but return false if the read value is unequal to 0.
#define IGNOREBITS_Z(numBits) {int val = file->readBits(numBits); if (val!=0) return false;}

bool vps::parse_vps(de265File_FileHandler *file)
{
  int val;

  READBITS(vps_video_parameter_set_id, 4);
  IGNOREBITS(1);  // vps_base_layer_internal_flag
  IGNOREBITS(1);  // vps_base_layer_available_flag
  READBITS(vps_max_layers_minus1, 6);

  // There is more to parse but we are not interested in this information (for now)
  return true;
}

bool sps::parse_sps(de265File_FileHandler *file)
{
  int val;

  READBITS(sps_video_parameter_set_id,4);
  READBITS(sps_max_sub_layers_minus1, 3);
  IGNOREBITS(1); // sps_temporal_id_nesting_flag
  
  {
    int maxNumSubLayersMinus1 = sps_max_sub_layers_minus1;

    /// Profile tier level
    bool profilePresentFlag = true;
    if (profilePresentFlag) {
    
      IGNOREBITS(2); // general_profile_space
      IGNOREBITS(1); // general_tier_flag
      
      int general_profile_idc;
      READBITS(general_profile_idc, 5);
      
      bool general_profile_compatibility_flag[32];
      for( int j = 0; j < 32; j++ ) {
        READFLAG(general_profile_compatibility_flag[j]);
      }
    
      IGNOREBITS(1); // general_progressive_source_flag
      IGNOREBITS(1); // general_interlaced_source_flag
      
      IGNOREBITS(1); // general_non_packed_constraint_flag
      IGNOREBITS(1); // general_frame_only_constraint_flag

      if( general_profile_idc == 4 || general_profile_compatibility_flag[4] || 
        general_profile_idc == 5 || general_profile_compatibility_flag[5] || 
        general_profile_idc == 6 || general_profile_compatibility_flag[6] || 
        general_profile_idc == 7 || general_profile_compatibility_flag[7] ) {

        IGNOREBITS(1); // general_max_12bit_constraint_flag
        IGNOREBITS(1); // general_max_10bit_constraint_flag
        IGNOREBITS(1); // general_max_8bit_constraint_flag
        IGNOREBITS(1); // general_max_422chroma_constraint_flag
        IGNOREBITS(1); // general_max_420chroma_constraint_flag
        IGNOREBITS(1); // general_max_monochrome_constraint_flag
        IGNOREBITS(1); // general_intra_constraint_flag
        IGNOREBITS(1); // general_one_picture_only_constraint_flag
        IGNOREBITS(1); // general_lower_bit_rate_constraint_flag

        IGNOREBITS_Z(34); // general_reserved_zero_34bits
      }
      else {
        IGNOREBITS_Z(43); // general_reserved_zero_43bits
      }

      if( ( general_profile_idc >= 1 && general_profile_idc <= 5 ) ||
          general_profile_compatibility_flag[1] || general_profile_compatibility_flag[2] || 
          general_profile_compatibility_flag[3] || general_profile_compatibility_flag[4] || 
          general_profile_compatibility_flag[5] ) {
      
        IGNOREBITS(1); // general_inbld_flag
      }
      else {
        IGNOREBITS_Z(1); // general_reserved_zero_bit
      }
    }

    IGNOREBITS(8); // general_level_idc
    
    bool sub_layer_profile_present_flag[8];
    bool sub_layer_level_present_flag[8];
    for( int i = 0; i < maxNumSubLayersMinus1; i++ ) {
      READFLAG(sub_layer_profile_present_flag[i]);
      READFLAG(sub_layer_level_present_flag[i]);
    }

    if( maxNumSubLayersMinus1 > 0 )
      for( int i = maxNumSubLayersMinus1; i < 8; i++ ) {
        IGNOREBITS_Z(2); // reserved_zero_2bits[i]
      }

    int sub_layer_profile_idc[8];
    bool sub_layer_profile_compatibility_flag[8][32];
    for( int i = 0; i < maxNumSubLayersMinus1; i++ ) {
      if( sub_layer_profile_present_flag[i] ) {
         
        IGNOREBITS(2); // sub_layer_profile_space[i]
        IGNOREBITS(1); // sub_layer_tier_flag[i]
        
        READBITS(sub_layer_profile_idc[i], 5);
        
        for( int j = 0; j < 32; j++ ) {
          READFLAG(sub_layer_profile_compatibility_flag[i][j]);
        }

        IGNOREBITS(1); // sub_layer_progressive_source_flag[i]
        IGNOREBITS(1); // sub_layer_interlaced_source_flag[i]
        IGNOREBITS(1); // sub_layer_non_packed_constraint_flag[i]
        IGNOREBITS(1); // sub_layer_frame_only_constraint_flag[i]
        
        if( sub_layer_profile_idc[i] == 4 || sub_layer_profile_compatibility_flag[i][4] || 
          sub_layer_profile_idc[i] == 5 || sub_layer_profile_compatibility_flag[i][5] || 
          sub_layer_profile_idc[i] == 6 || sub_layer_profile_compatibility_flag[i][6] || 
          sub_layer_profile_idc[i] == 7 || sub_layer_profile_compatibility_flag[i][7] ) {

          IGNOREBITS(1); // sub_layer_max_12bit_constraint_flag[i]
          IGNOREBITS(1); // sub_layer_max_10bit_constraint_flag[i]
          IGNOREBITS(1); // sub_layer_max_8bit_constraint_flag[i]
          IGNOREBITS(1); // sub_layer_max_422chroma_constraint_flag[i]
          IGNOREBITS(1); // sub_layer_max_420chroma_constraint_flag[i]
          IGNOREBITS(1); // sub_layer_max_monochrome_constraint_flag[i]
          IGNOREBITS(1); // sub_layer_intra_constraint_flag[i]
          IGNOREBITS(1); // sub_layer_one_picture_only_constraint_flag[i]
          IGNOREBITS(1); // sub_layer_lower_bit_rate_constraint_flag[i]

          IGNOREBITS_Z(34); // sub_layer_reserved_zero_34bits[i]
        }
        else {
          IGNOREBITS_Z(43); // sub_layer_reserved_zero_43bits[i]
        }

        if(( sub_layer_profile_idc[i] >= 1 && sub_layer_profile_idc[i] <= 5 ) || 
           sub_layer_profile_compatibility_flag[1] || sub_layer_profile_compatibility_flag[2] || 
           sub_layer_profile_compatibility_flag[3] || sub_layer_profile_compatibility_flag[4] || 
           sub_layer_profile_compatibility_flag[5] ) {
          IGNOREBITS(1); // sub_layer_inbld_flag[i]
        }
        else {
          IGNOREBITS_Z(1); // sub_layer_reserved_zero_bit[i]
        }
      }

      if( sub_layer_level_present_flag[i] ) {
        IGNOREBITS(8); // sub_layer_level_idc[i]
      }
    }
  } // End of profile/tier/level
  
  /// Back to the seq_parameter_set_rbsp

  READUEV(sps_seq_parameter_set_id);
  READUEV(chroma_format_idc);

  if (chroma_format_idc == 3) {
    READBITS(separate_colour_plane_flag,1);
  }
  else
    separate_colour_plane_flag = false;

  READUEV(pic_width_in_luma_samples);
  READUEV(pic_height_in_luma_samples);
  READFLAG(conformance_window_flag);

  if (conformance_window_flag) {
    READUEV(conf_win_left_offset);
    READUEV(conf_win_right_offset);
    READUEV(conf_win_top_offset);
    READUEV(conf_win_bottom_offset);
  }
  else {
    conf_win_left_offset = 0;
    conf_win_right_offset = 0;
    conf_win_top_offset = 0;
    conf_win_bottom_offset = 0;
  }

  READUEV(bit_depth_luma_minus8);
  READUEV(bit_depth_chroma_minus8);
  READUEV(log2_max_pic_order_cnt_lsb_minus4);

  // There is more to parse but we are not interested in this information (for now)
  return true;
}

bool pps::parse_pps(de265File_FileHandler *file)
{
  int val;

  READUEV(pps_pic_parameter_set_id);
  READUEV(pps_seq_parameter_set_id);

  IGNOREBITS(1); // dependent_slice_segments_enabled_flag
  READFLAG(output_flag_present_flag);
  READBITS(num_extra_slice_header_bits, 3);

  // There is more to parse but we are not interested in this information (for now)
  return true;
}

// Only true for the first slice instance
bool slice::bFirstAUInDecodingOrder = true;
int slice::prevTid0Pic_slice_pic_order_cnt_lsb = 0;
int slice::prevTid0Pic_PicOrderCntMsb = 0;

// T-REC-H.265-201410 - 7.3.6.1 slice_segment_header()
bool slice::parse_slice(de265File_FileHandler *file,
                        QMap<int, sps*> p_active_SPS_list,
                        QMap<int, pps*> p_active_PPS_list )
{
  int val;

  val = file->readBits(1);
  first_slice_segment_in_pic_flag = (val == 0) ? false : true;
  if (val == -1) return false;

  if (!first_slice_segment_in_pic_flag) {
    // We are only interested in the first slices of each picture
    return true;
  }
  
  if (nal_type == BLA_W_LP     || nal_type == BLA_W_RADL ||
    nal_type == BLA_N_LP       || nal_type == IDR_W_RADL ||
    nal_type == IDR_N_LP       || nal_type == CRA_NUT    ||
    nal_type == RSV_IRAP_VCL22 || nal_type == RSV_IRAP_VCL23 ) {
        
    // no_output_of_prior_pics_flag
    val = file->readBits(1);
    if (val == -1) return false;
  }

  slice_pic_parameter_set_id = file->readUE_V();
  if (slice_pic_parameter_set_id == -1) return false;

  // For the first slice dependent_slice_segment_flag is 0 so we can continue here.

  // Get the active PPS with the read ID
  if (!p_active_PPS_list.contains(slice_pic_parameter_set_id))
    // Parameter set not found. Something went wrong.
    return false;
  pps *act_pps = p_active_PPS_list[slice_pic_parameter_set_id];

  if (act_pps == NULL) return false;
        
  for (int i=0; i < act_pps->num_extra_slice_header_bits; i++) {
    // slice_reserved_flag
    val = file->readBits(1);
    if (val == -1) return false;
  }

  slice_type = file->readUE_V();
  if (slice_type == -1) return false;

  if (act_pps->output_flag_present_flag) {
    val = file->readBits(1);
    pic_output_flag = (val == 0) ? false : true;
    if (val == -1) return false;
  }

  // Get the active sps with the read ID
  if (!p_active_SPS_list.contains(act_pps->pps_seq_parameter_set_id))
    // Parameter set not found. Something went wrong.
    return false;
  sps *act_sps = p_active_SPS_list[act_pps->pps_seq_parameter_set_id];

  if (act_sps->separate_colour_plane_flag) {
    colour_plane_id = file->readBits(2);
    if (colour_plane_id == -1) return false;
  }

  if( nal_type != IDR_W_RADL && nal_type != IDR_N_LP ) {
    slice_pic_order_cnt_lsb = file->readBits( act_sps->log2_max_pic_order_cnt_lsb_minus4 + 4 );

    // ... short_term_ref_pic_set_sps_flag and so on
  }
  else {
    slice_pic_order_cnt_lsb = 0;
  }

  // Thats all we wanted from the slice header

  // Calculate the picture order count
  int MaxPicOrderCntLsb = 1 << (act_sps->log2_max_pic_order_cnt_lsb_minus4 + 4);

  // The variable NoRaslOutputFlag is specified as follows:
  bool NoRaslOutputFlag = false;
  if (nal_type == IDR_W_RADL || nal_type == BLA_W_LP)
    NoRaslOutputFlag = true;
  else if (bFirstAUInDecodingOrder) {
    NoRaslOutputFlag = true;
    bFirstAUInDecodingOrder = false;
  }

  // T-REC-H.265-201410 - 8.3.1 Decoding process for picture order count
  
  int prevPicOrderCntLsb = 0;
  int prevPicOrderCntMsb = 0;
  // When the current picture is not an IRAP picture with NoRaslOutputFlag equal to 1, ...
  if (!(isIRAP() && NoRaslOutputFlag)) {
    // the variables prevPicOrderCntLsb and prevPicOrderCntMsb are derived as follows:
     
    prevPicOrderCntLsb = prevTid0Pic_slice_pic_order_cnt_lsb;
    prevPicOrderCntMsb = prevTid0Pic_PicOrderCntMsb;
  }

  // The variable PicOrderCntMsb of the current picture is derived as follows:
  if (isIRAP() && NoRaslOutputFlag) {
    // If the current picture is an IRAP picture with NoRaslOutputFlag equal to 1, PicOrderCntMsb is set equal to 0.
    PicOrderCntMsb = 0;
  }
  else {
    // Otherwise, PicOrderCntMsb is derived as follows: (8-1)
    if((slice_pic_order_cnt_lsb < prevPicOrderCntLsb) && ((prevPicOrderCntLsb - slice_pic_order_cnt_lsb) >= (MaxPicOrderCntLsb / 2)))
      PicOrderCntMsb = prevPicOrderCntMsb + MaxPicOrderCntLsb;
    else if ((slice_pic_order_cnt_lsb > prevPicOrderCntLsb ) && ((slice_pic_order_cnt_lsb - prevPicOrderCntLsb) > (MaxPicOrderCntLsb / 2))) 
      PicOrderCntMsb = prevPicOrderCntMsb - MaxPicOrderCntLsb;
    else 
      PicOrderCntMsb = prevPicOrderCntMsb;
  }

  // PicOrderCntVal is derived as follows: (8-2)
  PicOrderCntVal = PicOrderCntMsb + slice_pic_order_cnt_lsb;

  if (nuh_temporal_id_plus1 - 1 == 0 && !isRASL() && !isRADL()) {
    // Let prevTid0Pic be the previous picture in decoding order that has TemporalId 
    // equal to 0 and that is not a RASL picture, a RADL picture or an SLNR picture.

    // Set these for the next slice
    prevTid0Pic_slice_pic_order_cnt_lsb = slice_pic_order_cnt_lsb;
    prevTid0Pic_PicOrderCntMsb = PicOrderCntMsb;
  }

  return true;
}


// The file handler, ... well ... it handeles the Annex B formatted file.
de265File_FileHandler::de265File_FileHandler(QString fileName)
{
  // Open the input file
  p_srcFile = new QFile(fileName);
  p_srcFile->open(QIODevice::ReadOnly);

  // Fill the buffer
  p_FileBuffer.resize(BUFFER_SIZE);
  p_FileBufferSize = p_srcFile->read(p_FileBuffer.data(), BUFFER_SIZE);
  p_posInBuffer = 0;
  p_posInBuffer_bits = 0;
  p_bufferStartPosInFile = 0;
  p_numEmuPrevZeroBytes = 0;

  // Set the start code to look for
  p_startCode.append((char)0);
  p_startCode.append((char)0);
  p_startCode.append((char)1);
  
  // Get the positions where we can start decoding
  scanFileForNalUnits();
}

bool de265File_FileHandler::updateBuffer()
{
  // Save the position of the first byte in this new buffer
  p_bufferStartPosInFile += p_FileBufferSize;

  p_FileBufferSize = p_srcFile->read(p_FileBuffer.data(), BUFFER_SIZE);
  p_posInBuffer = 0;
  p_posInBuffer_bits = 0;

  return (p_FileBufferSize > 0);
}

bool de265File_FileHandler::seekToNextNALUnit()
{
  if (p_posInBuffer_bits != 0) {
    // Go to the next whole byte
    if (!gotoNextByte())
      return false;
  }
  p_numEmuPrevZeroBytes = 0;

  // Check if there is another start code in the buffer
  int idx = p_FileBuffer.indexOf(p_startCode, p_posInBuffer);
  while (idx < 0) {
    // Start code not found in this buffer. Load next chuck of data from file.

    // Before we load more data, check with how many zeroes the current buffer ends.
    // This could be the beginning of a start code.
    int nrZeros = 0;
    for (int i = 1; i <= 3; i++) {
      if (p_FileBuffer.at(p_FileBufferSize-i) == 0)
        nrZeros++;
    }
    
    // Load the next buffer
    if (!updateBuffer()) {
      // Out of file
      return false;
    }

    if (nrZeros > 0) {
      // The last buffer ended with zeroes. 
      // Now check if the beginning of this buffer is the remaining part of a start code
      if ((nrZeros == 2 || nrZeros == 3) && p_FileBuffer.at(0) == 1) {
        // Start code found
        p_posInBuffer = 1;
        return true;
      }

      if ((nrZeros == 1) && p_FileBuffer.at(0) == 0 && p_FileBuffer.at(1) == 1) {
        // Start code found
        p_posInBuffer = 2;
        return true;
      }
    }

    // New buffer loaded but no start code found yet. Search for it again.
    idx = p_FileBuffer.indexOf(p_startCode, p_posInBuffer);
  }

  assert(idx >= 0);
  if (idx + 3 >= p_FileBufferSize) {
    // The start code is exactly at the end of the current buffer. 
    if (!updateBuffer()) {
      // Out of file
      return false;
    }
    return true;
  }

  // Update buffer position
  p_posInBuffer = idx + 3;
  return true;
}

bool de265File_FileHandler::gotoNextByte()
{
  // Before we go to the neyt byte, check if the last (current) byte is a zero byte.
  if (getCurByte() == 0)
    p_numEmuPrevZeroBytes++;

  // Skip the remaining sub-byte-bits
  p_posInBuffer_bits = 0;
  // Advance pointer
  p_posInBuffer++;
  
  if (p_posInBuffer >= p_FileBufferSize) {
    // The next byte is outside of the current buffer. Update the buffer.
    if (!updateBuffer()) {
      // Update failed. No more file left. Going to the next byte failed.
      return false;
    }
  }

  if (p_numEmuPrevZeroBytes == 2 && getCurByte() == 3) {
    // The current byte is an emulation prevention 3 byte. Skip it.
    
    // Advance pointer
    p_posInBuffer++;

    if (p_posInBuffer >= p_FileBufferSize) {
      // The next byte is outside of the current buffer. Update the buffer.
      if (!updateBuffer()) {
        // No more file left. Going to the next byte failed.
        return false;
      }
    }

    // Reset counter
    p_numEmuPrevZeroBytes = 0;
  }
  else if (getCurByte() != 0)
    // No zero byte. No emulation prevention 3 byte
    p_numEmuPrevZeroBytes = 0;

  return true;
}

bool de265File_FileHandler::scanFileForNalUnits()
{
  // These maps hold the last active VPS, SPS and PPS. This is required for parsing
  // the parameter sets.
  QMap<int, sps*> p_active_SPS_list;
  QMap<int, pps*> p_active_PPS_list;

  while (seekToNextNALUnit()) {
    // Seek successfull. Parse the NAL header.

    // Save current position in file
    quint64 curFilePos = tell();

    // Read forbidden_zeor_bit
    int forbidden_zero_bit = readBits(1);
    if (forbidden_zero_bit != 0) return false;

    // Read nal unit type
    int nal_unit_type_id = readBits(6);
    if (nal_unit_type_id == -1) return false;
    nal_unit_type nal_type = (nal_unit_type_id > UNSPECIFIED) ? UNSPECIFIED : (nal_unit_type)nal_unit_type_id;

    // Read layer id
    int layer_id = readBits(6);
    if (layer_id == -1) return false;
    
    int temporal_id_plus1 = readBits(3);
    if (temporal_id_plus1 == -1) return false;
    
    if (nal_type == IDR_W_RADL || nal_type == IDR_N_LP   || nal_type == CRA_NUT ||
        nal_type == BLA_W_LP   || nal_type == BLA_W_RADL || nal_type == BLA_N_LP ) {
      // Create a new slice unit
      slice *newSlice = new slice(curFilePos, nal_type, layer_id, temporal_id_plus1);
      if (!newSlice->parse_slice(this, p_active_SPS_list, p_active_PPS_list)) return false;

      if (newSlice->first_slice_segment_in_pic_flag) {
        // This is the first slice of a random access pont. Add it to the list.
        p_nalUnitList.append(newSlice);

        // Get the poc
        if (newSlice->PicOrderCntVal != -1) {
          if (!p_addPOCToList(newSlice->PicOrderCntVal)) return false;
        }
      }
      else {
        // Do not add. Delete.
        delete newSlice;
      }
    }
    else if (nal_type == TRAIL_N  || nal_type == TRAIL_R    || nal_type == TSA_N  ||
             nal_type == TSA_R    || nal_type == STSA_N     || nal_type == STSA_R ||
             nal_type == RADL_N   || nal_type == RADL_R     || nal_type == RASL_N ||
             nal_type == RASL_R ) {
      
      // This is a slice and has a slice header (and a POC) but it is not a random access picture.
      // Parse the slice header (at least until we know the POC).
      slice *newSlice = new slice(curFilePos, nal_type, layer_id, temporal_id_plus1);
      if (!newSlice->parse_slice(this, p_active_SPS_list, p_active_PPS_list)) return false;

      // Get the poc
      if (newSlice->PicOrderCntVal != -1) {
        if (!p_addPOCToList(newSlice->PicOrderCntVal)) return false;
      }

      // Don't save the position of non random access points.
      delete newSlice;
    }
    else if (nal_type == PPS_NUT) {
      // A picture parameter set
      pps *new_pps = new pps(curFilePos, nal_type, layer_id, temporal_id_plus1);
      if (!new_pps->parse_pps(this)) return false;
      
      // Add pps (replace old one if existed)
      p_active_PPS_list.insert(new_pps->pps_pic_parameter_set_id, new_pps);

      // Also add pps to list of all nals
      p_nalUnitList.append(new_pps);
    }
    else if (nal_type == SPS_NUT) {
      // A sequence parameter set
      sps *new_sps = new sps(curFilePos, nal_type, layer_id, temporal_id_plus1);
      if (!new_sps->parse_sps(this)) return false;
      
      // Add sps (replace old one if existed)
      p_active_SPS_list.insert(new_sps->sps_seq_parameter_set_id, new_sps);

      // Also add sps to list of all nals
      p_nalUnitList.append(new_sps);
    }
    else if (nal_type == VPS_NUT) {
      // A video parameter set
      vps *new_vps = new vps(curFilePos, nal_type, layer_id, temporal_id_plus1);
      if (!new_vps->parse_vps(this)) return false;

      // Put parameter sets into the NAL unit list
      p_nalUnitList.append(new_vps);
    }
    
  }
  return true;
}

int de265File_FileHandler::readBits(int nrBits)
{
  int out = 0;

  while (nrBits > 0) {

    // How many bits can be gotton from the current byte?
    int curBitsLeft = 8 - p_posInBuffer_bits;

    int readBits;	// Nr of bits to read
    int offset;		// Offset for reading from the right
    if (nrBits >= curBitsLeft) {
      // Read "curBitsLeft" bits
      readBits = curBitsLeft;
      offset = 0;
    }
    else {
      // Read "nrBits" bits
      assert(nrBits < 8 && nrBits < curBitsLeft);
      readBits = nrBits;
      offset = curBitsLeft - nrBits;
    }

    // Shift output value so that the new bits fit
    out = out << readBits;

    char c = getCurByte();
    c = c >> offset;
    int mask = ((1<<readBits) - 1);

    // Write bits to output
    out += (c & mask);

    // Update counters
    nrBits -= readBits;
    p_posInBuffer_bits += readBits;

    if (p_posInBuffer_bits == 8) {
      // We read all bits we could from the current byte.
      // Go to the next byte
      if (!gotoNextByte()) {
        // We could not get as many bytes as requested.
        return -1;
      }

    }
  }
  
  return out;
}

int de265File_FileHandler::readUE_V()
{
  int readBit = readBits(1);
  if (readBit == 1)
    return 0;
  
  // Get the length of the golomb
  int golLength = 0;
  while (readBit == 0) {
    readBit = readBits(1);
    golLength++;
  }

  // Read "golLength" bits
  int val = readBits(golLength);
  // Add the exponentional part
  val += (1 << golLength)-1;
  
  return val;
}

bool de265File_FileHandler::p_addPOCToList(int poc)
{
  if (poc < 0)
    return false;

  if (p_POC_List.contains(poc)) {
    // Two pictures with the same POC are not allowed
    return false;
  }
  
  p_POC_List.append(poc);
  return true;
}