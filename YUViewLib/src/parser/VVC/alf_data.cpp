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

#include "alf_data.h"

#include "adaptation_parameter_set_rbsp.h"
#include "parser/common/functions.h"

#include <cmath>

namespace parser::vvc
{

using namespace parser::reader;

void alf_data::parse(SubByteReaderLogging &reader, adaptation_parameter_set_rbsp *aps)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "alf_data");

  this->alf_luma_filter_signal_flag = reader.readFlag("alf_luma_filter_signal_flag");
  if (aps->aps_chroma_present_flag)
  {
    this->alf_chroma_filter_signal_flag = reader.readFlag("alf_chroma_filter_signal_flag");
    this->alf_cc_cb_filter_signal_flag  = reader.readFlag("alf_cc_cb_filter_signal_flag");
    this->alf_cc_cr_filter_signal_flag  = reader.readFlag("alf_cc_cr_filter_signal_flag");
  }
  if (this->alf_luma_filter_signal_flag)
  {
    this->alf_luma_clip_flag = reader.readFlag("alf_luma_clip_flag");
    this->alf_luma_num_filters_signalled_minus1 =
        reader.readUEV("alf_luma_num_filters_signalled_minus1");
    if (this->alf_luma_num_filters_signalled_minus1 > 0)
    {
      auto NumAlfFilters = 25u;
      for (unsigned filtIdx = 0; filtIdx < NumAlfFilters; filtIdx++)
      {
        auto nrBits = std::ceil(std::log2(alf_luma_num_filters_signalled_minus1 + 1));
        this->alf_luma_coeff_delta_idx = reader.readBits(
            formatArray("alf_luma_coeff_delta_idx", filtIdx),
            nrBits,
            Options().withCheckRange({0, this->alf_luma_num_filters_signalled_minus1}));
      }
    }
    for (unsigned sfIdx = 0; sfIdx <= alf_luma_num_filters_signalled_minus1; sfIdx++)
    {
      this->alf_luma_coeff_abs.push_back({});
      this->alf_luma_coeff_sign.push_back({});
      for (unsigned j = 0; j < 12; j++)
      {
        this->alf_luma_coeff_abs[sfIdx].push_back(
            reader.readUEV(formatArray("alf_luma_coeff_abs", sfIdx, j)));
        if (this->alf_luma_coeff_abs[sfIdx][j])
        {
          this->alf_luma_coeff_sign[sfIdx].push_back(
              reader.readFlag(formatArray("alf_luma_coeff_sign", sfIdx, j)));
        }
        else
        {
          this->alf_luma_coeff_sign[sfIdx].push_back(0);
        }
      }
    }
    if (this->alf_luma_clip_flag)
    {
      for (unsigned sfIdx = 0; sfIdx <= alf_luma_num_filters_signalled_minus1; sfIdx++)
      {
        this->alf_luma_clip_idx.push_back({});
        for (unsigned j = 0; j < 12; j++)
        {
          this->alf_luma_clip_idx[sfIdx].push_back(
              reader.readBits(formatArray("alf_luma_clip_idx", sfIdx, j), 2));
        }
      }
    }
  }
  if (this->alf_chroma_filter_signal_flag)
  {
    this->alf_chroma_clip_flag              = reader.readFlag("alf_chroma_clip_flag");
    this->alf_chroma_num_alt_filters_minus1 = reader.readUEV("alf_chroma_num_alt_filters_minus1");
    for (unsigned altIdx = 0; altIdx <= alf_chroma_num_alt_filters_minus1; altIdx++)
    {
      this->alf_chroma_coeff_abs.push_back({});
      this->alf_chroma_coeff_sign.push_back({});
      this->alf_chroma_clip_idx.push_back({});
      for (unsigned j = 0; j < 6; j++)
      {
        this->alf_chroma_coeff_abs[altIdx].push_back(
            reader.readUEV(formatArray("alf_chroma_coeff_abs", altIdx, j)));
        if (this->alf_chroma_coeff_abs[altIdx][j] > 0)
        {
          this->alf_chroma_coeff_sign[altIdx].push_back(
              reader.readFlag(formatArray("alf_chroma_coeff_sign", altIdx, j)));
        }
        else
        {
          this->alf_chroma_coeff_sign[altIdx].push_back(0);
        }
      }
      if (this->alf_chroma_clip_flag)
      {
        for (unsigned j = 0; j < 6; j++)
        {
          this->alf_chroma_clip_idx[altIdx].push_back(
              reader.readBits(formatArray("alf_chroma_clip_idx", altIdx, j), 2));
        }
      }
    }
  }
  if (this->alf_cc_cb_filter_signal_flag)
  {
    this->alf_cc_cb_filters_signalled_minus1 =
        reader.readUEV("alf_cc_cb_filters_signalled_minus1", Options().withCheckRange({0, 3}));
    for (unsigned k = 0; k < alf_cc_cb_filters_signalled_minus1 + 1; k++)
    {
      this->alf_cc_cb_mapped_coeff_abs.push_back({});
      this->alf_cc_cb_coeff_sign.push_back({});
      for (unsigned j = 0; j < 7; j++)
      {
        this->alf_cc_cb_mapped_coeff_abs[k].push_back(
            reader.readBits(formatArray("alf_cc_cb_mapped_coeff_abs", k, j), 3));
        if (this->alf_cc_cb_mapped_coeff_abs[k][j])
        {
          this->alf_cc_cb_coeff_sign[k].push_back(
              reader.readFlag(formatArray("alf_cc_cb_coeff_sign", k, j)));
        }
        else
        {
          this->alf_cc_cb_coeff_sign[k].push_back(0);
        }
      }
    }
  }
  if (this->alf_cc_cr_filter_signal_flag)
  {
    this->alf_cc_cr_filters_signalled_minus1 =
        reader.readUEV("alf_cc_cr_filters_signalled_minus1", Options().withCheckRange({0, 3}));
    for (unsigned k = 0; k < alf_cc_cr_filters_signalled_minus1 + 1; k++)
    {
      this->alf_cc_cr_mapped_coeff_abs.push_back({});
      this->alf_cc_cr_coeff_sign.push_back({});
      for (unsigned j = 0; j < 7; j++)
      {
        this->alf_cc_cr_mapped_coeff_abs[k].push_back(
            reader.readBits(formatArray("alf_cc_cr_mapped_coeff_abs", k, j), 3));
        if (this->alf_cc_cr_mapped_coeff_abs[k][j])
        {
          this->alf_cc_cr_coeff_sign[k].push_back(
              reader.readFlag(formatArray("alf_cc_cr_coeff_sign", k, j)));
        }
        else
        {
          this->alf_cc_cr_coeff_sign[k].push_back(0);
        }
      }
    }
  }
}

} // namespace parser::vvc
