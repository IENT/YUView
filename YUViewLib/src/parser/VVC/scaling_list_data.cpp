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

#include "scaling_list_data.h"

#include "adaptation_parameter_set_rbsp.h"
#include "common/typedef.h"

namespace parser::vvc
{

namespace
{

vector4d<int> calcDiagScanOrder()
{
  vector4d<int> DiagScanOrder;
  for (unsigned log2BlockWidth = 0; log2BlockWidth < 4; log2BlockWidth++)
  {
    DiagScanOrder.push_back({});
    for (unsigned log2BlockHeight = 0; log2BlockHeight < 4; log2BlockHeight++)
    {
      DiagScanOrder[log2BlockWidth].push_back({});
      auto blkWidth  = 1u << log2BlockWidth;
      auto blkHeight = 1u << log2BlockHeight;
      // 6.5.2 (24)
      unsigned i        = 0;
      int      x        = 0;
      int      y        = 0;
      bool     stopLoop = false;
      while (!stopLoop)
      {
        while (y >= 0)
        {
          if (x < int(blkWidth) && y < int(blkHeight))
          {
            DiagScanOrder[log2BlockWidth][log2BlockHeight].push_back({x, y});
            i++;
          }
          y--;
          x++;
        }
        y = x;
        x = 0;
        if (i >= blkWidth * blkHeight)
          stopLoop = true;
      }
    }
  }
  return DiagScanOrder;
}

} // namespace

using namespace parser::reader;

void scaling_list_data::parse(SubByteReaderLogging &reader, adaptation_parameter_set_rbsp *aps)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "scaling_list_data");

  auto DiagScanOrder = calcDiagScanOrder();

  for (unsigned id = 0; id < 28; id++)
  {
    auto matrixSize = id < 2 ? 2u : (id < 8 ? 4u : 8u);
    this->ScalingList.push_back({});
    if (aps->aps_chroma_present_flag || id % 3 == 2 || id == 27)
    {
      this->scaling_list_copy_mode_flag[id] = reader.readFlag("scaling_list_copy_mode_flag");
      if (!this->scaling_list_copy_mode_flag[id])
      {
        this->scaling_list_pred_mode_flag[id] = reader.readFlag("scaling_list_pred_mode_flag");
      }
      if ((this->scaling_list_copy_mode_flag[id] || this->scaling_list_pred_mode_flag[id]) &&
          id != 0 && id != 2 && id != 8)
      {
        this->scaling_list_pred_id_delta[id] = reader.readUEV("scaling_list_pred_id_delta");
      }
      if (!this->scaling_list_copy_mode_flag[id])
      {
        auto nextCoef = 0;
        if (id > 13)
        {
          this->scaling_list_dc_coef[id] = reader.readSEV("scaling_list_dc_coef");
          nextCoef += this->scaling_list_dc_coef[id - 14];
        }

        for (unsigned i = 0; i < matrixSize * matrixSize; i++)
        {
          auto x = DiagScanOrder[3][3][i][0];
          auto y = DiagScanOrder[3][3][i][1];
          if (!(id > 25 && x >= 4 && y >= 4))
          {
            this->scaling_list_delta_coef[id][i] = reader.readSEV("scaling_list_delta_coef");
            nextCoef += this->scaling_list_delta_coef[id][i];
          }
          this->ScalingList[id].push_back(nextCoef);
        }
      }
    }
    else
    {
      this->scaling_list_copy_mode_flag[id] = true;
    }
  }
}

} // namespace parser::vvc
