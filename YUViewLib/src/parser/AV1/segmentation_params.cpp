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

#include "segmentation_params.h"

#include "Typedef.h"

namespace parser::av1
{

using namespace reader;

void segmentation_params::parse(reader::SubByteReaderLogging &reader, unsigned primary_ref_frame)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "segmentation_params()");

  int Segmentation_Feature_Bits[SEG_LVL_MAX]   = {8, 6, 6, 6, 6, 3, 0, 0};
  int Segmentation_Feature_Signed[SEG_LVL_MAX] = {1, 1, 1, 1, 1, 0, 0, 0};
  int Segmentation_Feature_Max[SEG_LVL_MAX]    = {
      255, MAX_LOOP_FILTER, MAX_LOOP_FILTER, MAX_LOOP_FILTER, MAX_LOOP_FILTER, 7, 0, 0};

  this->segmentation_enabled = reader.readFlag("segmentation_enabled");
  if (this->segmentation_enabled)
  {
    if (primary_ref_frame == PRIMARY_REF_NONE)
    {
      this->segmentation_update_map      = 1;
      this->segmentation_temporal_update = 0;
      this->segmentation_update_data     = 1;
    }
    else
    {
      this->segmentation_update_map = reader.readFlag("segmentation_update_map");
      if (this->segmentation_update_map)
        this->segmentation_temporal_update = reader.readFlag("segmentation_temporal_update");
      this->segmentation_update_data = reader.readFlag("segmentation_update_data");
    }
    if (this->segmentation_update_data)
    {
      for (unsigned i = 0; i < MAX_SEGMENTS; i++)
      {
        for (unsigned j = 0; j < SEG_LVL_MAX; j++)
        {
          this->FeatureEnabled[i][j] = reader.readFlag("feature_enabled");
          int clippedValue           = 0;
          if (this->FeatureEnabled[i][j])
          {
            auto bitsToRead = Segmentation_Feature_Bits[j];
            auto limit      = Segmentation_Feature_Max[j];
            if (Segmentation_Feature_Signed[j])
            {
              clippedValue =
                  clip(int(reader.readSU("feature_value", bitsToRead + 1)), -limit, limit);
            }
            else
            {
              clippedValue = clip(int(reader.readBits("feature_value", bitsToRead)), 0, limit);
            }
          }
          this->FeatureData[i][j] = clippedValue;
        }
      }
    }
  }
  else
  {
    for (unsigned i = 0; i < MAX_SEGMENTS; i++)
    {
      for (unsigned j = 0; j < SEG_LVL_MAX; j++)
      {
        this->FeatureEnabled[i][j] = 0;
        this->FeatureData[i][j]    = 0;
      }
    }
  }
  this->SegIdPreSkip    = false;
  this->LastActiveSegId = 0;
  for (unsigned i = 0; i < MAX_SEGMENTS; i++)
  {
    for (unsigned j = 0; j < SEG_LVL_MAX; j++)
    {
      if (this->FeatureEnabled[i][j])
      {
        this->LastActiveSegId = i;
        if (j >= SEG_LVL_REF_FRAME)
          this->SegIdPreSkip = true;
      }
    }
  }
}

} // namespace parser::av1