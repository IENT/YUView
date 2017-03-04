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

#include "fileSourceHEVCAnnexBFile.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <exception>
#include <QApplication>
#include <QDebug>
#include <QProgressDialog>
#include <QSize>
#include "mainwindow.h"
#include "typedef.h"

#define HEVCANNEXBFILE_DEBUG_OUTPUT 0
#if HEVCANNEXBFILE_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_ANNEXB qDebug
#else
#define DEBUG_ANNEXB(fmt,...) ((void)0)
#endif

unsigned int fileSourceHEVCAnnexBFile::sub_byte_reader::readBits(int nrBits, QString *bitsRead)
{
  int out = 0;
  int nrBitsRead = nrBits;

  // The return unsigned int is of depth 32 bits
  if (nrBits > 32)
    throw std::logic_error("Trying to read more than 32 bits at once from the bitstream.");

  while (nrBits > 0)
  {
    if (posInBuffer_bits == 8 && nrBits != 0) 
    {
      // We read all bits we could from the current byte but we need more. Go to the next byte.
      if (!p_gotoNextByte())
        // We are at the end of the buffer but we need to read more. Error.
        throw std::logic_error("Error while reading annexB file. Trying to read over buffer boundary.");
    }

    // How many bits can be gotton from the current byte?
    int curBitsLeft = 8 - posInBuffer_bits;

    int readBits; // Nr of bits to read
    int offset;   // Offset for reading from the right
    if (nrBits >= curBitsLeft)
    {
      // Read "curBitsLeft" bits
      readBits = curBitsLeft;
      offset = 0;
    }
    else 
    {
      // Read "nrBits" bits
      assert(nrBits < 8 && nrBits < curBitsLeft);
      readBits = nrBits;
      offset = curBitsLeft - nrBits;
    }

    // Shift output value so that the new bits fit
    out = out << readBits;

    char c = p_byteArray[posInBuffer_bytes];
    c = c >> offset;
    int mask = ((1<<readBits) - 1);

    // Write bits to output
    out += (c & mask);

    // Update counters
    nrBits -= readBits;
    posInBuffer_bits += readBits;
  }
  
  if (bitsRead)
    for (int i = nrBitsRead-1; i >= 0; i--)
    {
      if (out & (1 << i))
        bitsRead->append("1");
      else
        bitsRead->append("0");
    }

  return out;
}

int fileSourceHEVCAnnexBFile::sub_byte_reader::readUE_V(QString *bitsRead)
{
  int readBit = readBits(1, bitsRead);
  if (readBit == 1)
    return 0;
  
  // Get the length of the golomb
  int golLength = 0;
  while (readBit == 0) 
  {
    readBit = readBits(1, bitsRead);
    golLength++;
  }

  // Read "golLength" bits
  int val = readBits(golLength, bitsRead);
  // Add the exponentional part
  val += (1 << golLength)-1;
  
  return val;
}

int fileSourceHEVCAnnexBFile::sub_byte_reader::readSE_V(QString *bitsRead)
{
  int val = readUE_V(bitsRead);
  if (val%2 == 0) 
    return -(val+1)/2;
  else
    return (val+1)/2;
}

bool fileSourceHEVCAnnexBFile::sub_byte_reader::p_gotoNextByte()
{
  // Before we go to the neyt byte, check if the last (current) byte is a zero byte.
  if (p_byteArray[posInBuffer_bytes] == (char)0)
    p_numEmuPrevZeroBytes++;

  // Skip the remaining sub-byte-bits
  posInBuffer_bits = 0;
  // Advance pointer
  posInBuffer_bytes++;
  
  if (posInBuffer_bytes >= p_byteArray.size()) 
    // The next byte is outside of the current buffer. Error.
    return false;    

  if (p_numEmuPrevZeroBytes == 2 && p_byteArray[posInBuffer_bytes] == (char)3) 
  {
    // The current byte is an emulation prevention 3 byte. Skip it.
    posInBuffer_bytes++; // Skip byte

    if (posInBuffer_bytes >= p_byteArray.size()) {
      // The next byte is outside of the current buffer. Error
      return false;
    }

    // Reset counter
    p_numEmuPrevZeroBytes = 0;
  }
  else if (p_byteArray[posInBuffer_bytes] != (char)0)
    // No zero byte. No emulation prevention 3 byte
    p_numEmuPrevZeroBytes = 0;

  return true;
}

/* Some macros that we use to read syntax elements from the bitstream.
 * The advantage of these macros is, that they can directly also create the tree structure for the QAbstractItemModel that is 
 * used to show the NAL units and their content. The tree will only be added if the pointer to the given tree itemTree is valid.
*/
// Read "numBits" bits into the variable "into". 
#define READBITS(into,numBits) {QString code; into=reader.readBits(numBits, &code); if (itemTree) new TreeItem(#into,into,QString("u(v) -> u(%1)").arg(numBits),code, itemTree);}
#define READBITS_A(into,numBits,i) {QString code; int v=reader.readBits(numBits,&code); into.append(v); if (itemTree) new TreeItem(QString(#into)+QString("[%1]").arg(i),v,QString("u(v) -> u(%1)").arg(numBits),code, itemTree);}
// Read a flag (1 bit) into the variable "into".
#define READFLAG(into) {into=(reader.readBits(1)!=0); if (itemTree) new TreeItem(#into,into,QString("u(1)"),(into!=0)?"1":"0",itemTree);}
#define READFLAG_A(into,i) {bool b=(reader.readBits(1)!=0); into.append(b); if (itemTree) new TreeItem(QString(#into)+QString("[%1]").arg(i),b,QString("u(1)"),b?"1":"0",itemTree);}
// Read a unsigned ue(v) code from the bitstream into the variable "into"
#define READUEV(into) {QString code; into=reader.readUE_V(&code); if (itemTree) new TreeItem(#into,into,QString("ue(v)"),code,itemTree);}
#define READUEV_A(arr,i) {QString code; int v=reader.readUE_V(&code); arr.append(v); if (itemTree) new TreeItem(QString(#arr)+QString("[%1]").arg(i),v,QString("ue(v)"),code,itemTree);}
// Read a signed se(v) code from the bitstream into the variable "into"
#define READSEV(into) {QString code; into=reader.readSE_V(&code); if (itemTree) new TreeItem(#into,into,QString("se(v)"),code,itemTree);}
#define READSEV_A(into,i) {QString code; int v=reader.readSE_V(&code); into.append(v); if (itemTree) new TreeItem(QString(#into)+QString("[%1]").arg(i),v,QString("se(v)"),code,itemTree);}
// Do not actually read anything but also put the value into the tree as a calculated value
#define LOGVAL(val) {if (itemTree) new TreeItem(#val,val,QString("calc"),QString(),itemTree);}
// Log a string and a value
#define LOGSTRVAL(str,val) {if (itemTree) new TreeItem(str,val,QString("calc"),QString(),itemTree);}

// Read "numBits" bits and ignore them. Return false if -1 was returned by the reading function.
#define IGNOREBITS(numBits) {int val = reader.readBits(numBits);}
// Read an UEV code and ignore the value. Return false if -1 was returned by the reading function.
#define IGNOREUEV() {int into = reader.readUE_V();}

void fileSourceHEVCAnnexBFile::profile_tier_level::parse_profile_tier_level(sub_byte_reader &reader, bool profilePresentFlag, int maxNumSubLayersMinus1, TreeItem *root)
{
  // Create a new TreeItem root for the item
  // The macros will use this variable to add all the parsed variables
  TreeItem *const itemTree = root ? new TreeItem("profile_tier_level()", root) : nullptr;

  /// Profile tier level
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

    int nrGeneralReservedZeroBits = 0;
    if( general_profile_idc == 4 || general_profile_compatibility_flag[4] || 
      general_profile_idc == 5 || general_profile_compatibility_flag[5] || 
      general_profile_idc == 6 || general_profile_compatibility_flag[6] || 
      general_profile_idc == 7 || general_profile_compatibility_flag[7] ) 
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

      // READBITS(general_reserved_zero_bits, 34);
      nrGeneralReservedZeroBits = 34;
    }
    else
      //READBITS(general_reserved_zero_bits, 43);
      nrGeneralReservedZeroBits = 43;

    // Read the zero nrGeneralReservedZeroBits bits. 
    // The readbits function can only read 32 bits at once so read the bits in multiple steps.
    QString code;
    int zero = reader.readBits(32, &code);
    int zero2 = reader.readBits(nrGeneralReservedZeroBits-32, &code);
    if (zero != 0 || zero2 != 0)
      throw std::logic_error("general_reserved_zero_bits were not zero.");
    if (itemTree)
      if (itemTree) new TreeItem("general_reserved_zero_bits",0,QString("u(v) -> u(%1)").arg(nrGeneralReservedZeroBits),code, itemTree);

    if( ( general_profile_idc >= 1 && general_profile_idc <= 5 ) ||
        general_profile_compatibility_flag[1] || general_profile_compatibility_flag[2] || 
        general_profile_compatibility_flag[3] || general_profile_compatibility_flag[4] || 
        general_profile_compatibility_flag[5] )
      READFLAG(general_inbld_flag)
    else 
      READFLAG(general_reserved_zero_bit);
  }

  READBITS(general_level_idc, 8);
    
  for( int i = 0; i < maxNumSubLayersMinus1; i++ ) 
  {
    READFLAG(sub_layer_profile_present_flag[i]);
    READFLAG(sub_layer_level_present_flag[i]);
  }

  if( maxNumSubLayersMinus1 > 0 )
    for( int i = maxNumSubLayersMinus1; i < 8; i++ ) 
      READBITS(reserved_zero_2bits[i], 2);

  for( int i = 0; i < maxNumSubLayersMinus1; i++ ) 
  {
    if( sub_layer_profile_present_flag[i] ) 
    {
      READBITS(sub_layer_profile_space[i], 2);
      READFLAG(sub_layer_tier_flag[i]);
        
      READBITS(sub_layer_profile_idc[i], 5);
      
      for( int j = 0; j < 32; j++ )
        READFLAG(sub_layer_profile_compatibility_flag[i][j]);

      READFLAG(sub_layer_progressive_source_flag[i]);
      READFLAG(sub_layer_interlaced_source_flag[i]);
      READFLAG(sub_layer_non_packed_constraint_flag[i]);
      READFLAG(sub_layer_frame_only_constraint_flag[i]);
        
      int nrSubLayerReservedZeroBits;
      if( sub_layer_profile_idc[i] == 4 || sub_layer_profile_compatibility_flag[i][4] || 
        sub_layer_profile_idc[i] == 5 || sub_layer_profile_compatibility_flag[i][5] || 
        sub_layer_profile_idc[i] == 6 || sub_layer_profile_compatibility_flag[i][6] || 
        sub_layer_profile_idc[i] == 7 || sub_layer_profile_compatibility_flag[i][7] ) 
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

        //READBITS(sub_layer_reserved_zero_bits[i], 34);
        nrSubLayerReservedZeroBits = 34;
      }
      else
        //READBITS(sub_layer_reserved_zero_bits[i], 43);
        nrSubLayerReservedZeroBits = 43;

      // Read the zero nrSubLayerReservedZeroBits bits. 
      // The readbits function can only read 32 bits at once so read the bits in multiple steps.
      QString code;
      int zero = reader.readBits(32, &code);
      int zero2 = reader.readBits(nrSubLayerReservedZeroBits-32, &code);
      if (zero != 0 || zero2 != 0)
        throw std::logic_error("general_reserved_zero_bits were not zero.");
      if (itemTree)
        if (itemTree) new TreeItem("general_reserved_zero_bits",0,QString("u(v) -> u(%1)").arg(nrSubLayerReservedZeroBits),code, itemTree);


      if(( sub_layer_profile_idc[i] >= 1 && sub_layer_profile_idc[i] <= 5 ) || 
          sub_layer_profile_compatibility_flag[1] || sub_layer_profile_compatibility_flag[2] || 
          sub_layer_profile_compatibility_flag[3] || sub_layer_profile_compatibility_flag[4] || 
          sub_layer_profile_compatibility_flag[5] )
        READFLAG(sub_layer_inbld_flag[i])
      else
        READFLAG(sub_layer_reserved_zero_bit[i]);
    }

    if( sub_layer_level_present_flag[i] ) 
      READBITS(sub_layer_level_idc[i], 8);
  }
}

void fileSourceHEVCAnnexBFile::sub_layer_hrd_parameters::parse_sub_layer_hrd_parameters(sub_byte_reader &reader, int subLayerId, int CpbCnt, bool sub_pic_hrd_params_present_flag, TreeItem *root)
{
  Q_UNUSED(subLayerId);

  // Create a new TreeItem root for the item
  // The macros will use this variable to add all the parsed variables
  TreeItem *const itemTree = root ? new TreeItem("sub_layer_hrd_parameters()", root) : nullptr;

  if (CpbCnt >= 32)
    throw std::logic_error("The value of CpbCnt must be in the range of 0 to 31");

  for(int i = 0; i <= CpbCnt; i++)
  {
    READUEV_A(bit_rate_value_minus1, i);
    READUEV_A(cpb_size_value_minus1, i);
    if (sub_pic_hrd_params_present_flag)
    {
      READUEV_A(cpb_size_du_value_minus1, i);
      READUEV_A(bit_rate_du_value_minus1, i);
    }
    READFLAG_A(cbr_flag, i);
  }
}

void fileSourceHEVCAnnexBFile::hrd_parameters::parse_hrd_parameters(sub_byte_reader &reader, bool commonInfPresentFlag, int maxNumSubLayersMinus1, TreeItem *root)
{
  // Create a new TreeItem root for the item
  // The macros will use this variable to add all the parsed variables
  TreeItem *const itemTree = root ? new TreeItem("hrd_parameters()", root) : nullptr;

  sub_pic_hrd_params_present_flag = false;

  if (commonInfPresentFlag)
  {
    READFLAG(nal_hrd_parameters_present_flag);
    READFLAG(vcl_hrd_parameters_present_flag);

    if(nal_hrd_parameters_present_flag || vcl_hrd_parameters_present_flag)
    {
      READFLAG(sub_pic_hrd_params_present_flag);
      if (sub_pic_hrd_params_present_flag)
      {
        READBITS(tick_divisor_minus2, 8);
        READBITS(du_cpb_removal_delay_increment_length_minus1, 5);
        READFLAG(sub_pic_cpb_params_in_pic_timing_sei_flag);
        READBITS(dpb_output_delay_du_length_minus1, 5);
      }
      READBITS(bit_rate_scale, 4);
      READBITS(cpb_size_scale, 4);
      if(sub_pic_hrd_params_present_flag)
        READBITS(cpb_size_du_scale, 4)
      READBITS(initial_cpb_removal_delay_length_minus1, 5);
      READBITS(au_cpb_removal_delay_length_minus1, 5);
      READBITS(dpb_output_delay_length_minus1, 5);
    }
  }

  if (maxNumSubLayersMinus1 >= 8)
    throw std::logic_error("The value of maxNumSubLayersMinus1 must be in the range of 0 to 7");

  for(int i = 0; i <= maxNumSubLayersMinus1; i++) 
  {
    READFLAG(fixed_pic_rate_general_flag[i]);
    if (fixed_pic_rate_general_flag[i])
      READFLAG(fixed_pic_rate_within_cvs_flag[i]);
    if (fixed_pic_rate_within_cvs_flag[i])
    {
      READFLAG(elemental_duration_in_tc_minus1[i]);
      low_delay_hrd_flag[i] = false;
    }
    else
      READFLAG(low_delay_hrd_flag[i]);
    cpb_cnt_minus1[i] = 0;
    if (!low_delay_hrd_flag[i])
      READUEV(cpb_cnt_minus1[i]);

    if(nal_hrd_parameters_present_flag)
      nal_sub_hrd[i].parse_sub_layer_hrd_parameters(reader, i, cpb_cnt_minus1[i], sub_pic_hrd_params_present_flag, itemTree);
    if( vcl_hrd_parameters_present_flag )
      vcl_sub_hrd[i].parse_sub_layer_hrd_parameters(reader, i, cpb_cnt_minus1[i], sub_pic_hrd_params_present_flag, itemTree);
  }
}

void fileSourceHEVCAnnexBFile::pred_weight_table::parse_pred_weight_table(sub_byte_reader &reader, sps *actSPS, slice *actSlice, TreeItem *root)
{
  // Create a new TreeItem root for the item
  // The macros will use this variable to add all the parsed variables
  TreeItem *const itemTree = root ? new TreeItem("pred_weight_table()", root) : nullptr;

  READUEV(luma_log2_weight_denom);
  if (actSPS->ChromaArrayType != 0)
    READSEV(delta_chroma_log2_weight_denom);
  for(int i = 0; i <= actSlice->num_ref_idx_l0_active_minus1; i++)
    READFLAG_A(luma_weight_l0_flag, i);
  if(actSPS->ChromaArrayType != 0)
    for(int i = 0; i <= actSlice->num_ref_idx_l0_active_minus1; i++)
      READFLAG_A(chroma_weight_l0_flag, i);
  for(int i = 0; i <= actSlice->num_ref_idx_l0_active_minus1; i++)
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

  if(actSlice->slice_type == 0 ) // B
  {
    for(int i = 0; i <= actSlice->num_ref_idx_l1_active_minus1; i++)
      READFLAG_A(luma_weight_l1_flag, i);
    if(actSPS->ChromaArrayType != 0)
      for(int i = 0; i <= actSlice->num_ref_idx_l1_active_minus1; i++)
        READFLAG_A(chroma_weight_l1_flag, i);
    for(int i = 0; i <= actSlice->num_ref_idx_l1_active_minus1; i++)
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
}

int fileSourceHEVCAnnexBFile::st_ref_pic_set::NumNegativePics[65];
int fileSourceHEVCAnnexBFile::st_ref_pic_set::NumPositivePics[65];
int fileSourceHEVCAnnexBFile::st_ref_pic_set::DeltaPocS0[65][16];
int fileSourceHEVCAnnexBFile::st_ref_pic_set::DeltaPocS1[65][16];
bool fileSourceHEVCAnnexBFile::st_ref_pic_set::UsedByCurrPicS0[65][16];
bool fileSourceHEVCAnnexBFile::st_ref_pic_set::UsedByCurrPicS1[65][16];
int fileSourceHEVCAnnexBFile::st_ref_pic_set::NumDeltaPocs[65];

void fileSourceHEVCAnnexBFile::st_ref_pic_set::parse_st_ref_pic_set(sub_byte_reader &reader, int stRpsIdx, sps *actSPS, TreeItem *root)
{
  // Create a new TreeItem root for the item
  // The macros will use this variable to add all the parsed variables
  TreeItem *const itemTree = root ? new TreeItem("st_ref_pic_set()", root) : nullptr;

  if (stRpsIdx > 64)
    throw std::logic_error("Error while parsing short term ref pic set. The stRpsIdx must be in the range [0..64].");

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

    for(int j=0; j<=NumDeltaPocs[RefRpsIdx]; j++)
    {
      READFLAG_A(used_by_curr_pic_flag, j);
      use_delta_flag.append(true); // Infer to 1
      if(!used_by_curr_pic_flag.last())
        READFLAG(use_delta_flag[j])
    }

    // Derive NumNegativePics Rec. ITU-T H.265 v3 (04/2015) (7-59)
    int i = 0;
    for(int j=NumPositivePics[RefRpsIdx] - 1; j >= 0; j--)
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
    for(int j=0; j<NumNegativePics[RefRpsIdx]; j++)
    { 
      int dPoc = DeltaPocS0[RefRpsIdx][j] + deltaRps;
      if( dPoc < 0 && use_delta_flag[j] ) 
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
    for( int j=NumNegativePics[RefRpsIdx] - 1; j>=0; j-- ) 
    { 
      int dPoc = DeltaPocS0[RefRpsIdx][j] + deltaRps;
      if( dPoc > 0 && use_delta_flag[j] ) 
      { 
        DeltaPocS1[stRpsIdx][i] = dPoc;
        LOGSTRVAL(QString("DeltaPocS1[%1][%2]").arg(stRpsIdx).arg(i), dPoc);
        UsedByCurrPicS1[stRpsIdx][i++] = used_by_curr_pic_flag[j];
      }
    }
    if( deltaRps > 0 && use_delta_flag[NumDeltaPocs[RefRpsIdx]] ) 
    {
      DeltaPocS1[stRpsIdx][i] = deltaRps;
      LOGSTRVAL(QString("DeltaPocS1[%1][%2]").arg(stRpsIdx).arg(i), deltaRps);
      UsedByCurrPicS1[stRpsIdx][i++] = used_by_curr_pic_flag[NumDeltaPocs[RefRpsIdx]];
    }
    for( int j=0; j<NumPositivePics[RefRpsIdx]; j++) 
    { 
      int dPoc = DeltaPocS1[RefRpsIdx][j] + deltaRps;
      if( dPoc > 0 && use_delta_flag[NumNegativePics[RefRpsIdx] + j] ) 
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
    for(int i = 0; i < num_negative_pics; i++)
    {
      READUEV_A(delta_poc_s0_minus1, i);
      READFLAG_A(used_by_curr_pic_s0_flag, i);
      
      if (i==0)
        DeltaPocS0[stRpsIdx][i] = -(delta_poc_s0_minus1.last() + 1); // (7-65)
      else
        DeltaPocS0[stRpsIdx][i] = DeltaPocS0[stRpsIdx][i-1] - (delta_poc_s0_minus1.last() + 1); // (7-67)
      LOGSTRVAL(QString("DeltaPocS0[%1][%2]").arg(stRpsIdx).arg(i), DeltaPocS0[stRpsIdx][i]);
    }
    for(int i = 0; i < num_positive_pics; i++)
    {
      READUEV_A(delta_poc_s1_minus1, i);
      READFLAG_A(used_by_curr_pic_s1_flag, i);

      if (i==0)
        DeltaPocS1[stRpsIdx][i] = delta_poc_s1_minus1.last() + 1; // (7-66)
      else
        DeltaPocS1[stRpsIdx][i] = DeltaPocS1[stRpsIdx][i-1] + (delta_poc_s1_minus1.last() + 1); // (7-68)
      LOGSTRVAL(QString("DeltaPocS1[%1][%2]").arg(stRpsIdx).arg(i), DeltaPocS1[stRpsIdx][i]);
    }

    NumNegativePics[stRpsIdx] = num_negative_pics;
    NumPositivePics[stRpsIdx] = num_positive_pics;
    LOGSTRVAL(QString("NumNegativePics[%1]").arg(stRpsIdx), num_negative_pics);
    LOGSTRVAL(QString("NumPositivePics[%1]").arg(stRpsIdx), num_positive_pics);
  }

  NumDeltaPocs[stRpsIdx] = NumNegativePics[stRpsIdx] + NumPositivePics[stRpsIdx]; // (7-69)
}

// (7-55)
int fileSourceHEVCAnnexBFile::st_ref_pic_set::NumPicTotalCurr(int CurrRpsIdx, slice *actSlice)
{
  int NumPicTotalCurr = 0;
  for(int i = 0; i < NumNegativePics[CurrRpsIdx]; i++)
    if(UsedByCurrPicS0[CurrRpsIdx][i])
      NumPicTotalCurr++ ;
  for(int i = 0; i < NumPositivePics[CurrRpsIdx]; i++)  
    if(UsedByCurrPicS1[CurrRpsIdx][i]) 
      NumPicTotalCurr++;
  for(int i = 0; i < actSlice->num_long_term_sps + actSlice->num_long_term_pics; i++) 
    if(actSlice->UsedByCurrPicLt[i])
      NumPicTotalCurr++;
  return NumPicTotalCurr;
}

void fileSourceHEVCAnnexBFile::vui_parameters::parse_vui_parameters(sub_byte_reader &reader, sps *actSPS, TreeItem *root)
{
  // Create a new TreeItem root for the item
  // The macros will use this variable to add all the parsed variables
  TreeItem *const itemTree = root ? new TreeItem("vui_parameters()", root) : nullptr;

  READFLAG(aspect_ratio_info_present_flag);
  if(aspect_ratio_info_present_flag)
  {
    READBITS(aspect_ratio_idc, 8);
    if( aspect_ratio_idc == 255 ) // EXTENDED_SAR=255
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
    READBITS(video_format, 3);
    READFLAG(video_full_range_flag);

    READFLAG(colour_description_present_flag);
    if(colour_description_present_flag)
    {
      READBITS(colour_primaries, 8);
      READBITS(transfer_characteristics, 8);
      READBITS(matrix_coeffs, 8);
    }
  }

  READFLAG(chroma_loc_info_present_flag);
  if( chroma_loc_info_present_flag ) 
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
      vui_hrd_parameters.parse_hrd_parameters(reader, 1, actSPS->sps_max_sub_layers_minus1, itemTree);
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
}

void fileSourceHEVCAnnexBFile::scaling_list_data::parse_scaling_list_data(sub_byte_reader &reader, TreeItem *root)
{
  // Create a new TreeItem root for the item
  // The macros will use this variable to add all the parsed variables
  TreeItem *const itemTree = root ? new TreeItem("scaling_list_data()", root) : nullptr;

  for(int sizeId=0; sizeId<4; sizeId++)
  {
    for(int matrixId=0; matrixId<6; matrixId += (sizeId == 3) ? 3 : 1) 
    { 
      READFLAG(scaling_list_pred_mode_flag[sizeId][matrixId]);
      if(!scaling_list_pred_mode_flag[sizeId][matrixId])
        READFLAG(scaling_list_pred_matrix_id_delta[sizeId][matrixId])
      else
      {
        int nextCoef = 8;
        int coefNum = std::min(64, (1 << (4 + (sizeId << 1))));
        if( sizeId > 1 )
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
}

void fileSourceHEVCAnnexBFile::vps::parse_vps(const QByteArray &parameterSetData, TreeItem *root)
{
  parameter_set_data = parameterSetData;
  
  sub_byte_reader reader(parameter_set_data);

  // Create a new TreeItem root for the item
  // The macros will use this variable to add all the parsed variables
  TreeItem *const itemTree = root ? new TreeItem("video_parameter_set_rbsp()", root) : nullptr;
  
  READBITS(vps_video_parameter_set_id, 4);
  READFLAG(vps_base_layer_internal_flag);
  READFLAG(vps_base_layer_available_flag);
  READBITS(vps_max_layers_minus1, 6);
  READBITS(vps_max_sub_layers_minus1, 3);
  READFLAG(vps_temporal_id_nesting_flag);
  READBITS(vps_reserved_0xffff_16bits, 16);

  // Parse the profile tier level
  ptl.parse_profile_tier_level(reader, true, vps_max_sub_layers_minus1, itemTree);
    
  READFLAG(vps_sub_layer_ordering_info_present_flag);
  for( int i = (vps_sub_layer_ordering_info_present_flag ? 0 : vps_max_sub_layers_minus1); i <= vps_max_sub_layers_minus1; i++) 
  {
    READUEV(vps_max_dec_pic_buffering_minus1[i]);
    READUEV(vps_max_num_reorder_pics[i]);
    READUEV(vps_max_latency_increase_plus1[i]);
  }

  READBITS(vps_max_layer_id, 6); // 0...63
  READUEV(vps_num_layer_sets_minus1); // 0 .. 1023

  for( int i=1; i <= vps_num_layer_sets_minus1; i++)
    for( int j=0; j <= vps_max_layer_id; j++)
      READFLAG_A(layer_id_included_flag[i], i);

  READFLAG(vps_timing_info_present_flag);
  if( vps_timing_info_present_flag )
  {
    // The VPS can provide timing info (the time between two POCs. So the refresh rate)
    READBITS(vps_num_units_in_tick,32);
    READBITS(vps_time_scale,32);
    READFLAG(vps_poc_proportional_to_timing_flag);
    if( vps_poc_proportional_to_timing_flag )
      READUEV(vps_num_ticks_poc_diff_one_minus1);

    // HRD parameters ...
    READUEV(vps_num_hrd_parameters);
    for(int i=0; i < vps_num_hrd_parameters; i++)
    {
      READUEV_A(hrd_layer_set_idx, i);

      if(i > 0)
        READFLAG_A(cprms_present_flag, i);
      
      // hrd_parameters...
      vps_hrd_parameters.append(hrd_parameters());
      vps_hrd_parameters.last().parse_hrd_parameters(reader, cprms_present_flag[i], vps_max_sub_layers_minus1, itemTree);
    }

    frameRate = (double)vps_time_scale / (double)vps_num_units_in_tick;
    LOGVAL(frameRate);
  }

  READFLAG(vps_extension_flag);
  if (vps_extension_flag)
    if (itemTree)
      new TreeItem("vps_extension() - not implemented yet...", itemTree);

  // Here comes the VPS extension.
  // This is specified in the annex F, multilayer and stuff.
  // This could be added and is definitely interesting.
  // ... later
}

fileSourceHEVCAnnexBFile::sps::sps(const nal_unit &nal) : parameter_set_nal(nal)
{
  // Infer some default values (if not present)
  separate_colour_plane_flag = false;
  conf_win_left_offset = 0;
  conf_win_right_offset = 0;
  conf_win_top_offset = 0;
  conf_win_bottom_offset = 0;
  num_long_term_ref_pics_sps = 0;
  sps_range_extension_flag = false;
  sps_multilayer_extension_flag = false;
  sps_3d_extension_flag = false;
  sps_extension_5bits = 0;
}

void fileSourceHEVCAnnexBFile::sps::parse_sps(const QByteArray &parameterSetData, TreeItem *root)
{
  parameter_set_data = parameterSetData;
  
  sub_byte_reader reader(parameter_set_data);

  // Create a new TreeItem root for the item
  // The macros will use this variable to add all the parsed variables
  TreeItem *const itemTree = root ? new TreeItem("seq_parameter_set_rbsp()", root) : nullptr;

  READBITS(sps_video_parameter_set_id,4);
  READBITS(sps_max_sub_layers_minus1, 3);
  READFLAG(sps_temporal_id_nesting_flag);
  
  // parse profile tier level
  ptl.parse_profile_tier_level(reader, true, sps_max_sub_layers_minus1, itemTree);
  
  /// Back to the seq_parameter_set_rbsp
  READUEV(sps_seq_parameter_set_id);
  READUEV(chroma_format_idc);
  if (chroma_format_idc == 3) 
    READBITS(separate_colour_plane_flag,1)
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
  for(int i = (sps_sub_layer_ordering_info_present_flag ? 0 : sps_max_sub_layers_minus1); i <= sps_max_sub_layers_minus1; i++)
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
      sps_scaling_list_data.parse_scaling_list_data(reader, itemTree);
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
  for(int i=0; i<num_short_term_ref_pic_sets; i++)
  {
    sps_st_ref_pic_sets.append(st_ref_pic_set());
    sps_st_ref_pic_sets.last().parse_st_ref_pic_set(reader, i, this, itemTree);
  }

  READFLAG(long_term_ref_pics_present_flag);
  if(long_term_ref_pics_present_flag)
  {
    READUEV(num_long_term_ref_pics_sps);
    for(int i=0; i<num_long_term_ref_pics_sps; i++)
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
    sps_vui_parameters.parse_vui_parameters(reader, this, itemTree);

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
    if (itemTree)
      new TreeItem("sps_range_extension() - not implemented yet...", itemTree);

  if(sps_multilayer_extension_flag)
    if (itemTree)
      new TreeItem("sps_multilayer_extension() - not implemented yet...", itemTree);

  if(sps_3d_extension_flag)
    if (itemTree)
      new TreeItem("sps_3d_extension() - not implemented yet...", itemTree);

  if (sps_extension_5bits != 0)
    if (itemTree)
      new TreeItem("sps_extension_data_flag - not implemented yet...", itemTree);

  // Calculate some values - Rec. ITU-T H.265 v3 (04/2015) 7.4.3.2.1
  MinCbLog2SizeY = log2_min_luma_coding_block_size_minus3 + 3;              // (7-10)
  CtbLog2SizeY = MinCbLog2SizeY + log2_diff_max_min_luma_coding_block_size; // (7-11)
  CtbSizeY = 1 << CtbLog2SizeY;                                             // (7-13)
  PicWidthInCtbsY = ceil( pic_width_in_luma_samples / CtbSizeY );           // (7-15)
  PicHeightInCtbsY = ceil( pic_height_in_luma_samples / CtbSizeY );         // (7-17)
  PicSizeInCtbsY = PicWidthInCtbsY * PicHeightInCtbsY;                      // (7-19)

  LOGVAL(MinCbLog2SizeY);
  LOGVAL(CtbLog2SizeY);
  LOGVAL(CtbSizeY);
  LOGVAL(PicWidthInCtbsY);
  LOGVAL(PicHeightInCtbsY);
  LOGVAL(PicSizeInCtbsY);
}

fileSourceHEVCAnnexBFile::pps::pps(const nal_unit &nal) : parameter_set_nal(nal)
{
  deblocking_filter_override_enabled_flag = false;
  pps_range_extension_flag = false;
  pps_multilayer_extension_flag = false;
  pps_3d_extension_flag = false;
  pps_extension_5bits = false;
}

void fileSourceHEVCAnnexBFile::pps::parse_pps(const QByteArray &parameterSetData, TreeItem *root)
{
  parameter_set_data = parameterSetData;
  
  sub_byte_reader reader(parameter_set_data);

  // Create a new TreeItem root for the item
  // The macros will use this variable to add all the parsed variables
  TreeItem *const itemTree = root ? new TreeItem("pic_parameter_set_rbsp()", root) : nullptr;

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
      for(int i = 0; i < num_tile_columns_minus1; i++)
        READUEV_A(column_width_minus1, i);
      for(int i = 0; i < num_tile_rows_minus1; i++)
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
    pps_scaling_list_data.parse_scaling_list_data(reader, itemTree);
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
  // This would also be interesting but later.
  if(pps_range_extension_flag)
    range_extension.parse_pps_range_extension(reader, this, itemTree);

  if(pps_multilayer_extension_flag)
    if (itemTree)
      new TreeItem("pps_multilayer_extension() - not implemented yet...", itemTree);

  if(pps_3d_extension_flag)
    if (itemTree)
      new TreeItem("pps_3d_extension() - not implemented yet...", itemTree);

  if (pps_extension_5bits != 0)
    if (itemTree)
      new TreeItem("pps_extension_data_flag - not implemented yet...", itemTree);

  // There is more to parse but we are not interested in this information (for now)
}

fileSourceHEVCAnnexBFile::pps_range_extension::pps_range_extension()
{
  chroma_qp_offset_list_enabled_flag = false;
}

void fileSourceHEVCAnnexBFile::pps_range_extension::parse_pps_range_extension(sub_byte_reader &reader, pps *actPPS, TreeItem *root)
{
  // Create a new TreeItem root for the item
  // The macros will use this variable to add all the parsed variables
  TreeItem *const itemTree = root ? new TreeItem("pps_range_extension()", root) : nullptr;

  if(actPPS->transform_skip_enabled_flag)
    READUEV(log2_max_transform_skip_block_size_minus2);
  READFLAG(cross_component_prediction_enabled_flag);
  READFLAG(chroma_qp_offset_list_enabled_flag);
  if (chroma_qp_offset_list_enabled_flag)
  {
    READUEV(diff_cu_chroma_qp_offset_depth);
    READUEV(chroma_qp_offset_list_len_minus1);
    for(int i = 0; i <= chroma_qp_offset_list_len_minus1; i++) 
    {
      READSEV_A(cb_qp_offset_list, i);
      READSEV_A(cr_qp_offset_list, i);
    }
  }
  READUEV(log2_sao_offset_scale_luma);
  READUEV(log2_sao_offset_scale_chroma);
}

void fileSourceHEVCAnnexBFile::ref_pic_lists_modification::parse_ref_pic_lists_modification(sub_byte_reader &reader, slice *actSlice, int NumPicTotalCurr, TreeItem *root)
{
  // Create a new TreeItem root for the item
  // The macros will use this variable to add all the parsed variables
  TreeItem *const itemTree = root ? new TreeItem("ref_pic_lists_modification()", root) : nullptr;

  int nrBits = ceil(log2(NumPicTotalCurr));

  READFLAG(ref_pic_list_modification_flag_l0);
  if (ref_pic_list_modification_flag_l0)
    for(int i = 0; i <= actSlice->num_ref_idx_l0_active_minus1; i++)
      READBITS_A(list_entry_l0, nrBits, i);

  if(actSlice->slice_type == 0) // B
  {
    READFLAG(ref_pic_list_modification_flag_l1);
    if (ref_pic_list_modification_flag_l1)
      for(int i = 0; i <= actSlice->num_ref_idx_l1_active_minus1; i++)
        READBITS_A(list_entry_l1, nrBits, i);
  }
}

// Initialize static member. Only true for the first slice instance
bool fileSourceHEVCAnnexBFile::slice::bFirstAUInDecodingOrder = true;
int fileSourceHEVCAnnexBFile::slice::prevTid0Pic_slice_pic_order_cnt_lsb = 0;
int fileSourceHEVCAnnexBFile::slice::prevTid0Pic_PicOrderCntMsb = 0;

fileSourceHEVCAnnexBFile::slice::slice(const nal_unit &nal) : nal_unit(nal)
{
  PicOrderCntVal = -1;
  PicOrderCntMsb = -1;

  // When not present, the value of dependent_slice_segment_flag is inferred to be equal to 0.
  dependent_slice_segment_flag = false;
  short_term_ref_pic_set_sps_flag = false;
  num_long_term_sps = 0;
  num_long_term_pics = 0;
  deblocking_filter_override_flag = false;
  slice_temporal_mvp_enabled_flag = false;
  slice_sao_luma_flag = false;
  slice_sao_chroma_flag = false;
}

// T-REC-H.265-201410 - 7.3.6.1 slice_segment_header()
void fileSourceHEVCAnnexBFile::slice::parse_slice(const QByteArray &sliceHeaderData,
                        const QMap<int, sps*> &p_active_SPS_list,
                        const QMap<int, pps*> &p_active_PPS_list,
                        slice *firstSliceInSegment,
                        TreeItem *root)
{
  sub_byte_reader reader(sliceHeaderData);

  // Create a new TreeItem root for the item
  // The macros will use this variable to add all the parsed variables
  TreeItem *const itemTree = root ? new TreeItem("slice_segment_header()", root) : nullptr;

  READFLAG(first_slice_segment_in_pic_flag);
  
  if (isIRAP())
    READFLAG(no_output_of_prior_pics_flag);

  READUEV(slice_pic_parameter_set_id);
  // The value of slice_pic_parameter_set_id shall be in the range of 0 to 63, inclusive. (Max 11 bits read)
  if (slice_pic_parameter_set_id >= 63) 
    throw std::out_of_range("The variable slice_pic_parameter_set_id is out of range.");

  // Get the active PPS
  if (!p_active_PPS_list.contains(slice_pic_parameter_set_id))
    throw std::logic_error("The signaled PPS was not found in the bitstream.");
  actPPS = p_active_PPS_list.value(slice_pic_parameter_set_id);

  // Get the active SPS
  if (!p_active_SPS_list.contains(actPPS->pps_seq_parameter_set_id))
    throw std::logic_error("The signaled SPS was not found in the bitstream.");
  actSPS = p_active_SPS_list.value(actPPS->pps_seq_parameter_set_id);

  if (!first_slice_segment_in_pic_flag)
  {
    if (actPPS->dependent_slice_segments_enabled_flag)
      READFLAG(dependent_slice_segment_flag);
    int nrBits = ceil(log2(actSPS->PicSizeInCtbsY));  // 7.4.7.1
    READBITS(slice_segment_address, nrBits);
  }

  if (!dependent_slice_segment_flag)
  {
    for (int i=0; i < actPPS->num_extra_slice_header_bits; i++)
      READFLAG_A(slice_reserved_flag, i);

    READUEV(slice_type); // Max 3 bits read. 0-B 1-P 2-I
    if (actPPS->output_flag_present_flag) 
      READFLAG(pic_output_flag);

    if (actSPS->separate_colour_plane_flag) 
      READFLAG(colour_plane_id);

    if(nal_type != IDR_W_RADL && nal_type != IDR_N_LP)
    {
      READBITS(slice_pic_order_cnt_lsb, actSPS->log2_max_pic_order_cnt_lsb_minus4 + 4); // Max 16 bits read
      READFLAG(short_term_ref_pic_set_sps_flag);

      //Decoding of the reference picture sets works differently. 
      //This has to be re-thought ...

      if(!short_term_ref_pic_set_sps_flag)
        st_rps.parse_st_ref_pic_set(reader, actSPS->num_short_term_ref_pic_sets, actSPS, itemTree);
      else if(actSPS->num_short_term_ref_pic_sets > 1)
      {
        int nrBits = ceil(log2(actSPS->num_short_term_ref_pic_sets));
        READBITS(short_term_ref_pic_set_idx, nrBits);

        // The short term ref pic set is the one with the given index from the SPS
        if (short_term_ref_pic_set_idx >= actSPS->sps_st_ref_pic_sets.length())
          throw std::logic_error("Error parsing slice header. The specified short term ref pic list could not be found in the SPS.");
        st_rps = actSPS->sps_st_ref_pic_sets[short_term_ref_pic_set_idx];

      }
      if(actSPS->long_term_ref_pics_present_flag)
      {
        if(actSPS->num_long_term_ref_pics_sps > 0)
          READUEV(num_long_term_sps);
        READUEV(num_long_term_pics);
        for(int i = 0; i < num_long_term_sps + num_long_term_pics; i++)
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

          READFLAG_A(delta_poc_msb_present_flag, i)
          if(delta_poc_msb_present_flag.last())
            READUEV_A(delta_poc_msb_cycle_lt, i)
        }
      }
      if(actSPS->sps_temporal_mvp_enabled_flag)
        READFLAG(slice_temporal_mvp_enabled_flag)
    }
    else 
    {
      slice_pic_order_cnt_lsb = 0;
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
        slice_rpl_mod.parse_ref_pic_lists_modification(reader, this, NumPicTotalCurr, itemTree);

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
        slice_pred_weight_table.parse_pred_weight_table(reader, actSPS, this, itemTree);

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

    if(actPPS->pps_loop_filter_across_slices_enabled_flag && (slice_sao_luma_flag || slice_sao_chroma_flag || !slice_deblocking_filter_disabled_flag))
      READFLAG(slice_loop_filter_across_slices_enabled_flag);
  } 
  else // dependent_slice_segment_flag is true -- inferr values from firstSliceInSegment
  {
    if (firstSliceInSegment == nullptr)
      throw std::logic_error("Dependent slice without a first slice in the segment.");

    slice_pic_order_cnt_lsb = firstSliceInSegment->slice_pic_order_cnt_lsb;
  }
  
  if(actPPS->tiles_enabled_flag || actPPS->entropy_coding_sync_enabled_flag)
  {
    READUEV(num_entry_point_offsets);
    if (num_entry_point_offsets > 0)
    {
      READUEV(offset_len_minus1);
      if (offset_len_minus1 > 31)
        throw std::logic_error("offset_len_minus1 shall be 0..31");

      for(int i = 0; i < num_entry_point_offsets; i++)
      {
        int nrBits = offset_len_minus1 + 1;
        READBITS_A(entry_point_offset_minus1, nrBits, i);
      }
    }
  }

  if(actPPS->slice_segment_header_extension_present_flag)
  {
    READUEV(slice_segment_header_extension_length);
    for(int i = 0; i < slice_segment_header_extension_length; i++)
      READBITS_A(slice_segment_header_extension_data_byte, 8, i);
  }

  // End of the slice header - byte_alignment()

  // Calculate the picture order count
  int MaxPicOrderCntLsb = 1 << (actSPS->log2_max_pic_order_cnt_lsb_minus4 + 4);
  LOGVAL(MaxPicOrderCntLsb);

  // The variable NoRaslOutputFlag is specified as follows:
  bool NoRaslOutputFlag = false;
  if (nal_type == IDR_W_RADL || nal_type == BLA_W_LP)
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
    if((slice_pic_order_cnt_lsb < prevPicOrderCntLsb) && ((prevPicOrderCntLsb - slice_pic_order_cnt_lsb) >= (MaxPicOrderCntLsb / 2)))
      PicOrderCntMsb = prevPicOrderCntMsb + MaxPicOrderCntLsb;
    else if ((slice_pic_order_cnt_lsb > prevPicOrderCntLsb ) && ((slice_pic_order_cnt_lsb - prevPicOrderCntLsb) > (MaxPicOrderCntLsb / 2))) 
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
}

const QStringList fileSourceHEVCAnnexBFile::nal_unit_type_toString = QStringList()
   << "TRAIL_N" << "TRAIL_R" << "TSA_N" << "TSA_R" << "STSA_N" << "STSA_R" << "RADL_N" << "RADL_R" << "RASL_N" << "RASL_R" << "RSV_VCL_N10" << "RSV_VCL_N12" << "RSV_VCL_N14" << 
      "RSV_VCL_R11" << "RSV_VCL_R13" << "RSV_VCL_R15" << "BLA_W_LP" << "BLA_W_RADL" << "BLA_N_LP" << "IDR_W_RADL" <<
      "IDR_N_LP" << "CRA_NUT" << "RSV_IRAP_VCL22" << "RSV_IRAP_VCL23" << "RSV_VCL24" << "RSV_VCL25" << "RSV_VCL26" << "RSV_VCL27" << "RSV_VCL28" << "RSV_VCL29" <<
      "RSV_VCL30" << "RSV_VCL31" << "VPS_NUT" << "SPS_NUT" << "PPS_NUT" << "AUD_NUT" << "EOS_NUT" << "EOB_NUT" << "FD_NUT" << "PREFIX_SEI_NUT" <<
      "SUFFIX_SEI_NUT" << "RSV_NVCL41" << "RSV_NVCL42" << "RSV_NVCL43" << "RSV_NVCL44" << "RSV_NVCL45" << "RSV_NVCL46" << "RSV_NVCL47" << "UNSPECIFIED";

fileSourceHEVCAnnexBFile::fileSourceHEVCAnnexBFile()
{
  fileBuffer.resize(BUFFER_SIZE);
  posInBuffer = 0;
  bufferStartPosInFile = 0;
  numZeroBytes = 0;

  // Set the start code to look for (0x00 0x00 0x01)
  startCode.append((char)0);
  startCode.append((char)0);
  startCode.append((char)1);
}

fileSourceHEVCAnnexBFile::~fileSourceHEVCAnnexBFile()
{
  if (!nalUnitListCopied)
    // We created all the instances in the nalUnitList. Delete them all again.
    qDeleteAll(nalUnitList);
  nalUnitList.clear();
}

// Open the file and fill the read buffer. 
// Then scan the file for NAL units and save the start of every NAL unit in the file.
// If full parsing is enabled, all parameter set data will be fully parsed and saved in the tree structure
// so that it can be used by the QAbstractItemModel.
bool fileSourceHEVCAnnexBFile::openFile(const QString &fileName, bool saveAllUnits, fileSourceHEVCAnnexBFile *otherFile)
{
  DEBUG_ANNEXB("fileSourceHEVCAnnexBFile::openFile fileName %s %s", fileName, saveAllUnits ? "saveAllUnits" : "");

  if (srcFile.isOpen())
  {
    // A file was already open. We are re-opening the file.
    
    // Reset the default values
    fileBuffer = QByteArray();
    fileBuffer.resize(BUFFER_SIZE);
    fileBufferSize = -1;
    posInBuffer = 0;
    bufferStartPosInFile = 0;
    numZeroBytes = 0;

    // Clear out knowloedget of the bitstream
    qDeleteAll(nalUnitList);
    nalUnitList.clear();
    POC_List.clear();
  }

  // Open the input file (again)
  fileSource::openFile(fileName);

  // Fill the buffer
  fileBufferSize = srcFile.read(fileBuffer.data(), BUFFER_SIZE);
  if (fileBufferSize == 0)
    // The file is empty of there was an error reading from the file.
    return false;
    
  // Get the positions where we can start decoding
  nalUnitListCopied = (otherFile != nullptr);
  if (otherFile)
  {
    // Copy the nalUnitList and POC_List from the other file
    POC_List = otherFile->POC_List;
    nalUnitList = otherFile->nalUnitList;
    return true;
  }
  else
    return scanFileForNalUnits(saveAllUnits);
}

bool fileSourceHEVCAnnexBFile::updateBuffer()
{
  // Save the position of the first byte in this new buffer
  bufferStartPosInFile += fileBufferSize;

  fileBufferSize = srcFile.read(fileBuffer.data(), BUFFER_SIZE);
  posInBuffer = 0;

  DEBUG_ANNEXB("fileSourceHEVCAnnexBFile::updateBuffer fileBufferSize %d", fileBufferSize);
  return (fileBufferSize > 0);
}

bool fileSourceHEVCAnnexBFile::seekToNextNALUnit()
{
  // Are we currently at the one byte of a start code?
  if (curPosAtStartCode())
    return gotoNextByte();

  numZeroBytes = 0;
  
  // Check if there is another start code in the buffer
  int idx = fileBuffer.indexOf(startCode, posInBuffer);
  while (idx < 0) 
  {
    // Start code not found in this buffer. Load next chuck of data from file.

    // Before we load more data, check with how many zeroes the current buffer ends.
    // This could be the beginning of a start code.
    int nrZeros = 0;
    for (int i = 1; i <= 3; i++) 
    {
      if (fileBuffer.at(fileBufferSize-i) == 0)
        nrZeros++;
    }
    
    // Load the next buffer
    if (!updateBuffer()) 
    {
      // Out of file
      return false;
    }

    if (nrZeros > 0)
    {
      // The last buffer ended with zeroes. 
      // Now check if the beginning of this buffer is the remaining part of a start code
      if ((nrZeros == 2 || nrZeros == 3) && fileBuffer.at(0) == 1) 
      {
        // Start code found
        posInBuffer = 1;
        return true;
      }

      if ((nrZeros == 1) && fileBuffer.at(0) == 0 && fileBuffer.at(1) == 1) 
      {
        // Start code found
        posInBuffer = 2;
        return true;
      }
    }

    // New buffer loaded but no start code found yet. Search for it again.
    idx = fileBuffer.indexOf(startCode, posInBuffer);
  }

  assert(idx >= 0);
  if (quint64(idx + 3) >= fileBufferSize) 
  {
    // The start code is exactly at the end of the current buffer. 
    if (!updateBuffer()) 
      // Out of file
      return false;
    return true;
  }

  // Update buffer position
  posInBuffer = idx + 3;
  return true;
}

bool fileSourceHEVCAnnexBFile::gotoNextByte()
{
  // First check if the current byte is a zero byte
  if (getCurByte() == (char)0)
    numZeroBytes++;
  else
    numZeroBytes = 0;

  posInBuffer++;

  if (posInBuffer >= fileBufferSize) 
  {
    // The next byte is in the next buffer
    if (!updateBuffer())
    {
      // Out of file
      return false;
    }
    posInBuffer = 0;
  }

  return true;
}

bool fileSourceHEVCAnnexBFile::scanFileForNalUnits(bool saveAllUnits)
{
  DEBUG_ANNEXB("fileSourceHEVCAnnexBFile::scanFileForNalUnits %s", saveAllUnits ? "saveAllUnits" : "");

  // Show a modal QProgressDialog while this operation is running.
  // If the user presses cancel, we will cancel and return false (opening the file failed).
  // First, get a pointer to the main window to use as a parent for the modal parsing progress dialog.
  QWidgetList l = QApplication::topLevelWidgets();
  QWidget *mainWindow = nullptr;
  for (QWidget *w : l)
  {
    MainWindow *mw = dynamic_cast<MainWindow*>(w);
    if (mw)
      mainWindow = mw;
  }
  // Create the dialog
  qint64 maxPos = getFileSize();
  // Updating the dialog (setValue) is quite slow. Only do this if the percent value changes.
  int curPercentValue = 0;
  QProgressDialog progress("Parsing AnnexB bitstream...", "Cancel", 0, 100, mainWindow);
  progress.setMinimumDuration(1000);  // Show after 1s
  progress.setAutoClose(false);
  progress.setAutoReset(false);
  progress.setWindowModality(Qt::WindowModal);

  // These maps hold the last active VPS, SPS and PPS. This is required for parsing
  // the parameter sets.
  QMap<int, sps*> active_SPS_list;
  QMap<int, pps*> active_PPS_list;
  // We keept a pointer to the last slice with first_slice_segment_in_pic_flag set. 
  // All following slices with dependent_slice_segment_flag set need this slice to infer some values.
  slice *lastFirstSliceSegmentInPic = nullptr;

  // Count the NALs
  int nalID = 0;

  if (saveAllUnits && nalUnitModel.rootItem.isNull())
    // Create a new root for the nal unit tree of the QAbstractItemModel
    nalUnitModel.rootItem.reset(new TreeItem(QStringList() << "Name" << "Value" << "Coding" << "Code", nullptr));

  while (seekToNextNALUnit()) 
  {
    try
    {
      // Seek successfull. The file is now pointing to the first byte after the start code.

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
      nal_unit nal(curFilePos);
      nal.parse_nal_unit_header(nalHeaderBytes, nalRoot);

      if (nal.nal_type == VPS_NUT) 
      {
        // A video parameter set
        vps *new_vps = new vps(nal);
        new_vps->parse_vps(getRemainingNALBytes(), nalRoot);

        // Put parameter sets into the NAL unit list
        nalUnitList.append(new_vps);

        // Add the VPS ID
        specificDescription = QString(" VPS_NUT ID %1").arg(new_vps->vps_video_parameter_set_id);
      }
      else if (nal.nal_type == SPS_NUT) 
      {
        // A sequence parameter set
        sps *new_sps = new sps(nal);
        new_sps->parse_sps(getRemainingNALBytes(), nalRoot);
      
        // Add sps (replace old one if existed)
        active_SPS_list.insert(new_sps->sps_seq_parameter_set_id, new_sps);

        // Also add sps to list of all nals
        nalUnitList.append(new_sps);

        // Add the SPS ID
        specificDescription = QString(" SPS_NUT ID %1").arg(new_sps->sps_seq_parameter_set_id);
      }
      else if (nal.nal_type == PPS_NUT) 
      {
        // A picture parameter set
        pps *new_pps = new pps(nal);
        new_pps->parse_pps(getRemainingNALBytes(), nalRoot);
      
        // Add pps (replace old one if existed)
        active_PPS_list.insert(new_pps->pps_pic_parameter_set_id, new_pps);

        // Also add pps to list of all nals
        nalUnitList.append(new_pps);

        // Add the PPS ID
        specificDescription = QString(" PPS_NUT ID %1").arg(new_pps->pps_pic_parameter_set_id);
      }
      else if (nal.isSlice())
      {
        // Create a new slice unit
        slice *newSlice = new slice(nal);
        newSlice->parse_slice(getRemainingNALBytes(), active_SPS_list, active_PPS_list, lastFirstSliceSegmentInPic, nalRoot);

        // Add the POC of the slice
        specificDescription = QString(" POC %1").arg(newSlice->PicOrderCntVal);

        if (newSlice->first_slice_segment_in_pic_flag)
          lastFirstSliceSegmentInPic = newSlice;

        // Get the poc and add it to the POC list
        if (newSlice->PicOrderCntVal >= 0)
          addPOCToList(newSlice->PicOrderCntVal);

        if (nal.isIRAP())
        {
          if (newSlice->first_slice_segment_in_pic_flag)
            // This is the first slice of a random access pont. Add it to the list.
            nalUnitList.append(newSlice);
          else
            delete newSlice;
        }
      }
      else if (nal.nal_type == PREFIX_SEI_NUT || nal.nal_type == SUFFIX_SEI_NUT)
      {
        // An SEI message
        sei *new_sei = new sei(nal);
        new_sei->parse_sei_message(getRemainingNALBytes(), nalRoot);

        specificDescription = QString(" payloadType %1").arg(new_sei->payloadType);

        // We don't use the SEI message
        delete new_sei;
      }

      if (nalRoot)
        // Set a useful name of the TreeItem (the root for this NAL)
        nalRoot->itemData.append(QString("NAL %1: %2").arg(nalID).arg(nal_unit_type_toString.value(nal.nal_type)) + specificDescription);

      nalID++;

      // Update the progress dialog
      if (progress.wasCanceled())
      {
        POC_List.clear();
        qDeleteAll(nalUnitList);
        nalUnitList.clear();
        return false;
      }
      int newPercentValue = pos() * 100 / maxPos;
      if (newPercentValue != curPercentValue)
      {
        progress.setValue(newPercentValue);
        curPercentValue = newPercentValue;
      }
    }
    catch (...)
    {
      // Reading a NAL unit failed at some point.
      // This is not too bad. Just don't use this NAL unit and continue with the next one.
    }
  }

  // We are done.
  progress.close();

  // Finally sort the POC list
  std::sort(POC_List.begin(), POC_List.end());
  
  return true;
}

QByteArray fileSourceHEVCAnnexBFile::getRemainingNALBytes(int maxBytes)
{
  QByteArray retArray;
  int nrBytesRead = 0;

  while (!curPosAtStartCode() && (maxBytes == -1 || nrBytesRead < maxBytes)) 
  {
    // Save byte and goto the next one
    retArray.append( getCurByte() );

    if (!gotoNextByte())
      // No more bytes. Return all we got.
      return retArray;

    nrBytesRead++;
  }

  // We should now be at a header byte. Remove the zeroes from the start code that we put into retArray
  while (retArray.endsWith(char(0))) 
    // Chop off last zero byte
    retArray.chop(1);

  return retArray;
}

bool fileSourceHEVCAnnexBFile::addPOCToList(int poc)
{
  if (poc < 0)
    return false;

  if (POC_List.contains(poc))
    // Two pictures with the same POC are not allowed
    return false;
  
  POC_List.append(poc);
  return true;
}

// Look through the random access points and find the closest one before (or equal)
// the given frameIdx where we can start decoding
int fileSourceHEVCAnnexBFile::getClosestSeekableFrameNumber(int frameIdx) const
{
  // Get the POC for the frame number
  int iPOC = POC_List[frameIdx];

  // We schould always be able to seek to the beginning of the file
  int bestSeekPOC = POC_List[0];

  for (nal_unit *nal : nalUnitList)
  {
    if (nal->isSlice()) 
    {
      // We can cast this to a slice.
      slice *s = dynamic_cast<slice*>(nal);

      if (s->PicOrderCntVal <= iPOC) 
        // We could seek here
        bestSeekPOC = s->PicOrderCntVal;
      else
        break;
    }
  }

  // Get the frame index for the given POC
  return POC_List.indexOf(bestSeekPOC);
}

QByteArray fileSourceHEVCAnnexBFile::seekToFrameNumber(int iFrameNr)
{
  // Get the POC for the frame number
  int iPOC = POC_List[iFrameNr];

  // Collect the active parameter sets
  QMap<int, vps*> active_VPS_list;
  QMap<int, sps*> active_SPS_list;
  QMap<int, pps*> active_PPS_list;
  
  for (nal_unit *nal : nalUnitList)
  {
    if (nal->isSlice()) 
    {
      // We can cast this to a slice.
      slice *s = dynamic_cast<slice*>(nal);

      if (s->PicOrderCntVal == iPOC) 
      {
        // Seek here
        seekToFilePos(s->filePos);

        // Get the bitstream of all active parameter sets
        QByteArray paramSetStream;

        for (vps *v : active_VPS_list)
          paramSetStream.append(v->getParameterSetData());
        for (sps *s : active_SPS_list)
          paramSetStream.append(s->getParameterSetData());
        for (pps *p : active_PPS_list)
          paramSetStream.append(p->getParameterSetData());

        return paramSetStream;
      }
    }
    else if (nal->nal_type == VPS_NUT) {
      // Add vps (replace old one if existed)
      vps *v = dynamic_cast<vps*>(nal);
      active_VPS_list.insert(v->vps_video_parameter_set_id, v);
    }
    else if (nal->nal_type == SPS_NUT) {
      // Add sps (replace old one if existed)
      sps *s = dynamic_cast<sps*>(nal);
      active_SPS_list.insert(s->sps_seq_parameter_set_id, s);
    }
    else if (nal->nal_type == PPS_NUT) {
      // Add pps (replace old one if existed)
      pps *p = dynamic_cast<pps*>(nal);
      active_PPS_list.insert(p->pps_pic_parameter_set_id, p);
    }
  }

  return QByteArray();
}

bool fileSourceHEVCAnnexBFile::seekToFilePos(quint64 pos)
{
  DEBUG_ANNEXB("fileSourceHEVCAnnexBFile::seekToFilePos %d", pos);

  if (!srcFile.seek(pos))
    return false;

  bufferStartPosInFile = pos;
  numZeroBytes = 0;
  fileBufferSize = 0;

  return updateBuffer();
}

QSize fileSourceHEVCAnnexBFile::getSequenceSize() const
{
  // Find the first SPS and return the size
  for (nal_unit *nal : nalUnitList)
  {
    if (nal->nal_type == SPS_NUT) 
    {
      sps *s = dynamic_cast<sps*>(nal);
      return QSize(s->get_conformance_cropping_width(), s->get_conformance_cropping_height());
    }
  }

  return QSize(-1,-1);
}

double fileSourceHEVCAnnexBFile::getFramerate() const
{
  // First try to get the framerate from the parameter sets themselves
  for (nal_unit *nal : nalUnitList)
  {
    if (nal->nal_type == VPS_NUT) 
    {
      vps *v = dynamic_cast<vps*>(nal);
      if (v->vps_timing_info_present_flag)
        // The VPS knows the frame rate
        return v->frameRate;
    }
  }

  // The VPS had no information on the frame rate.
  // Look for VUI information in the sps
  for (nal_unit *nal : nalUnitList)
  {
    if (nal->nal_type == SPS_NUT)
    {
      sps *s = dynamic_cast<sps*>(nal);
      if (s->vui_parameters_present_flag && s->sps_vui_parameters.vui_timing_info_present_flag)
        // The VUI knows the frame rate
        return s->sps_vui_parameters.frameRate;
    }
  }

  // Try to find the framerate from the file name
  QSize frameSize;
  int frameRate, bitDepth;
  QString subFormat;
  formatFromFilename(frameSize, frameRate, bitDepth);
  if (frameRate != -1)
    return (double)frameRate;

  return DEFAULT_FRAMERATE;
}

QByteArray fileSourceHEVCAnnexBFile::getRemainingBuffer_Update()
{
  QByteArray retArr = fileBuffer.mid(posInBuffer, fileBufferSize-posInBuffer); 
  updateBuffer();
  return retArr;
}

void fileSourceHEVCAnnexBFile::nal_unit::parse_nal_unit_header(const QByteArray &parameterSetData, TreeItem *root)
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
  int nal_unit_type_id;
  READBITS(nal_unit_type_id, 6);
  nal_type = (nal_unit_type_id > UNSPECIFIED) ? UNSPECIFIED : (nal_unit_type)nal_unit_type_id;

  READBITS(nuh_layer_id, 6);
  READBITS(nuh_temporal_id_plus1, 3);
}

bool fileSourceHEVCAnnexBFile::nal_unit::isIRAP()
{ 
  return (nal_type == BLA_W_LP       || nal_type == BLA_W_RADL ||
          nal_type == BLA_N_LP       || nal_type == IDR_W_RADL ||
          nal_type == IDR_N_LP       || nal_type == CRA_NUT    ||
          nal_type == RSV_IRAP_VCL22 || nal_type == RSV_IRAP_VCL23); 
}

bool fileSourceHEVCAnnexBFile::nal_unit::isSLNR() 
{ 
  return (nal_type == TRAIL_N     || nal_type == TSA_N       ||
          nal_type == STSA_N      || nal_type == RADL_N      ||
          nal_type == RASL_N      || nal_type == RSV_VCL_N10 ||
          nal_type == RSV_VCL_N12 || nal_type == RSV_VCL_N14); 
}

bool fileSourceHEVCAnnexBFile::nal_unit::isRADL() { 
  return (nal_type == RADL_N || nal_type == RADL_R); 
}

bool fileSourceHEVCAnnexBFile::nal_unit::isRASL() 
{ 
  return (nal_type == RASL_N || nal_type == RASL_R); 
}

bool fileSourceHEVCAnnexBFile::nal_unit::isSlice() 
{ 
  return (nal_type == IDR_W_RADL || nal_type == IDR_N_LP   || nal_type == CRA_NUT  ||
          nal_type == BLA_W_LP   || nal_type == BLA_W_RADL || nal_type == BLA_N_LP ||
          nal_type == TRAIL_N    || nal_type == TRAIL_R    || nal_type == TSA_N    ||
          nal_type == TSA_R      || nal_type == STSA_N     || nal_type == STSA_R   ||
          nal_type == RADL_N     || nal_type == RADL_R     || nal_type == RASL_N   ||
          nal_type == RASL_R); 
}

QByteArray fileSourceHEVCAnnexBFile::nal_unit::getNALHeader() const
{ 
  int out = ((int)nal_type << 9) + (nuh_layer_id << 3) + nuh_temporal_id_plus1;
  char c[6] = { 0, 0, 0, 1,  (char)(out >> 8), (char)out };
  return QByteArray(c, 6);
}

QVariant fileSourceHEVCAnnexBFile::NALUnitModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole && rootItem != nullptr)
    return rootItem->itemData.value(section, QString());

  return QVariant();
}

QVariant fileSourceHEVCAnnexBFile::NALUnitModel::data(const QModelIndex &index, int role) const
{
  //qDebug() << "ileSourceHEVCAnnexBFile::data " << index;

  if (!index.isValid())
    return QVariant();

  if (role != Qt::DisplayRole)
    return QVariant();

  TreeItem *item = static_cast<TreeItem*>(index.internalPointer());

  return QVariant(item->itemData.value(index.column()));
}

QModelIndex fileSourceHEVCAnnexBFile::NALUnitModel::index(int row, int column, const QModelIndex &parent) const
{
  //qDebug() << "ileSourceHEVCAnnexBFile::index " << row << column << parent;

  if (!hasIndex(row, column, parent))
    return QModelIndex();

  TreeItem *parentItem;

  if (!parent.isValid())
    parentItem = rootItem.data();
  else
    parentItem = static_cast<TreeItem*>(parent.internalPointer());

  if (parentItem == nullptr)
    return QModelIndex();

  TreeItem *childItem = parentItem->childItems.value(row, nullptr);
  if (childItem)
    return createIndex(row, column, childItem);
  else
    return QModelIndex();
}

QModelIndex fileSourceHEVCAnnexBFile::NALUnitModel::parent(const QModelIndex &index) const
{
  //qDebug() << "ileSourceHEVCAnnexBFile::parent " << index;

  if (!index.isValid())
    return QModelIndex();

  TreeItem *childItem = static_cast<TreeItem*>(index.internalPointer());
  TreeItem *parentItem = childItem->parentItem;

  if (parentItem == rootItem.data())
    return QModelIndex();

  // Get the row of the item in the list of children of the parent item
  int row = 0;
  if (parentItem)
    row = parentItem->parentItem->childItems.indexOf(const_cast<TreeItem*>(parentItem));

  return createIndex(row, 0, parentItem);

}

int fileSourceHEVCAnnexBFile::NALUnitModel::rowCount(const QModelIndex &parent) const
{
  //qDebug() << "ileSourceHEVCAnnexBFile::rowCount " << parent;

  TreeItem *parentItem;
  if (parent.column() > 0)
    return 0;

  if (!parent.isValid())
    parentItem = rootItem.data();
  else
    parentItem = static_cast<TreeItem*>(parent.internalPointer());

  return (parentItem == nullptr) ? 0 : parentItem->childItems.count();
}

void fileSourceHEVCAnnexBFile::sei::parse_sei_message(const QByteArray &sliceHeaderData, TreeItem *root)
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
  LOGVAL(payloadType);
  
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

  // Here comes the payload (Annex D)
  // Not implemented.
}
