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

#ifndef FILESOURCEHEVCANNEXBFILE_H
#define FILESOURCEHEVCANNEXBFILE_H

#include <QAbstractItemModel>
#include <QMap>
#include "fileSourceAnnexBFile.h"
#include "videoHandlerYUV.h"

using namespace YUV_Internals;

class fileSourceAVCAnnexBFile : public fileSourceAnnexBFile
{
  Q_OBJECT

public:
  fileSourceAVCAnnexBFile() : fileSourceAnnexBFile() { firstPOCRandomAccess = INT_MAX; CpbDpbDelaysPresentFlag = false; }
  ~fileSourceAVCAnnexBFile() {}

  // What it the framerate?
  double getFramerate() const;
  // What is the sequence resolution?
  QSize getSequenceSizeSamples() const;
  // What is the chroma format?
  YUVSubsamplingType getSequenceSubsampling() const;
  // What is the bit depth of the output?
  int getSequenceBitDepth(Component c) const;

  // Seek the file to the given frame number. The given frame number has to be a random 
  // access point. We can start decoding the file from here. Use getClosestSeekableFrameNumber to find a random access point.
  // Returns the active parameter sets as a byte array. This has to be given to the decoder first.
  // The fileSourceAnnexBFile can also seekt to a certain frame number. But, we know more about parameter sets and will only
  // return the active parameter sets.
  QList<QByteArray> seekToFrameNumber(int iFrameNr) Q_DECL_OVERRIDE;

protected:
  // ----- Some nested classes that are only used in the scope of this file handler class

  // All the different NAL unit types (T-REC-H.265-201504 Page 85)
  enum nal_unit_type_enum
  {
    UNSPECIFIED,
    CODED_SLICE_NON_IDR,
    CODED_SLICE_DATA_PARTITION_A,
    CODED_SLICE_DATA_PARTITION_B,
    CODED_SLICE_DATA_PARTITION_C,
    CODED_SLICE_IDR,
    SEI,
    SPS,
    PPS,
    AUD,
    END_OF_SEQUENCE,
    END_OF_STREAM,
    FILLER,
    SPS_EXT,
    PREFIX_NAL,
    SUBSET_SPS,
    DEPTH_PARAMETER_SET,
    RESERVED_17,
    RESERVED_18,
    CODED_SLICE_AUX,
    CODED_SLICE_EXTENSION,
    CODED_SLICE_EXTENSION_DEPTH_MAP,
    RESERVED_22, RESERVED_23,
    UNSPCIFIED_24, UNSPCIFIED_25, UNSPCIFIED_26, UNSPCIFIED_27, UNSPCIFIED_28, UNSPCIFIED_29, UNSPCIFIED_30, UNSPCIFIED_31
  };
  static const QStringList nal_unit_type_toString;

  /* The basic HEVC NAL unit. Additionally to the basic NAL unit, it knows the HEVC nal unit types.
  */
  struct nal_unit_avc : nal_unit
  {
    nal_unit_avc(quint64 filePos, int nal_idx) : nal_unit(filePos, nal_idx), nal_unit_type(UNSPECIFIED) {}
    nal_unit_avc(QSharedPointer<nal_unit_avc> nal_src) : nal_unit(nal_src->filePos, nal_src->nal_idx) { nal_ref_idc = nal_src->nal_ref_idc; nal_unit_type = nal_src->nal_unit_type; }
    virtual ~nal_unit_avc() {}

    // Parse the parameter set from the given data bytes. If a TreeItem pointer is provided, the values will be added to the tree as well.
    void parse_nal_unit_header(const QByteArray &parameterSetData, TreeItem *root) Q_DECL_OVERRIDE;

    bool isRandomAccess() { return nal_unit_type == CODED_SLICE_IDR; }
    bool isSlice()        { return nal_unit_type >= CODED_SLICE_NON_IDR && nal_unit_type <= CODED_SLICE_IDR; }
    virtual QByteArray getNALHeader() const override;
    virtual bool isParameterSet() const override { return nal_unit_type == SPS || nal_unit_type == PPS; }
    
    /// The information of the NAL unit header
    int nal_ref_idc;
    nal_unit_type_enum nal_unit_type;
  };

  // The sequence parameter set.
  struct sps : nal_unit_avc
  {
    sps(const nal_unit_avc &nal);
    void parse_sps(const QByteArray &parameterSetData, TreeItem *root);

    int profile_idc;
    bool constraint_set0_flag;
    bool constraint_set1_flag;
    bool constraint_set2_flag;
    bool constraint_set3_flag;
    bool constraint_set4_flag;
    bool constraint_set5_flag;
    int reserved_zero_2bits;
    int level_idc;
    int seq_parameter_set_id;
    int chroma_format_idc;
    bool separate_colour_plane_flag;
    int bit_depth_luma_minus8;
    int bit_depth_chroma_minus8;
    bool qpprime_y_zero_transform_bypass_flag;
    bool seq_scaling_matrix_present_flag;
    bool seq_scaling_list_present_flag[8];
    int ScalingList4x4[6][16];
    bool UseDefaultScalingMatrix4x4Flag[6];
    int ScalingList8x8[6][64];
    bool UseDefaultScalingMatrix8x8Flag[2];
    int log2_max_frame_num_minus4;
    int pic_order_cnt_type;
    int log2_max_pic_order_cnt_lsb_minus4;
    bool delta_pic_order_always_zero_flag;
    int offset_for_non_ref_pic;
    int offset_for_top_to_bottom_field;
    int num_ref_frames_in_pic_order_cnt_cycle;
    int offset_for_ref_frame[256];
    int max_num_ref_frames;
    bool gaps_in_frame_num_value_allowed_flag;
    int pic_width_in_mbs_minus1;
    int pic_height_in_map_units_minus1;
    bool frame_mbs_only_flag;
    bool mb_adaptive_frame_field_flag;
    bool direct_8x8_inference_flag;
    bool frame_cropping_flag;
    int frame_crop_left_offset;
    int frame_crop_right_offset;
    int frame_crop_top_offset;
    int frame_crop_bottom_offset;
    bool vui_parameters_present_flag;

    struct vui_parameters_struct
    {
      vui_parameters_struct();
      void read(sub_byte_reader &reader, TreeItem *root, int BitDeptYC, int BitDepthC, int chroma_format_idc);

      bool aspect_ratio_info_present_flag;
      int aspect_ratio_idc;
      int sar_width;
      int sar_height;

      bool overscan_info_present_flag;
      bool overscan_appropriate_flag;

      bool video_signal_type_present_flag;
      int video_format;
      bool video_full_range_flag;
      bool colour_description_present_flag;
      int colour_primaries;
      int transfer_characteristics;
      int matrix_coefficients;

      bool chroma_loc_info_present_flag;
      int chroma_sample_loc_type_top_field;
      int chroma_sample_loc_type_bottom_field;

      bool timing_info_present_flag;
      int num_units_in_tick;
      int time_scale;
      bool fixed_frame_rate_flag;

      struct hrd_parameters_struct
      {
        hrd_parameters_struct();
        void read(sub_byte_reader &reader, TreeItem *root);

        int cpb_cnt_minus1;
        int bit_rate_scale;
        int cpb_size_scale;
        QList<quint32> bit_rate_value_minus1;
        QList<quint32> cpb_size_value_minus1;
        QList<bool> cbr_flag;
        int initial_cpb_removal_delay_length_minus1;
        int cpb_removal_delay_length_minus1;
        int dpb_output_delay_length_minus1;
        int time_offset_length;
      };
      hrd_parameters_struct nal_hrd;
      hrd_parameters_struct vcl_hrd;

      bool nal_hrd_parameters_present_flag;
      bool vcl_hrd_parameters_present_flag;
      bool low_delay_hrd_flag;
      bool pic_struct_present_flag;
      bool bitstream_restriction_flag;
      bool motion_vectors_over_pic_boundaries_flag;
      int max_bytes_per_pic_denom;
      int max_bits_per_mb_denom;
      int log2_max_mv_length_horizontal;
      int log2_max_mv_length_vertical;
      int max_num_reorder_frames;
      int max_dec_frame_buffering;
    };
    vui_parameters_struct vui_parameters;

    // The following values are not read from the bitstream but are calculated from the read values.
    int BitDepthY;
    int QpBdOffsetY;
    int BitDepthC;
    int QpBdOffsetC;
    int PicWidthInMbs;
    int FrameHeightInMbs;
    int PicHeightInMbs;
    int PicHeightInMapUnits;
    int PicSizeInMbs;
    int PicSizeInMapUnits;
    int ChromaArrayType;
    bool MbaffFrameFlag;
    int MaxPicOrderCntLsb;
    int ExpectedDeltaPerPicOrderCntCycle;
    int MaxFrameNum;
  };

  // The picture parameter set.
  struct pps : nal_unit_avc
  {
    pps(const nal_unit_avc &nal);
    void parse_pps(const QByteArray &parameterSetData, TreeItem *root, const QMap<int, QSharedPointer<sps>> &p_active_SPS_list);

    int pic_parameter_set_id;
    int seq_parameter_set_id;
    bool entropy_coding_mode_flag;
    bool bottom_field_pic_order_in_frame_present_flag;
    int num_slice_groups_minus1;
    int slice_group_map_type;
    int run_length_minus1[8];
    int top_left[8];
    int bottom_right[8];
    bool slice_group_change_direction_flag;
    int slice_group_change_rate_minus1;
    int pic_size_in_map_units_minus1;
    QList<int> slice_group_id;
    int num_ref_idx_l0_default_active_minus1;
    int num_ref_idx_l1_default_active_minus1;
    bool weighted_pred_flag;
    int weighted_bipred_idc;
    int pic_init_qp_minus26;
    int pic_init_qs_minus26;
    int chroma_qp_index_offset;
    bool deblocking_filter_control_present_flag;
    bool constrained_intra_pred_flag;
    bool redundant_pic_cnt_present_flag;
    
    bool transform_8x8_mode_flag;
    bool pic_scaling_matrix_present_flag;
    int second_chroma_qp_index_offset;
    bool pic_scaling_list_present_flag[12];

    int ScalingList4x4[6][16];
    bool UseDefaultScalingMatrix4x4Flag[6];
    int ScalingList8x8[6][64];
    bool UseDefaultScalingMatrix8x8Flag[2];

    // The following values are not read from the bitstream but are calculated from the read values.
    int SliceGroupChangeRate;
  };

  // A slice NAL unit.
  struct slice_header : nal_unit_avc
  {
    slice_header(const nal_unit_avc &nal);
    void parse_slice_header(const QByteArray &sliceHeaderData, const QMap<int, QSharedPointer<sps>> &p_active_SPS_list, const QMap<int, QSharedPointer<pps>> &p_active_PPS_list, QSharedPointer<slice_header> prev_pic, TreeItem *root);

    enum slice_type_enum
    {
      SLICE_P,
      SLICE_B,
      SLICE_I,
      SLICE_SP,
      SLICE_SI
    };

    int first_mb_in_slice;
    int slice_type_id;
    int pic_parameter_set_id;
    int colour_plane_id;
    int frame_num;
    bool field_pic_flag;
    bool bottom_field_flag;
    int idr_pic_id;
    int pic_order_cnt_lsb;
    int delta_pic_order_cnt_bottom;
    int delta_pic_order_cnt[2];
    int redundant_pic_cnt;
    bool direct_spatial_mv_pred_flag;
    bool num_ref_idx_active_override_flag;
    int num_ref_idx_l0_active_minus1;
    int num_ref_idx_l1_active_minus1;

    struct ref_pic_list_mvc_modification_struct
    {
      void read(sub_byte_reader &reader, TreeItem *itemTree, slice_type_enum slicy_type);

      bool ref_pic_list_modification_flag_l0;
      QList<int> modification_of_pic_nums_idc_l0;
      QList<int> abs_diff_pic_num_minus1_l0;
      QList<int> long_term_pic_num_l0;
      QList<int> abs_diff_view_idx_minus1_l0;

      bool ref_pic_list_modification_flag_l1;
      QList<int> modification_of_pic_nums_idc_l1;
      QList<int> abs_diff_pic_num_minus1_l1;
      QList<int> long_term_pic_num_l1;
      QList<int> abs_diff_view_idx_minus1_l1;
    };
    ref_pic_list_mvc_modification_struct ref_pic_list_mvc_modification;

    struct ref_pic_list_modification_struct
    {
      void read(sub_byte_reader &reader, TreeItem *itemTree, slice_type_enum slicy_type);

      bool ref_pic_list_modification_flag_l0;
      QList<int> modification_of_pic_nums_idc_l0;
      QList<int> abs_diff_pic_num_minus1_l0;
      QList<int> long_term_pic_num_l0;

      bool ref_pic_list_modification_flag_l1;
      QList<int> modification_of_pic_nums_idc_l1;
      QList<int> abs_diff_pic_num_minus1_l1;
      QList<int> long_term_pic_num_l1;
    };
    ref_pic_list_modification_struct ref_pic_list_modification;

    struct pred_weight_table_struct
    {
      void read(sub_byte_reader & reader, TreeItem * itemTree, slice_type_enum slicy_type, int ChromaArrayType, int num_ref_idx_l0_active_minus1, int num_ref_idx_l1_active_minus1);

      int luma_log2_weight_denom;
      int chroma_log2_weight_denom;
      QList<bool> luma_weight_l0_flag_list;
      QList<int> luma_weight_l0;
      QList<int> luma_offset_l0;
      QList<bool> chroma_weight_l0_flag_list;
      QList<int> chroma_weight_l0[2];
      QList<int> chroma_offset_l0[2];
      QList<bool> luma_weight_l1_flag_list;
      QList<int> luma_weight_l1;
      QList<int> luma_offset_l1;
      QList<bool> chroma_weight_l1_flag_list;
      QList<int> chroma_weight_l1[2];
      QList<int> chroma_offset_l1[2];
    };
    pred_weight_table_struct pred_weight_table;

    struct dec_ref_pic_marking_struct
    {
      void read(sub_byte_reader & reader, TreeItem * itemTree, bool IdrPicFlag);

      bool no_output_of_prior_pics_flag;
      bool long_term_reference_flag;
      bool adaptive_ref_pic_marking_mode_flag;
      QList<int> memory_management_control_operation_list;
      QList<int> difference_of_pic_nums_minus1;
      QList<int> long_term_pic_num;
      QList<int> long_term_frame_idx;
      QList<int> max_long_term_frame_idx_plus1;
    };
    dec_ref_pic_marking_struct dec_ref_pic_marking;
    
    int cabac_init_idc;
    int slice_qp_delta;
    bool sp_for_switch_flag;
    int slice_qs_delta;
    int disable_deblocking_filter_idc;
    int slice_alpha_c0_offset_div2;
    int slice_beta_offset_div2;
    int slice_group_change_cycle;

    // These values are not parsed from the slice header but are calculated
    // from the parsed values.
    int IdrPicFlag;
    slice_type_enum slice_type;
    bool slice_type_fixed;  // slice_type_id is > 4
    // For pic_order_cnt_type == 0
    int prevPicOrderCntMsb;
    int prevPicOrderCntLsb;
    int PicOrderCntMsb;
    // For pic_order_cnt_type == 1
    int FrameNumOffset;

    int TopFieldOrderCnt;
    int BottomFieldOrderCnt;

    // This value is not defined in the standard. We just keep on counting the 
    // POC up from where we started parsing the bitstream to get a "global" POC.
    int globalPOC;
    int globalPOC_highestGlobalPOCLastGOP;
    int globalPOC_lastIDR;
  };

  struct sei : nal_unit_avc
  {
    sei(const nal_unit_avc &nal) : nal_unit_avc(nal) {}
    sei(QSharedPointer<sei> sei_src) : nal_unit_avc(sei_src) { payloadType = sei_src->payloadType; last_payload_type_byte = sei_src->last_payload_type_byte; payloadSize = sei_src->payloadSize; last_payload_size_byte = sei_src->last_payload_size_byte; payloadTypeName = sei_src->payloadTypeName; }
    // Parse SEI message and return how many bytes were read
    int parse_sei_message(QByteArray &sliceHeaderData, TreeItem *root);

    int payloadType;
    int last_payload_type_byte;
    int payloadSize;
    int last_payload_size_byte;
    QString payloadTypeName;
  };

  struct buffering_period_sei : sei
  {
    buffering_period_sei(QSharedPointer<sei> sei_src) : sei(sei_src) {};
    void parse_buffering_period_sei(QByteArray &sliceHeaderData, const QMap<int, QSharedPointer<sps>> &p_active_SPS_list, TreeItem *root);

    int seq_parameter_set_id;
    QList<int> initial_cpb_removal_delay;
    QList<int> initial_cpb_removal_delay_offset;
  };

  struct pic_timing_sei : sei
  {
    pic_timing_sei(QSharedPointer<sei> sei_src) : sei(sei_src) {};
    void parse_pic_timing_sei(QByteArray &sliceHeaderData, const QMap<int, QSharedPointer<sps>> &p_active_SPS_list, bool CpbDpbDelaysPresentFlag, TreeItem *root);

    int cpb_removal_delay;
    int dpb_output_delay;

    int pic_struct;
    QList<bool> clock_timestamp_flag;
    int ct_type[3];
    bool nuit_field_based_flag[3];
    int counting_type[3];
    bool full_timestamp_flag[3];
    bool discontinuity_flag[3];
    bool cnt_dropped_flag[3];
    int n_frames[3];
    int seconds_value[3];
    int minutes_value[3];
    int hours_value[3];
    bool seconds_flag[3];
    bool minutes_flag[3];
    bool hours_flag[3];
    int time_offset[3];
  };

  struct user_data_sei : sei
  {
    user_data_sei(QSharedPointer<sei> sei_src) : sei(sei_src) {};
    void parse_user_data_sei(QByteArray &sliceHeaderData, TreeItem *root);

    QString user_data_UUID;
    QString user_data_message;
  };

  static void read_scaling_list(sub_byte_reader &reader, int *scalingList, int sizeOfScalingList, bool *useDefaultScalingMatrixFlag, TreeItem *itemTree);

  void parseAndAddNALUnit(int nalID) Q_DECL_OVERRIDE;

  // When we start to parse the bitstream we will remember the first RAP POC
  // so that we can disregard any possible RASL pictures.
  int firstPOCRandomAccess;

  // These maps hold the last active VPS, SPS and PPS. This is required for parsing
  // the parameter sets.
  QMap<int, QSharedPointer<sps>> active_SPS_list;
  QMap<int, QSharedPointer<pps>> active_PPS_list;

  // In order to calculate POCs we need the first slice of the last reference picture
  QSharedPointer<slice_header> last_picture_first_slice;

  bool CpbDpbDelaysPresentFlag;
};

#endif //FILESOURCEHEVCANNEXBFILE_H
