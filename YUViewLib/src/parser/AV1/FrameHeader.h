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

#include "CommonTypes.h"
#include "OBU.h"
#include "SequenceHeader.h"
#include "GlobalDecodingValues.h"

namespace AV1
{

FrameType indexToFrameType(unsigned index);

struct FrameHeader : public OBU
{
  FrameHeader(const OBU &obu) : OBU(obu) {};
  bool parse(const QByteArray &sequenceHeaderData, TreeItem *root, QSharedPointer<SequenceHeader> seq_header, GlobalDecodingValues &decValues);

  bool parse_uncompressed_header(ReaderHelper &reader, QSharedPointer<SequenceHeader> seq_header, GlobalDecodingValues &decValues);
  void mark_ref_frames(int idLen, QSharedPointer<SequenceHeader> seq_header, GlobalDecodingValues &decValues);

  bool show_existing_frame;
  unsigned int frame_to_show_map_idx;
  unsigned int refresh_frame_flags;
  unsigned int display_frame_id;

  FrameType frame_type;

  bool FrameIsIntra;
  bool show_frame;
  bool showable_frame;
  bool error_resilient_mode;

  bool disable_cdf_update;
  unsigned int allow_screen_content_tools;
  unsigned int force_integer_mv;

  unsigned int current_frame_id;
  bool frame_size_override_flag;
  unsigned int order_hint;
  unsigned int OrderHint;
  unsigned int primary_ref_frame;

  bool buffer_removal_time_present_flag;
  int opPtIdc;
  bool inTemporalLayer;
  bool inSpatialLayer;
  QList<unsigned int> buffer_removal_time;

  bool allow_high_precision_mv;
  bool use_ref_frame_mvs;
  bool allow_intrabc;

  QList<unsigned int> ref_order_hint;

  bool parse_frame_size(ReaderHelper &reader, QSharedPointer<SequenceHeader> seq_header);
  bool parse_frame_size_with_refs(ReaderHelper &reader, QSharedPointer<SequenceHeader> seq_header, GlobalDecodingValues &decValues);
  unsigned int frame_width_minus_1;
  unsigned int frame_height_minus_1;
  unsigned int FrameWidth;
  unsigned int FrameHeight;

  bool parse_superres_params(ReaderHelper &reader, QSharedPointer<SequenceHeader> seq_header);
  bool use_superres;
  unsigned int coded_denom;
  unsigned int SuperresDenom;
  unsigned int UpscaledWidth;

  void compute_image_size();
  int MiCols;
  int MiRows;

  bool parse_render_size(ReaderHelper &reader);
  bool render_and_frame_size_different;
  unsigned int render_width_minus_1;
  unsigned int render_height_minus_1;
  unsigned int RenderWidth;
  unsigned int RenderHeight;

  bool frame_refs_short_signaling;
  unsigned int delta_frame_id_minus_1;
  unsigned int last_frame_idx;
  unsigned int gold_frame_idx;

  struct frame_refs_struct
  {
    bool set_frame_refs(ReaderHelper &reader, int OrderHintBits, bool enable_order_hint, int last_frame_idx, int gold_frame_idx, int OrderHint, GlobalDecodingValues &decValues);

    int find_latest_backward(int curFrameHint);
    int find_earliest_backward(int curFrameHint);
    int find_latest_forward(int curFrameHint);

    int get_relative_dist(int a, int b, bool enable_order_hint, int OrderHintBits);

    int ref_frame_idx[8];
    bool usedFrame[8];
    int shiftedOrderHints[8];
    // TODO: How should these be initialized. I think this is missing in the specification.
    int latestOrderHint {-1};
    int earliestOrderHint {32000};
  };
  frame_refs_struct frame_refs;

  int expectedFrameId[8];
  bool is_motion_mode_switchable;

  bool disable_frame_end_update_cdf;

  bool read_interpolation_filter(ReaderHelper &reader);
  bool is_filter_switchable;
  enum interpolation_filter_enum
  {
    EIGHTTAP,
    EIGHTTAP_SMOOTH,
    EIGHTTAP_SHARP,
    BILINEAR,
    SWITCHABLE
  };
  interpolation_filter_enum interpolation_filter;

  struct tile_info_struct
  {
    bool parse_tile_info(int MiCols, int MiRows, ReaderHelper &reader, QSharedPointer<SequenceHeader> seq_header);

    int sbCols, sbRows;
    int sbShift;
    int sbSize;
    int maxTileWidthSb, maxTileAreaSb;
    int minLog2TileCols, maxLog2TileCols;
    int maxLog2TileRows;
    int minLog2Tiles;

    bool uniform_tile_spacing_flag;
    int TileColsLog2;
    bool increment_tile_cols_log2;

    int tileWidthSb;
    QList<int> MiColStarts;
    int TileCols;
    int minLog2TileRows;
    int TileRowsLog2;
    int tileHeightSb;
    int TileRows;
    QList<int> MiRowStarts;

    int widestTileSb;
    int startSb;
    int width_in_sbs_minus_1, height_in_sbs_minus_1;
    int maxTileHeightSb;
    unsigned int tile_size_bytes_minus_1;
    unsigned int context_update_tile_id;
    int TileSizeBytes;
  };
  tile_info_struct tile_info;

  struct quantization_params_struct
  {
    bool parse_quantization_params(ReaderHelper &reader, QSharedPointer<SequenceHeader> seq_header);
    bool read_delta_q(QString deltaValName, int &delta_q, ReaderHelper &reader);

    unsigned int base_q_idx;
    bool diff_uv_delta;

    int DeltaQYDc;
    int DeltaQUDc;
    int DeltaQUAc;
    int DeltaQVDc;
    int DeltaQVAc;

    bool using_qmatrix;
    unsigned int qm_y, qm_u, qm_v;
  };
  quantization_params_struct quantization_params;

  struct segmentation_params_struct
  {
    bool parse_segmentation_params(int primary_ref_frame, ReaderHelper &reader);

    bool segmentation_enabled;
    bool segmentation_update_map;
    bool segmentation_temporal_update;
    bool segmentation_update_data;

    bool FeatureEnabled[8][8];
    int  FeatureData[8][8];

    bool SegIdPreSkip;
    int LastActiveSegId;
  };
  segmentation_params_struct segmentation_params;

  struct delta_q_params_struct
  {
    bool parse_delta_q_params(int base_q_idx, ReaderHelper &reader);

    unsigned int delta_q_res;
    bool delta_q_present;
  };
  delta_q_params_struct delta_q_params;

  struct delta_lf_params_struct
  {
    bool parse_delta_lf_params(bool delta_q_present, bool allow_intrabc, ReaderHelper &reader);

    bool delta_lf_present;
    unsigned int  delta_lf_res;
    bool delta_lf_multi;
  };
  delta_lf_params_struct delta_lf_params;

  bool CodedLossless;

  bool seg_feature_active_idx(int idx, int feature) const { return segmentation_params.segmentation_enabled && segmentation_params.FeatureEnabled[idx][feature]; }
  int get_qindex(bool ignoreDeltaQ, int segmentId) const;
  bool LosslessArray[8];
  int SegQMLevel[3][8];
  bool AllLossless;

  struct loop_filter_params_struct
  {
    bool parse_loop_filter_params(bool CodedLossless, bool allow_intrabc, ReaderHelper &reader, QSharedPointer<SequenceHeader> seq_header);

    unsigned int loop_filter_level[4];
    int loop_filter_ref_deltas[8];
    int loop_filter_mode_deltas[2];

    unsigned int loop_filter_sharpness;
    bool loop_filter_delta_enabled;
    bool loop_filter_delta_update;

  };
  loop_filter_params_struct loop_filter_params;

  struct cdef_params_struct
  {
    bool parse_cdef_params(bool CodedLossless, bool allow_intrabc, ReaderHelper &reader, QSharedPointer<SequenceHeader> seq_header);

    unsigned int cdef_bits;
    unsigned int cdef_y_pri_strength[16];
    unsigned int cdef_y_sec_strength[16];
    unsigned int cdef_uv_pri_strength[16];
    unsigned int cdef_uv_sec_strength[16];
    int CdefDamping;
    unsigned int cdef_damping_minus_3;
  };
  cdef_params_struct cdef_params;

  bool allow_warped_motion;
  bool reduced_tx_set;

};

} // namespace AV1