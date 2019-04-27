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

#ifndef PARSERAV1OBU_H
#define PARSERAV1OBU_H

#include "parserBase.h"
#include "videoHandlerYUV.h"

using namespace YUV_Internals;

class parserAV1OBU : public parserBase
{
  Q_OBJECT

public:
    parserAV1OBU(QObject *parent = nullptr);
    ~parserAV1OBU() {}

    unsigned int parseAndAddOBU(int obuID, QByteArray data, parserCommon::TreeItem *parent, QUint64Pair obuStartEndPosFile = QUint64Pair(-1,-1), QString *obuTypeName=nullptr);

    // So far, we only parse AV1 Obu files from the AVFormat parser so we don't need this (yet).
    // When parsing of raw OBU files is added, we will need this.
    bool runParsingOfFile(QString fileName) Q_DECL_OVERRIDE { Q_UNUSED(fileName); assert(false); return false; }
    QList<QTreeWidgetItem*> getStreamInfo() Q_DECL_OVERRIDE { return QList<QTreeWidgetItem*>(); }
    unsigned int getNrStreams() Q_DECL_OVERRIDE { return 1; }

protected:

  enum frame_type_enum
  {
    KEY_FRAME,
    INTER_FRAME,
    INTRA_ONLY_FRAME,
    SWITCH_FRAME
  };

  // All the different types of OBUs
  enum obu_type_enum
  {
    UNSPECIFIED = -1,
    RESERVED,
    OBU_SEQUENCE_HEADER,
    OBU_TEMPORAL_DELIMITER,
    OBU_FRAME_HEADER,
    OBU_TILE_GROUP,
    OBU_METADATA,
    OBU_FRAME,
    OBU_REDUNDANT_FRAME_HEADER,
    OBU_TILE_LIST,
    OBU_PADDING
  };
  static const QStringList obu_type_toString;

  /* The basic Open bitstream unit. Contains the OBU header and the file position of the unit.
  */
  struct obu_unit
  {
    obu_unit(QUint64Pair filePosStartEnd, int obu_idx) : filePosStartEnd(filePosStartEnd), obu_idx(obu_idx) {}
    obu_unit(QSharedPointer<obu_unit> obu_src);
    virtual ~obu_unit() {} // This class is meant to be derived from.

    // Parse the header the given data bytes.
    bool parse_obu_header(const QByteArray &header_data, unsigned int &nrBytesHeader, parserCommon::TreeItem *root);

    /// Pointer to the first byte of the start code of the NAL unit
    QUint64Pair filePosStartEnd;

    // The index of the obu within the bitstream
    int obu_idx;

    virtual bool isParameterSet() { return false; }
    // Get the raw OBU unit
    // This only works if the payload was saved of course
    //QByteArray getRawOBUData() const { return getOBUHeader() + obuPayload; }

    // Each obu (in all known standards) has a type id
    obu_type_enum obu_type           {UNSPECIFIED};
    bool          obu_extension_flag {false};
    bool          obu_has_size_field {false};
    
    // OBU extension header
    unsigned int temporal_id {0};
    unsigned int spatial_id  {0};

    uint64_t obu_size {0};

    // Optionally, the OBU unit can store it's payload. A parameter set, for example, can thusly be saved completely.
    QByteArray obuPayload;
  };

  struct sequence_header : obu_unit
  {
    sequence_header(const obu_unit &obu) : obu_unit(obu) {};
    bool parse_sequence_header(const QByteArray &sequenceHeaderData, parserCommon::TreeItem *root);

    unsigned int seq_profile;
    bool still_picture;
    bool reduced_still_picture_header;

    bool timing_info_present_flag;
    bool decoder_model_info_present_flag;
    bool initial_display_delay_present_flag;
    unsigned int operating_points_cnt_minus_1;
    QList<unsigned int> operating_point_idc;
    QList<unsigned int> seq_level_idx;
    QList<bool> seq_tier;
    QList<bool> decoder_model_present_for_this_op;
    QList<bool> initial_display_delay_present_for_this_op;
    QList<unsigned int> initial_display_delay_minus_1;

    struct timing_info_struct
    {
      bool parse_timing_info(parserCommon::reader_helper &reader);

      unsigned int num_units_in_display_tick;
      unsigned int time_scale;
      bool equal_picture_interval;
      uint64_t num_ticks_per_picture_minus_1;
    };
    timing_info_struct timing_info;

    struct decoder_model_info_struct
    {
      bool parse_decoder_model(parserCommon::reader_helper &reader);

      unsigned int buffer_delay_length_minus_1;
      unsigned int num_units_in_decoding_tick;
      unsigned int buffer_removal_time_length_minus_1;
      unsigned int frame_presentation_time_length_minus_1;
    };
    decoder_model_info_struct decoder_model_info;

    struct operating_parameters_info_struct
    {
      bool parse_operating_parameters_info(parserCommon::reader_helper &reader, int op, decoder_model_info_struct &dmodel);

      QList<unsigned int> decoder_buffer_delay;
      QList<unsigned int> encoder_buffer_delay;
      QList<bool> low_delay_mode_flag;
    };
    operating_parameters_info_struct operating_parameters_info;

    int operatingPoint;
    int choose_operating_point() { return 0; }
    int OperatingPointIdc;

    unsigned int frame_width_bits_minus_1;
    unsigned int frame_height_bits_minus_1;
    unsigned int max_frame_width_minus_1;
    unsigned int max_frame_height_minus_1;
    bool frame_id_numbers_present_flag;
    unsigned int delta_frame_id_length_minus_2;
    unsigned int additional_frame_id_length_minus_1;
    bool use_128x128_superblock;
    bool enable_filter_intra;
    bool enable_intra_edge_filter;

    bool enable_interintra_compound;
    bool enable_masked_compound;
    bool enable_warped_motion;
    bool enable_dual_filter;
    bool enable_order_hint;
    bool enable_jnt_comp;
    bool enable_ref_frame_mvs;
    bool seq_choose_screen_content_tools;
    unsigned int seq_force_screen_content_tools;
    unsigned int seq_force_integer_mv;
    bool seq_choose_integer_mv;
    unsigned int order_hint_bits_minus_1;
    unsigned int OrderHintBits;

    bool enable_superres;
    bool enable_cdef;
    bool enable_restoration;

    struct color_config_struct
    {
      bool parse_color_config(parserCommon::reader_helper &reader, int seq_profile);

      bool high_bitdepth;
      bool twelve_bit;
      int BitDepth;
      bool mono_chrome;
      int NumPlanes;
      bool color_description_present_flag;

      enum color_primaries_enum
      {
        CP_BT_709 = 1,
        CP_UNSPECIFIED,
        CP_BT_470_M = 4,
        CP_BT_470_B_G,
        CP_BT_601,
        CP_SMPTE_240,
        CP_GENERIC_FILM,
        CP_BT_2020,
        CP_XYZ,
        CP_SMPTE_431,
        CP_SMPTE_432,
        CP_EBU_3213 = 22
      };
      color_primaries_enum color_primaries;

      enum transfer_characteristics_enum
      {
        TC_RESERVED_0 = 0,
        TC_BT_709,
        TC_UNSPECIFIED,
        TC_RESERVED_3,
        TC_BT_470_M,
        TC_BT_470_B_G,
        TC_BT_601,
        TC_SMPTE_240,
        TC_LINEAR,
        TC_LOG_100,
        TC_LOG_100_SQRT10,
        TC_IEC_61966,
        TC_BT_1361,
        TC_SRGB,
        TC_BT_2020_10_BIT,
        TC_BT_2020_12_BIT,
        TC_SMPTE_2084,
        TC_SMPTE_428,
        TC_HLG
      };
      transfer_characteristics_enum transfer_characteristics;

      enum matrix_coefficients_enum
      {
        MC_IDENTITY = 0,
        MC_BT_709,
        MC_UNSPECIFIED,
        MC_RESERVED_3,
        MC_FCC,
        MC_BT_470_B_G,
        MC_BT_601,
        MC_SMPTE_240,
        MC_SMPTE_YCGCO,
        MC_BT_2020_NCL,
        MC_BT_2020_CL,
        MC_SMPTE_2085,
        MC_CHROMAT_NCL,
        MC_CHROMAT_CL,
        MC_ICTCP
      };
      matrix_coefficients_enum matrix_coefficients;

      bool color_range;
      bool subsampling_x;
      bool subsampling_y;

      enum chroma_sample_position_enum
      {
        CSP_UNKNOWN = 0,
        CSP_VERTICAL,
        CSP_COLOCATED,
        CSP_RESERVED
      };
      chroma_sample_position_enum chroma_sample_position;

      bool separate_uv_delta_q;
    };
    color_config_struct color_config;

    bool film_grain_params_present;
  };

  struct global_decoding_values
  {
    frame_type_enum RefFrameType[8];
    bool RefValid[8];
    unsigned int RefOrderHint[8];
    unsigned int OrderHints[8];
    int RefFrameId[8];

    bool SeenFrameHeader;
    int PrevFrameID;
    int current_frame_id;

    // TODO: These need to be set at the end of the "decoding" process
    int RefUpscaledWidth[8];
    int RefFrameHeight[8];
    int RefRenderWidth[8];
    int RefRenderHeight[8];
  };
  global_decoding_values decValues;

  struct frame_header : obu_unit
  {
    frame_header(const obu_unit &obu) : obu_unit(obu) {};
    bool parse_frame_header(const QByteArray &sequenceHeaderData, parserCommon::TreeItem *root, QSharedPointer<sequence_header> seq_header, global_decoding_values &decValues);

    bool parse_uncompressed_header(parserCommon::reader_helper &reader, QSharedPointer<sequence_header> seq_header, global_decoding_values &decValues);
    void mark_ref_frames(int idLen, QSharedPointer<sequence_header> seq_header, global_decoding_values &decValues);

    bool show_existing_frame;
    unsigned int frame_to_show_map_idx;
    unsigned int refresh_frame_flags;
    unsigned int display_frame_id;

    frame_type_enum frame_type;

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

    bool parse_frame_size(parserCommon::reader_helper &reader, QSharedPointer<sequence_header> seq_header);
    bool parse_frame_size_with_refs(parserCommon::reader_helper &reader, QSharedPointer<sequence_header> seq_header, global_decoding_values &decValues);
    unsigned int frame_width_minus_1;
    unsigned int frame_height_minus_1;
    unsigned int FrameWidth;
    unsigned int FrameHeight;

    bool parse_superres_params(parserCommon::reader_helper &reader, QSharedPointer<sequence_header> seq_header);
    bool use_superres;
    unsigned int coded_denom;
    unsigned int SuperresDenom;
    unsigned int UpscaledWidth;

    void compute_image_size();
    int MiCols;
    int MiRows;

    bool parse_render_size(parserCommon::reader_helper &reader);
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
      bool set_frame_refs(parserCommon::reader_helper &reader, int OrderHintBits, bool enable_order_hint, int last_frame_idx, int gold_frame_idx, int OrderHint, global_decoding_values &decValues);

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

    bool read_interpolation_filter(parserCommon::reader_helper &reader);
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
      bool parse_tile_info(int MiCols, int MiRows, parserCommon::reader_helper &reader, QSharedPointer<sequence_header> seq_header);

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
      bool parse_quantization_params(parserCommon::reader_helper &reader, QSharedPointer<sequence_header> seq_header);
      bool read_delta_q(QString deltaValName, int &delta_q, parserCommon::reader_helper &reader);

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
      bool parse_segmentation_params(int primary_ref_frame, parserCommon::reader_helper &reader);

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
      bool parse_delta_q_params(int base_q_idx, parserCommon::reader_helper &reader);

      unsigned int delta_q_res;
      bool delta_q_present;
    };
    delta_q_params_struct delta_q_params;

    struct delta_lf_params_struct
    {
      bool parse_delta_lf_params(bool delta_q_present, bool allow_intrabc, parserCommon::reader_helper &reader);

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
      bool parse_loop_filter_params(bool CodedLossless, bool allow_intrabc, parserCommon::reader_helper &reader, QSharedPointer<sequence_header> seq_header);

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
      bool parse_cdef_params(bool CodedLossless, bool allow_intrabc, parserCommon::reader_helper &reader, QSharedPointer<sequence_header> seq_header);

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

  };  // struct frame_header

  QSharedPointer<sequence_header> active_sequence_header;
};

#endif // PARSERAV1OBU_H