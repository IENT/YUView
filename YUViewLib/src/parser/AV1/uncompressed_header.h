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

#include "FrameRefs.h"
#include "GlobalDecodingValues.h"
#include "OpenBitstreamUnit.h"
#include "cdef_params.h"
#include "delta_lf_params.h"
#include "delta_q_params.h"
#include "frame_size.h"
#include "interpolation_filter.h"
#include "loop_filter_params.h"
#include "parser/common/SubByteReaderLogging.h"
#include "quantization_params.h"
#include "segmentation_params.h"
#include "tile_info.h"


namespace parser::av1
{

class sequence_header_obu;

class uncompressed_header
{
public:
  uncompressed_header() = default;

  void parse(reader::SubByteReaderLogging &       reader,
             std::shared_ptr<sequence_header_obu> seqHeader,
             GlobalDecodingValues &               decValues,
             unsigned                             temporal_id,
             unsigned                             spatial_id);

  bool     show_existing_frame{};
  unsigned frame_to_show_map_idx{};
  unsigned refresh_frame_flags{};
  unsigned display_frame_id{};

  frame_size frameSize;

  FrameType frame_type{};

  bool FrameIsIntra{};
  bool show_frame{};
  bool showable_frame{};
  bool error_resilient_mode{};

  bool     disable_cdf_update{};
  unsigned allow_screen_content_tools{};
  unsigned force_integer_mv{};

  unsigned current_frame_id{};
  bool     frame_size_override_flag{};
  unsigned order_hint{};
  unsigned OrderHint{};
  unsigned primary_ref_frame{};

  bool             buffer_removal_time_present_flag{};
  vector<unsigned> opPtIdc{};
  vector<bool>     inTemporalLayer{};
  vector<bool>     inSpatialLayer{};
  vector<unsigned> buffer_removal_time;

  bool allow_high_precision_mv{};
  bool use_ref_frame_mvs{};
  bool allow_intrabc{};

  vector<unsigned> ref_order_hint;

  bool     frame_refs_short_signaling{};
  unsigned last_frame_idx{};
  unsigned gold_frame_idx{};

  FrameRefs frameRefs{};

  vector<unsigned> delta_frame_id_minus_1;
  vector<unsigned> expectedFrameId;

  interpolation_filter interpolationFilter{};

  bool is_motion_mode_switchable{};
  bool disable_frame_end_update_cdf{};

  tile_info           tileInfo;
  quantization_params quantizationParams;
  segmentation_params segmentationParams;
  delta_q_params      deltaQParams;
  delta_lf_params     deltaLfParams;

  bool CodedLossless{};
  bool LosslessArray[8]{};
  int  SegQMLevel[3][8]{};
  bool AllLossless{};

  loop_filter_params loopFilterParams;
  cdef_params        cdefParams;

  bool allow_warped_motion{};
  bool reduced_tx_set{};

private:
  void mark_ref_frames(int                                  idLen,
                       std::shared_ptr<sequence_header_obu> seqHeader,
                       GlobalDecodingValues &               decValues);
  int  get_qindex(bool ignoreDeltaQ, int segmentId) const;
  bool seg_feature_active_idx(int idx, int feature) const;
};

} // namespace parser::av1
