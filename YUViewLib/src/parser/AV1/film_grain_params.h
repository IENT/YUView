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

#pragma once

#include <parser/common/SubByteReaderLogging.h>

#include "Typedef.h"

namespace parser::av1
{

class sequence_header_obu;

class film_grain_params
{
public:
  film_grain_params() = default;

  void parse(reader::SubByteReaderLogging &       reader,
             std::shared_ptr<sequence_header_obu> seqHeader,
             bool                                 show_frame,
             bool                                 showable_frame,
             FrameType                            frame_type);

  bool             apply_grain{};
  unsigned         grain_seed{};
  bool             update_grain{};
  unsigned         film_grain_params_ref_idx{};
  unsigned         num_y_points{};
  vector<unsigned> point_y_value;
  vector<unsigned> point_y_scaling;
  bool             chroma_scaling_from_luma{};
  unsigned         num_cb_points{};
  unsigned         num_cr_points{};
  vector<unsigned> point_cb_value;
  vector<unsigned> point_cb_scaling;
  vector<unsigned> point_cr_value;
  vector<unsigned> point_cr_scaling;
  unsigned         grain_scaling_minus_8{};
  unsigned         ar_coeff_lag{};
  unsigned         numPosChroma{};
  vector<unsigned> ar_coeffs_y_plus_128;
  vector<unsigned> ar_coeffs_cb_plus_128;
  vector<unsigned> ar_coeffs_cr_plus_128;
  unsigned         ar_coeff_shift_minus_6{};
  unsigned         grain_scale_shift{};
  unsigned         cb_mult{};
  unsigned         cb_luma_mult{};
  unsigned         cb_offset{};
  unsigned         cr_mult{};
  unsigned         cr_luma_mult{};
  unsigned         cr_offset{};
  bool             overlap_flag{};
  bool             clip_to_restricted_range{};
};

} // namespace parser::av1
