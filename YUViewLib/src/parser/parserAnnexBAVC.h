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

#ifndef PARSERANNEXBAVC_H
#define PARSERANNEXBAVC_H

#include <QSharedPointer>

#include "parserAnnexB.h"
#include "video/videoHandlerYUV.h"

using namespace YUV_Internals;

// This class knows how to parse the bitrstream of HEVC annexB files
class parserAnnexBAVC : public parserAnnexB
{
  Q_OBJECT
  
public:
  parserAnnexBAVC(QObject *parent = nullptr) : parserAnnexB(parent) { curFrameFileStartEndPos = QUint64Pair(-1, -1); };
  ~parserAnnexBAVC() {};

  // Get properties
  double getFramerate() const Q_DECL_OVERRIDE;
  QSize getSequenceSizeSamples() const Q_DECL_OVERRIDE;
  yuvPixelFormat getPixelFormat() const Q_DECL_OVERRIDE;

  bool parseAndAddNALUnit(int nalID, QByteArray data, parserCommon::BitrateItemModel *bitrateModel, parserCommon::TreeItem *parent=nullptr, QUint64Pair nalStartEndPosFile = QUint64Pair(-1,-1), QString *nalTypeName=nullptr) Q_DECL_OVERRIDE;

  QList<QByteArray> getSeekFrameParamerSets(int iFrameNr, uint64_t &filePos) Q_DECL_OVERRIDE;
  QByteArray getExtradata() Q_DECL_OVERRIDE;
  QPair<int,int> getProfileLevel() Q_DECL_OVERRIDE;
  QPair<int,int> getSampleAspectRatio() Q_DECL_OVERRIDE;

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
    nal_unit_avc(QUint64Pair filePosStartEnd, int nal_idx) : nal_unit(filePosStartEnd, nal_idx) {}
    nal_unit_avc(QSharedPointer<nal_unit_avc> nal_src) : nal_unit(nal_src->filePosStartEnd, nal_src->nal_idx) { nal_ref_idc = nal_src->nal_ref_idc; nal_unit_type = nal_src->nal_unit_type; }
    virtual ~nal_unit_avc() {}

    // Parse the parameter set from the given data bytes. If a parserCommon::TreeItem pointer is provided, the values will be added to the tree as well.
    bool parse_nal_unit_header(const QByteArray &header_byte, parserCommon::TreeItem *root) Q_DECL_OVERRIDE;

    bool isSlice()        { return nal_unit_type >= CODED_SLICE_NON_IDR && nal_unit_type <= CODED_SLICE_IDR; }
    virtual QByteArray getNALHeader() const override;
    virtual bool isParameterSet() const override { return nal_unit_type == SPS || nal_unit_type == PPS; }

    /// The information of the NAL unit header
    unsigned int nal_ref_idc {0};
    nal_unit_type_enum nal_unit_type {UNSPECIFIED};
  };

  // The sequence parameter set.
  struct sps : nal_unit_avc
  {
    sps(const nal_unit_avc &nal) : nal_unit_avc(nal) {};
    bool parse_sps(const QByteArray &parameterSetData, parserCommon::TreeItem *root);

    unsigned int profile_idc;
    bool constraint_set0_flag;
    bool constraint_set1_flag;
    bool constraint_set2_flag;
    bool constraint_set3_flag;
    bool constraint_set4_flag;
    bool constraint_set5_flag;
    unsigned int reserved_zero_2bits;
    unsigned int level_idc;
    unsigned int seq_parameter_set_id;
    unsigned int chroma_format_idc {1};
    bool separate_colour_plane_flag {false};
    unsigned int bit_depth_luma_minus8 {0};
    unsigned int bit_depth_chroma_minus8 {0};
    bool qpprime_y_zero_transform_bypass_flag {false};
    bool seq_scaling_matrix_present_flag {false};
    bool seq_scaling_list_present_flag[8];
    int ScalingList4x4[6][16];
    bool UseDefaultScalingMatrix4x4Flag[6];
    int ScalingList8x8[6][64];
    bool UseDefaultScalingMatrix8x8Flag[2];
    unsigned int log2_max_frame_num_minus4;
    unsigned int pic_order_cnt_type;
    unsigned int log2_max_pic_order_cnt_lsb_minus4;
    bool delta_pic_order_always_zero_flag;
    int offset_for_non_ref_pic;
    int offset_for_top_to_bottom_field;
    unsigned int num_ref_frames_in_pic_order_cnt_cycle;
    int offset_for_ref_frame[256];
    unsigned int max_num_ref_frames;
    bool gaps_in_frame_num_value_allowed_flag;
    unsigned pic_width_in_mbs_minus1;
    unsigned pic_height_in_map_units_minus1;
    bool frame_mbs_only_flag;
    bool mb_adaptive_frame_field_flag {false};
    bool direct_8x8_inference_flag;
    bool frame_cropping_flag;
    unsigned int frame_crop_left_offset {0};
    unsigned int frame_crop_right_offset {0};
    unsigned int frame_crop_top_offset {0};
    unsigned int frame_crop_bottom_offset {0};
    bool vui_parameters_present_flag;

    struct vui_parameters_struct
    {
      bool parse_vui(parserCommon::reader_helper &reader, int BitDeptYC, int BitDepthC, int chroma_format_idc, bool frame_mbs_only_flag);

      bool aspect_ratio_info_present_flag;
      unsigned int aspect_ratio_idc {0};
      unsigned int sar_width;
      unsigned int sar_height;

      bool overscan_info_present_flag;
      bool overscan_appropriate_flag;

      bool video_signal_type_present_flag;
      unsigned int video_format {5};
      bool video_full_range_flag {false};
      bool colour_description_present_flag;
      unsigned int colour_primaries {2};
      unsigned int transfer_characteristics {2};
      unsigned int matrix_coefficients {2};

      bool chroma_loc_info_present_flag;
      unsigned int chroma_sample_loc_type_top_field {0};
      unsigned int chroma_sample_loc_type_bottom_field {0};

      bool timing_info_present_flag;
      unsigned int num_units_in_tick;
      unsigned int time_scale;
      bool fixed_frame_rate_flag {false};

      struct hrd_parameters_struct
      {
        bool parse_hrd(parserCommon::reader_helper &reader);

        unsigned int cpb_cnt_minus1;
        unsigned int bit_rate_scale;
        unsigned int cpb_size_scale;
        QList<quint32> bit_rate_value_minus1;
        QList<quint32> cpb_size_value_minus1;
        QList<bool> cbr_flag;
        unsigned int initial_cpb_removal_delay_length_minus1 {23};
        unsigned int cpb_removal_delay_length_minus1;
        unsigned int dpb_output_delay_length_minus1;
        unsigned int time_offset_length {24};
      };
      hrd_parameters_struct nal_hrd;
      hrd_parameters_struct vcl_hrd;

      bool nal_hrd_parameters_present_flag {false};
      bool vcl_hrd_parameters_present_flag {false};
      bool low_delay_hrd_flag;
      bool pic_struct_present_flag;
      bool bitstream_restriction_flag;
      bool motion_vectors_over_pic_boundaries_flag;
      unsigned int max_bytes_per_pic_denom;
      unsigned int max_bits_per_mb_denom;
      unsigned int log2_max_mv_length_horizontal;
      unsigned int log2_max_mv_length_vertical;
      unsigned int max_num_reorder_frames;
      unsigned int max_dec_frame_buffering;

      // The following values are not read from the bitstream but are calculated from the read values.
      double frameRate;
    };
    vui_parameters_struct vui_parameters;

    // The following values are not read from the bitstream but are calculated from the read values.
    unsigned int BitDepthY;
    unsigned int QpBdOffsetY;
    unsigned int BitDepthC;
    unsigned int QpBdOffsetC;
    unsigned int PicWidthInMbs;
    unsigned int FrameHeightInMbs;
    unsigned int PicHeightInMbs;
    unsigned int PicHeightInMapUnits;
    unsigned int PicSizeInMbs;
    unsigned int PicSizeInMapUnits;
    unsigned int SubWidthC;
    unsigned int SubHeightC;
    unsigned int MbHeightC;
    unsigned int MbWidthC;
    unsigned int PicHeightInSamplesL;
    unsigned int PicHeightInSamplesC;
    unsigned int PicWidthInSamplesL;
    unsigned int PicWidthInSamplesC;
    unsigned int ChromaArrayType;
    unsigned int CropUnitX;
    unsigned int CropUnitY;
    unsigned int PicCropLeftOffset;
    int PicCropWidth;
    unsigned int PicCropTopOffset;
    int PicCropHeight;
    bool MbaffFrameFlag;
    unsigned int MaxPicOrderCntLsb {0};
    int ExpectedDeltaPerPicOrderCntCycle {0};
    unsigned int MaxFrameNum;
  };
  typedef QMap<int, QSharedPointer<sps>> sps_map;

  // The picture parameter set.
  struct pps : nal_unit_avc
  {
    pps(const nal_unit_avc &nal) : nal_unit_avc(nal) {};
    bool parse_pps(const QByteArray &parameterSetData, parserCommon::TreeItem *root, const sps_map &active_SPS_list);

    unsigned int pic_parameter_set_id;
    unsigned int seq_parameter_set_id;
    bool entropy_coding_mode_flag;
    bool bottom_field_pic_order_in_frame_present_flag;
    unsigned int num_slice_groups_minus1;
    unsigned int slice_group_map_type;
    unsigned int run_length_minus1[8];
    unsigned int top_left[8];
    unsigned int bottom_right[8];
    bool slice_group_change_direction_flag;
    unsigned int slice_group_change_rate_minus1;
    unsigned int pic_size_in_map_units_minus1;
    QList<unsigned int> slice_group_id;
    unsigned int num_ref_idx_l0_default_active_minus1;
    unsigned int num_ref_idx_l1_default_active_minus1;
    bool weighted_pred_flag;
    unsigned int weighted_bipred_idc;
    int pic_init_qp_minus26;
    int pic_init_qs_minus26;
    int chroma_qp_index_offset;
    bool deblocking_filter_control_present_flag;
    bool constrained_intra_pred_flag;
    bool redundant_pic_cnt_present_flag;

    bool transform_8x8_mode_flag {false};
    bool pic_scaling_matrix_present_flag {false};
    int second_chroma_qp_index_offset;
    bool pic_scaling_list_present_flag[12];

    int ScalingList4x4[6][16];
    bool UseDefaultScalingMatrix4x4Flag[6];
    int ScalingList8x8[6][64];
    bool UseDefaultScalingMatrix8x8Flag[2];

    // The following values are not read from the bitstream but are calculated from the read values.
    int SliceGroupChangeRate;
  };
  typedef QMap<int, QSharedPointer<pps>> pps_map;

  // A slice NAL unit.
  struct slice_header : nal_unit_avc
  {
    slice_header(const nal_unit_avc &nal) : nal_unit_avc(nal) {};
    bool parse_slice_header(const QByteArray &sliceHeaderData, const sps_map &active_SPS_list, const pps_map &active_PPS_list, QSharedPointer<slice_header> prev_pic, parserCommon::TreeItem *root);
    bool isRandomAccess() { return (nal_unit_type == CODED_SLICE_IDR || slice_type == SLICE_I); }
    QString getSliceTypeString() const;

    enum slice_type_enum
    {
      SLICE_P,
      SLICE_B,
      SLICE_I,
      SLICE_SP,
      SLICE_SI
    };

    unsigned int first_mb_in_slice;
    unsigned int slice_type_id;
    unsigned int pic_parameter_set_id;
    unsigned int colour_plane_id;
    unsigned int frame_num;
    bool field_pic_flag {false};
    bool bottom_field_flag {false};
    unsigned int idr_pic_id;
    unsigned int pic_order_cnt_lsb;
    int delta_pic_order_cnt_bottom {0};
    int delta_pic_order_cnt[2];
    unsigned int redundant_pic_cnt {0};
    bool direct_spatial_mv_pred_flag;
    bool num_ref_idx_active_override_flag;
    unsigned int num_ref_idx_l0_active_minus1;
    unsigned int num_ref_idx_l1_active_minus1;

    struct ref_pic_list_mvc_modification_struct
    {
      bool parse_ref_pic_list_mvc_modification(parserCommon::reader_helper &reader, slice_type_enum slicy_type);

      bool ref_pic_list_modification_flag_l0;
      QList<unsigned int> modification_of_pic_nums_idc_l0;
      QList<unsigned int> abs_diff_pic_num_minus1_l0;
      QList<unsigned int> long_term_pic_num_l0;
      QList<unsigned int> abs_diff_view_idx_minus1_l0;

      bool ref_pic_list_modification_flag_l1;
      QList<int> modification_of_pic_nums_idc_l1;
      QList<int> abs_diff_pic_num_minus1_l1;
      QList<int> long_term_pic_num_l1;
      QList<int> abs_diff_view_idx_minus1_l1;
    };
    ref_pic_list_mvc_modification_struct ref_pic_list_mvc_modification;

    struct ref_pic_list_modification_struct
    {
      bool parse_ref_pic_list_modification(parserCommon::reader_helper &reader, slice_type_enum slicy_type);

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
      bool parse_pred_weight_table(parserCommon::reader_helper & reader, slice_type_enum slicy_type, int ChromaArrayType, int num_ref_idx_l0_active_minus1, int num_ref_idx_l1_active_minus1);

      unsigned int luma_log2_weight_denom;
      unsigned int chroma_log2_weight_denom;
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
      bool parse_dec_ref_pic_marking(parserCommon::reader_helper & reader, bool IdrPicFlag);

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

    unsigned int cabac_init_idc;
    int slice_qp_delta;
    bool sp_for_switch_flag;
    int slice_qs_delta;
    unsigned int disable_deblocking_filter_idc;
    int slice_alpha_c0_offset_div2;
    int slice_beta_offset_div2;
    unsigned int slice_group_change_cycle;

    // These values are not parsed from the slice header but are calculated
    // from the parsed values.
    int IdrPicFlag;
    slice_type_enum slice_type;
    bool slice_type_fixed;  // slice_type_id is > 4 For pic_order_cnt_type == 0
    int firstMacroblockAddressInSlice;
    int prevPicOrderCntMsb {-1};
    int prevPicOrderCntLsb {-1};
    int PicOrderCntMsb {-1};
    // For pic_order_cnt_type == 1
    int FrameNumOffset {-1};

    int TopFieldOrderCnt {-1};
    int BottomFieldOrderCnt {-1};

    // This value is not defined in the standard. We just keep on counting the 
    // POC up from where we started parsing the bitstream to get a "global" POC.
    int globalPOC;
    int globalPOC_highestGlobalPOCLastGOP {-1};
    int globalPOC_lastIDR;
  };

  struct sei : nal_unit_avc
  {
    sei(const nal_unit_avc &nal) : nal_unit_avc(nal) {}
    sei(QSharedPointer<sei> sei_src) : nal_unit_avc(sei_src) { payloadType = sei_src->payloadType; last_payload_type_byte = sei_src->last_payload_type_byte; payloadSize = sei_src->payloadSize; last_payload_size_byte = sei_src->last_payload_size_byte; payloadTypeName = sei_src->payloadTypeName; }
    // Parse SEI header (type, length) and return how many bytes were read
    int parse_sei_header(QByteArray &sliceHeaderData, parserCommon::TreeItem *root);
    // If parsing of a special SEI is not implemented, this function can just parse/show the raw bytes.
    sei_parsing_return_t parser_sei_bytes(QByteArray &data, parserCommon::TreeItem *root);

    int payloadType;
    int last_payload_type_byte;
    int payloadSize;
    int last_payload_size_byte;
    QString payloadTypeName;
  };

  class buffering_period_sei : public sei
  {
  public:
    buffering_period_sei(QSharedPointer<sei> sei_src) : sei(sei_src) {};
    // Parsing might return SEI_PARSING_WAIT_FOR_PARAMETER_SETS if the referenced SPS was not found (yet).
    // In this case we have to parse this SEI once the VPS was recieved (which should happen at the beginning of the bitstream).
    sei_parsing_return_t parse_buffering_period_sei(QByteArray &data, const sps_map &active_SPS_list, parserCommon::TreeItem *root);
    void reparse_buffering_period_sei(const sps_map &active_SPS_list) { parse(active_SPS_list, true); }

    unsigned int seq_parameter_set_id;
    QList<unsigned int> initial_cpb_removal_delay;
    QList<unsigned int> initial_cpb_removal_delay_offset;

  private:
    // These are used internally when parsing of the SEI must be prosponed until the SPS is received.
    bool parse(const sps_map &active_SPS_list, bool reparse);
    parserCommon::TreeItem *itemTree;
    QByteArray sei_data_storage;
  };

  class pic_timing_sei : public sei
  {
  public:
    pic_timing_sei(QSharedPointer<sei> sei_src) : sei(sei_src) {};
    // Parsing might return SEI_PARSING_WAIT_FOR_PARAMETER_SETS if the referenced SPS was not found (yet).
    // In this case we have to parse this SEI once the VPS was recieved (which should happen at the beginning of the bitstream).
    sei_parsing_return_t parse_pic_timing_sei(QByteArray &data, const sps_map &active_SPS_list, bool CpbDpbDelaysPresentFlag, parserCommon::TreeItem *root);
    void reparse_pic_timing_sei(const sps_map &active_SPS_list, bool CpbDpbDelaysPresentFlag) { parse(active_SPS_list, CpbDpbDelaysPresentFlag, true); }

    unsigned int cpb_removal_delay;
    unsigned int dpb_output_delay;

    unsigned int pic_struct;
    QList<bool> clock_timestamp_flag;
    unsigned int ct_type[3];
    bool nuit_field_based_flag[3];
    unsigned int counting_type[3];
    bool full_timestamp_flag[3];
    bool discontinuity_flag[3];
    bool cnt_dropped_flag[3];
    unsigned int n_frames[3];
    unsigned int seconds_value[3];
    unsigned int minutes_value[3];
    unsigned int hours_value[3];
    bool seconds_flag[3];
    bool minutes_flag[3];
    bool hours_flag[3];
    unsigned int time_offset[3];

  private:
    // These are used internally when parsing of the SEI must be prosponed until the SPS is received.
    bool parse(const sps_map &active_SPS_list, bool CpbDpbDelaysPresentFlag, bool reparse);
    parserCommon::TreeItem *itemTree;
    QByteArray sei_data_storage;
  };

  class user_data_sei : sei
  {
  public:
    user_data_sei(QSharedPointer<sei> sei_src) : sei(sei_src) {};
    sei_parsing_return_t parse_user_data_sei(QByteArray &sliceHeaderData, parserCommon::TreeItem *root) { return parse_internal(sliceHeaderData, root) ? SEI_PARSING_OK : SEI_PARSING_ERROR; }

    QString user_data_UUID;
    QString user_data_message;
  private:
    bool parse_internal(QByteArray &sliceHeaderData, parserCommon::TreeItem *root);
  };

  static bool read_scaling_list(parserCommon::reader_helper &reader, int *scalingList, int sizeOfScalingList, bool *useDefaultScalingMatrixFlag);

  // When we start to parse the bitstream we will remember the first RAP POC
  // so that we can disregard any possible RASL pictures.
  int firstPOCRandomAccess {INT_MAX};

  // These maps hold the last active VPS, SPS and PPS. This is required for parsing
  // the parameter sets.
  QMap<int, QSharedPointer<sps>> active_SPS_list;
  QMap<int, QSharedPointer<pps>> active_PPS_list;
  // In order to calculate POCs we need the first slice of the last reference picture
  QSharedPointer<slice_header> last_picture_first_slice;
  // It is allowed that units (like SEI messages) sent before the parameter sets but still refer to the 
  // parameter sets. Here we keep a list of seis that need to be parsed after the parameter sets were recieved.
  QList<QSharedPointer<sei>> reparse_sei;

  // In an SEI, the number of bytes indicated do not consider the emulation prevention. This function
  // can determine the real number of bytes that we need to read from the input considering the emulation prevention
  int determineRealNumberOfBytesSEIEmulationPrevention(QByteArray &in, int nrBytes);

  bool CpbDpbDelaysPresentFlag {false};

  // For every frame, we save the file position where the NAL unit of the first slice starts and where the NAL of the last slice ends.
  // This is used by getNextFrameNALUnits to return all information (NAL units) for a specific frame.
  QUint64Pair curFrameFileStartEndPos;  //< Save the file start/end position of the current frame (in case the frame has multiple NAL units)
  // The POC of the current frame. We save this when we encounter a NAL from the next POC; then we add it.
  int curFramePOC {-1};
  bool curFrameIsRandomAccess {false};
  
  struct auDelimiterDetector_t
  {
    bool isStartOfNewAU(nal_unit_avc &nal_avc, int curFramePOC);
    nal_unit_type_enum lastNalType {UNSPECIFIED};
    int lastNalSlicePoc {-1};
    bool delimiterPresent {false};
  };
  auDelimiterDetector_t auDelimiterDetector;

  unsigned int sizeCurrentAU {0};
  int lastFramePOC{-1};
  int counterAU {0};
  bool currentAUAllSlicesIntra {true};
  QString currentAUAllSliceTypes;
};

#endif // PARSERANNEXBAVC_H
