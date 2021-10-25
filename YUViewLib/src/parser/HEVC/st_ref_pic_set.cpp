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

#include "st_ref_pic_set.h"

#include <parser/common/Functions.h>
#include "slice_segment_header.h"

namespace parser::hevc
{

unsigned st_ref_pic_set::st_ref_pic_set::NumNegativePics[65]{};
unsigned st_ref_pic_set::st_ref_pic_set::NumPositivePics[65]{};
int st_ref_pic_set::st_ref_pic_set::DeltaPocS0[65][16]{};
int st_ref_pic_set::st_ref_pic_set::DeltaPocS1[65][16]{};
bool st_ref_pic_set::st_ref_pic_set::UsedByCurrPicS0[65][16]{};
bool st_ref_pic_set::st_ref_pic_set::UsedByCurrPicS1[65][16]{};
unsigned st_ref_pic_set::st_ref_pic_set::NumDeltaPocs[65]{};

using namespace reader;

void st_ref_pic_set::parse(SubByteReaderLogging &reader, unsigned stRpsIdx, unsigned num_short_term_ref_pic_sets)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "st_ref_pic_set()");
  
  if (stRpsIdx > 64)
    throw std::logic_error("Error while parsing short term ref pic set. The stRpsIdx must be in the range [0..64].");

  this->inter_ref_pic_set_prediction_flag = false;
  if(stRpsIdx != 0)
    this->inter_ref_pic_set_prediction_flag = reader.readFlag("inter_ref_pic_set_prediction_flag");

  if (this->inter_ref_pic_set_prediction_flag)
  {
    this->delta_idx_minus1 = 0;
    if (stRpsIdx == num_short_term_ref_pic_sets)
      this->delta_idx_minus1 = reader.readUEV("delta_idx_minus1");
    this->delta_rps_sign = reader.readFlag("delta_rps_sign");
    this->abs_delta_rps_minus1 = reader.readUEV("abs_delta_rps_minus1");

    int RefRpsIdx = stRpsIdx - (this->delta_idx_minus1 + 1);                     // Rec. ITU-T H.265 v3 (04/2015) (7-57)
    int deltaRps = (1 - 2 * this->delta_rps_sign) * (this->abs_delta_rps_minus1 + 1);  // Rec. ITU-T H.265 v3 (04/2015) (7-58)
    reader.logCalculatedValue("RefRpsIdx", RefRpsIdx);
    reader.logCalculatedValue("deltaRps", deltaRps);

    for(unsigned j=0; j<=NumDeltaPocs[RefRpsIdx]; j++)
    {
      this->used_by_curr_pic_flag.push_back(reader.readFlag(formatArray("used_by_curr_pic_flag", j)));
      if(!this->used_by_curr_pic_flag.back())
        this->use_delta_flag.push_back(reader.readFlag(formatArray("use_delta_flag", j)));
      else
        this->use_delta_flag.push_back(true);
    }

    // Derive NumNegativePics Rec. ITU-T H.265 v3 (04/2015) (7-59)
    unsigned i = 0;
    for(int j=int(NumPositivePics[RefRpsIdx]) - 1; j >= 0; j--)
    {
      int dPoc = DeltaPocS1[RefRpsIdx][j] + deltaRps;
      if(dPoc < 0 && this->use_delta_flag[NumNegativePics[RefRpsIdx] + j]) 
      { 
        DeltaPocS0[stRpsIdx][i] = dPoc;
        reader.logArbitrary(formatArray("DeltaPocS0", stRpsIdx, i), std::to_string(dPoc));
        UsedByCurrPicS0[stRpsIdx][i++] = this->used_by_curr_pic_flag[NumNegativePics[RefRpsIdx] + j];
      }
    }
    if(deltaRps < 0 && this->use_delta_flag[NumDeltaPocs[RefRpsIdx]])
    { 
      DeltaPocS0[stRpsIdx][i] = deltaRps;
      reader.logArbitrary(formatArray("DeltaPocS0", stRpsIdx, i), std::to_string(deltaRps));
      UsedByCurrPicS0[stRpsIdx][i++] = this->used_by_curr_pic_flag[NumDeltaPocs[RefRpsIdx]];
    }
    for(unsigned int j=0; j<NumNegativePics[RefRpsIdx]; j++)
    { 
      int dPoc = DeltaPocS0[RefRpsIdx][j] + deltaRps;
      if(dPoc < 0 && this->use_delta_flag[j])
      { 
        DeltaPocS0[stRpsIdx][i] = dPoc;
        reader.logArbitrary(formatArray("DeltaPocS0", stRpsIdx, i), std::to_string(dPoc));
        UsedByCurrPicS0[stRpsIdx][i++] = this->used_by_curr_pic_flag[j];
      } 
    } 
    NumNegativePics[stRpsIdx] = i;
    reader.logCalculatedValue(formatArray("NumNegativePics", stRpsIdx), i);

    // Derive NumPositivePics Rec. ITU-T H.265 v3 (04/2015) (7-60)
    i = 0;
    for(int j=int(NumNegativePics[RefRpsIdx]) - 1; j>=0; j--)
    { 
      auto dPoc = DeltaPocS0[RefRpsIdx][j] + deltaRps;
      if(dPoc > 0 && this->use_delta_flag[j])
      { 
        DeltaPocS1[stRpsIdx][i] = dPoc;
        reader.logArbitrary(formatArray("DeltaPocS1", stRpsIdx, i), std::to_string(dPoc));
        UsedByCurrPicS1[stRpsIdx][i++] = this->used_by_curr_pic_flag[j];
      }
    }
    if(deltaRps > 0 && this->use_delta_flag[NumDeltaPocs[RefRpsIdx]])
    {
      DeltaPocS1[stRpsIdx][i] = deltaRps;
      reader.logArbitrary(formatArray("DeltaPocS1", stRpsIdx, i), std::to_string(deltaRps));
      UsedByCurrPicS1[stRpsIdx][i++] = this->used_by_curr_pic_flag[NumDeltaPocs[RefRpsIdx]];
    }
    for(unsigned j=0; j<NumPositivePics[RefRpsIdx]; j++)
    { 
      int dPoc = DeltaPocS1[RefRpsIdx][j] + deltaRps;
      if(dPoc > 0 && this->use_delta_flag[NumNegativePics[RefRpsIdx] + j])
      { 
        DeltaPocS1[stRpsIdx][i] = dPoc;
        reader.logArbitrary(formatArray("DeltaPocS1", stRpsIdx, i), std::to_string(dPoc));
        UsedByCurrPicS1[stRpsIdx][i++] = this->used_by_curr_pic_flag[NumNegativePics[RefRpsIdx] + j] ;
      }
    }
    NumPositivePics[stRpsIdx] = i;
    reader.logCalculatedValue(formatArray("NumPositivePics", stRpsIdx), i);
  }
  else
  {
    this->num_negative_pics = reader.readUEV("num_negative_pics");
    this->num_positive_pics = reader.readUEV("num_positive_pics");
    for(unsigned i = 0; i < num_negative_pics; i++)
    {
      this->delta_poc_s0_minus1.push_back(reader.readUEV(formatArray("delta_poc_s0_minus1", i)));
      this->used_by_curr_pic_s0_flag.push_back(reader.readFlag(formatArray("used_by_curr_pic_s0_flag", i)));

      if (i==0)
        DeltaPocS0[stRpsIdx][i] = -(int(this->delta_poc_s0_minus1.back()) + 1); // (7-65)
      else
        DeltaPocS0[stRpsIdx][i] = DeltaPocS0[stRpsIdx][i-1] - (this->delta_poc_s0_minus1.back() + 1); // (7-67)
      reader.logArbitrary(formatArray("DeltaPocS0", stRpsIdx, i), std::to_string(DeltaPocS0[stRpsIdx][i]));
      UsedByCurrPicS0[stRpsIdx][i] = used_by_curr_pic_s0_flag[i];
      reader.logArbitrary(formatArray("UsedByCurrPicS0", stRpsIdx, i), std::to_string(UsedByCurrPicS0[stRpsIdx][i]));
      
    }
    for(unsigned i = 0; i < num_positive_pics; i++)
    {
      this->delta_poc_s1_minus1.push_back(reader.readUEV(formatArray("delta_poc_s1_minus1", i)));
      this->used_by_curr_pic_s1_flag.push_back(reader.readFlag(formatArray("used_by_curr_pic_s1_flag", i)));

      if (i==0)
        DeltaPocS1[stRpsIdx][i] = this->delta_poc_s1_minus1.back() + 1; // (7-66)
      else
        DeltaPocS1[stRpsIdx][i] = DeltaPocS1[stRpsIdx][i-1] + (this->delta_poc_s1_minus1.back() + 1); // (7-68)
      reader.logArbitrary(formatArray("DeltaPocS1", stRpsIdx, i), std::to_string(DeltaPocS1[stRpsIdx][i]));
      UsedByCurrPicS1[stRpsIdx][i] = used_by_curr_pic_s1_flag[i];
      reader.logArbitrary(formatArray("UsedByCurrPicS1", stRpsIdx, i), std::to_string(UsedByCurrPicS1[stRpsIdx][i]));
    }

    NumNegativePics[stRpsIdx] = num_negative_pics;
    NumPositivePics[stRpsIdx] = num_positive_pics;
    reader.logArbitrary(formatArray("NumNegativePics", stRpsIdx), std::to_string(num_negative_pics));
    reader.logArbitrary(formatArray("NumPositivePics", stRpsIdx), std::to_string(num_positive_pics));
  }

  NumDeltaPocs[stRpsIdx] = NumNegativePics[stRpsIdx] + NumPositivePics[stRpsIdx]; // (7-69)
}

// (7-55)
unsigned st_ref_pic_set::NumPicTotalCurr(int CurrRpsIdx, const slice_segment_header* slice)
{
  int NumPicTotalCurr = 0;
  for(unsigned int i = 0; i < NumNegativePics[CurrRpsIdx]; i++)
    if(UsedByCurrPicS0[CurrRpsIdx][i])
      NumPicTotalCurr++ ;
  for(unsigned int i = 0; i < NumPositivePics[CurrRpsIdx]; i++)  
    if(UsedByCurrPicS1[CurrRpsIdx][i]) 
      NumPicTotalCurr++;
  for(unsigned int i = 0; i < slice->num_long_term_sps + slice->num_long_term_pics; i++) 
    if(slice->UsedByCurrPicLt[i])
      NumPicTotalCurr++;
  return NumPicTotalCurr;
}

} // namespace parser::hevc