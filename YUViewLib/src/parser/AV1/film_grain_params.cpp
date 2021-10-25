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

#include "film_grain_params.h"

#include <parser/common/functions.h>
#include "sequence_header_obu.h"

namespace parser::av1
{

using namespace reader;

void film_grain_params::parse(reader::SubByteReaderLogging &       reader,
                              std::shared_ptr<sequence_header_obu> seqHeader,
                              bool                                 show_frame,
                              bool                                 showable_frame,
                              FrameType                            frame_type)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "film_grain_params()");

  if (!seqHeader->film_grain_params_present || (!show_frame && !showable_frame))
  {
    reader.logArbitrary("reset_grain_params()");
    return;
  }
  this->apply_grain = reader.readFlag("apply_grain");
  if (!this->apply_grain)
  {
    reader.logArbitrary("reset_grain_params()");
    return;
  }
  this->grain_seed = reader.readBits("grain_seed", 16);
  if (frame_type == FrameType::INTER_FRAME)
    this->update_grain = reader.readFlag("update_grain");
  else
    this->update_grain = true;
  if (!this->update_grain)
  {
    this->film_grain_params_ref_idx = reader.readBits("film_grain_params_ref_idx", 3);
    auto tempGrainSeed              = this->grain_seed;
    reader.logArbitrary("load_grain_params(" + std::to_string(this->film_grain_params_ref_idx) + ")");
    this->grain_seed = tempGrainSeed;
    return;
  }
  this->num_y_points = reader.readBits("num_y_points", 4);
  for (unsigned i = 0; i < this->num_y_points; i++)
  {
    this->point_y_value.push_back(reader.readBits(formatArray("point_y_value", i), 8));
    this->point_y_scaling.push_back(reader.readBits(formatArray("point_y_scaling", i), 8));
  }
  if (seqHeader->colorConfig.mono_chrome)
  {
    this->chroma_scaling_from_luma = false;
  }
  else
  {
    this->chroma_scaling_from_luma = reader.readFlag("chroma_scaling_from_luma");
  }
  if (seqHeader->colorConfig.mono_chrome || this->chroma_scaling_from_luma ||
      (seqHeader->colorConfig.subsampling_x == 1 && seqHeader->colorConfig.subsampling_y == 1 &&
       this->num_y_points == 0))
  {
    this->num_cb_points = 0;
    this->num_cr_points = 0;
  }
  else
  {
    this->num_cb_points = reader.readBits("num_cb_points", 4);
    for (unsigned i = 0; i < this->num_cb_points; i++)
    {
      this->point_cb_value.push_back(reader.readBits(formatArray("point_cb_value", i), 8));
      this->point_cb_scaling.push_back(reader.readBits(formatArray("point_cb_scaling", i), 8));
    }
    this->num_cr_points = reader.readBits("num_cr_points", 4);
    for (unsigned i = 0; i < this->num_cr_points; i++)
    {
      this->point_cr_value.push_back(reader.readBits(formatArray("point_cr_value", i), 8));
      this->point_cr_scaling.push_back(reader.readBits(formatArray("point_cr_scaling", i), 8));
    }
  }

  this->grain_scaling_minus_8 = reader.readBits("grain_scaling_minus_8", 2);
  this->ar_coeff_lag          = reader.readBits("ar_coeff_lag", 2);
  auto numPosLuma             = 2 * this->ar_coeff_lag * (this->ar_coeff_lag + 1);
  if (num_y_points)
  {
    this->numPosChroma = numPosLuma + 1;
    for (unsigned i = 0; i < numPosLuma; i++)
      this->ar_coeffs_y_plus_128.push_back(
          reader.readBits(formatArray("ar_coeffs_y_plus_128", i), 8));
  }
  else
  {
    this->numPosChroma = numPosLuma;
  }
  if (this->chroma_scaling_from_luma || num_cb_points)
  {
    for (unsigned i = 0; i < numPosChroma; i++)
      this->ar_coeffs_cb_plus_128.push_back(
          reader.readBits(formatArray("ar_coeffs_cb_plus_128", i), 8));
  }
  if (this->chroma_scaling_from_luma || num_cr_points)
  {
    for (unsigned i = 0; i < numPosChroma; i++)
      this->ar_coeffs_cr_plus_128.push_back(
          reader.readBits(formatArray("ar_coeffs_cr_plus_128", i), 8));
  }
  this->ar_coeff_shift_minus_6 = reader.readBits("ar_coeff_shift_minus_6", 2);
  this->grain_scale_shift      = reader.readBits("grain_scale_shift", 2);
  if (num_cb_points)
  {
    this->cb_mult      = reader.readBits("cb_mult", 8);
    this->cb_luma_mult = reader.readBits("cb_luma_mult", 8);
    this->cb_offset    = reader.readBits("cb_offset", 9);
  }
  if (num_cr_points)
  {
    this->cr_mult      = reader.readBits("cr_mult", 8);
    this->cr_luma_mult = reader.readBits("cr_luma_mult", 8);
    this->cr_offset    = reader.readBits("cr_offset", 9);
  }
  this->overlap_flag             = reader.readFlag("overlap_flag");
  this->clip_to_restricted_range = reader.readFlag("clip_to_restricted_range");
}

} // namespace parser::av1