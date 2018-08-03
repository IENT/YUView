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

#ifndef FILESOURCEAVCANNEXBFILE_H
#define FILESOURCEAVCANNEXBFILE_H

#include <QAbstractItemModel>
#include <QMap>
#include "fileSourceAnnexBFile.h"
#include "videoHandlerYUV.h"

using namespace YUV_Internals;

class fileSourceHEVCAnnexBFile : public fileSourceAnnexBFile
{
  Q_OBJECT

public:
  fileSourceHEVCAnnexBFile() : fileSourceAnnexBFile() { firstPOCRandomAccess = INT_MAX; pocCounterOffset = 0; maxPOCCount = -1; }
  ~fileSourceHEVCAnnexBFile() {}

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
  enum nal_unit_type
  {
    TRAIL_N,        TRAIL_R,     TSA_N,          TSA_R,          STSA_N,      STSA_R,      RADL_N,     RADL_R,     RASL_N,     RASL_R, 
    RSV_VCL_N10,    RSV_VCL_N12, RSV_VCL_N14,    RSV_VCL_R11,    RSV_VCL_R13, RSV_VCL_R15, BLA_W_LP,   BLA_W_RADL, BLA_N_LP,   IDR_W_RADL,
    IDR_N_LP,       CRA_NUT,     RSV_IRAP_VCL22, RSV_IRAP_VCL23, RSV_VCL24,   RSV_VCL25,   RSV_VCL26,  RSV_VCL27,  RSV_VCL28,  RSV_VCL29,
    RSV_VCL30,      RSV_VCL31,   VPS_NUT,        SPS_NUT,        PPS_NUT,     AUD_NUT,     EOS_NUT,    EOB_NUT,    FD_NUT,     PREFIX_SEI_NUT,
    SUFFIX_SEI_NUT, RSV_NVCL41,  RSV_NVCL42,     RSV_NVCL43,     RSV_NVCL44,  RSV_NVCL45,  RSV_NVCL46, RSV_NVCL47, UNSPECIFIED
  };
  static const QStringList nal_unit_type_toString;

  /* The basic HEVC NAL unit. Additionally to the basic NAL unit, it knows the HEVC nal unit types.
  */
  struct nal_unit_hevc : nal_unit
  {
    nal_unit_hevc(quint64 filePos, int nal_idx) : nal_unit(filePos, nal_idx), nal_type(UNSPECIFIED) {}
    nal_unit_hevc(QSharedPointer<nal_unit_hevc> nal_src) : nal_unit(nal_src->filePos, nal_src->nal_idx) { nal_type = nal_src->nal_type; nuh_layer_id = nal_src->nuh_layer_id; nuh_temporal_id_plus1 = nal_src->nuh_temporal_id_plus1; }
    virtual ~nal_unit_hevc() {}

    virtual QByteArray getNALHeader() const override;
    virtual bool isParameterSet() const override { return nal_type == VPS_NUT || nal_type == SPS_NUT || nal_type == PPS_NUT; }

    // Parse the parameter set from the given data bytes. If a TreeItem pointer is provided, the values will be added to the tree as well.
    void parse_nal_unit_header(const QByteArray &parameterSetData, TreeItem *root) Q_DECL_OVERRIDE;

    bool isIRAP();
    bool isSLNR();
    bool isRADL();
    bool isRASL();
    bool isSlice();
    
    // The information of the NAL unit header
    nal_unit_type nal_type;
    int nuh_layer_id;
    int nuh_temporal_id_plus1;
  };

  // The profile tier level syntax elements. 7.3.3
  struct profile_tier_level
  {
    void parse_profile_tier_level(sub_byte_reader &reader, bool profilePresentFlag, int maxNumSubLayersMinus1, TreeItem *root);

    int general_profile_space;
    bool general_tier_flag;
    int general_profile_idc;
    bool general_profile_compatibility_flag[32];
    bool general_progressive_source_flag;
    bool general_interlaced_source_flag;
    bool general_non_packed_constraint_flag;
    bool general_frame_only_constraint_flag;

    bool general_max_12bit_constraint_flag;
    bool general_max_10bit_constraint_flag;
    bool general_max_8bit_constraint_flag;
    bool general_max_422chroma_constraint_flag;
    bool general_max_420chroma_constraint_flag;
    bool general_max_monochrome_constraint_flag;
    bool general_intra_constraint_flag;
    bool general_one_picture_only_constraint_flag;
    bool general_lower_bit_rate_constraint_flag;
    int general_reserved_zero_bits;
    bool general_inbld_flag;
    bool general_reserved_zero_bit;  // general_reserved_zero_34bits or general_reserved_zero_43bits

    int general_level_idc;

    // A maximum of 8 sub-layer are allowed
    bool sub_layer_profile_present_flag[8];
    bool sub_layer_level_present_flag[8];
    int  reserved_zero_2bits[8];
    int  sub_layer_profile_space[8];
    bool sub_layer_tier_flag[8];
    int  sub_layer_profile_idc[8];
    bool sub_layer_profile_compatibility_flag[8][32];
    bool sub_layer_progressive_source_flag[8];
    bool sub_layer_interlaced_source_flag[8];
    bool sub_layer_non_packed_constraint_flag[8];
    bool sub_layer_frame_only_constraint_flag[8];
    bool sub_layer_max_12bit_constraint_flag[8];
    bool sub_layer_max_10bit_constraint_flag[8];
    bool sub_layer_max_8bit_constraint_flag[8];
    bool sub_layer_max_422chroma_constraint_flag[8];
    bool sub_layer_max_420chroma_constraint_flag[8];
    bool sub_layer_max_monochrome_constraint_flag[8];
    bool sub_layer_intra_constraint_flag[8];
    bool sub_layer_one_picture_only_constraint_flag[8];
    bool sub_layer_lower_bit_rate_constraint_flag[8];
    int  sub_layer_reserved_zero_bits[8]; // sub_layer_reserved_zero_34bits or sub_layer_reserved_zero_43bits
    bool sub_layer_inbld_flag[8];
    bool sub_layer_reserved_zero_bit[8];
    int  sub_layer_level_idc[8];
  };

  // E.2.3 Sub-layer HRD parameters syntax
  struct sub_layer_hrd_parameters
  {
    void parse_sub_layer_hrd_parameters(sub_byte_reader &reader, int subLayerId, int CpbCnt, bool sub_pic_hrd_params_present_flag, TreeItem *root);

    QList<int> bit_rate_value_minus1;
    QList<int> cpb_size_value_minus1;
    QList<int> cpb_size_du_value_minus1;
    QList<int> bit_rate_du_value_minus1;
    QList<bool> cbr_flag;
  };

  // E.2.2 HRD parameters syntax
  struct hrd_parameters
  {
    hrd_parameters();
    void parse_hrd_parameters(sub_byte_reader &reader, bool commonInfPresentFlag, int maxNumSubLayersMinus1, TreeItem *root);

    bool nal_hrd_parameters_present_flag = 0;
    bool vcl_hrd_parameters_present_flag = 0;
    bool sub_pic_hrd_params_present_flag = 0;

    int tick_divisor_minus2;
    int du_cpb_removal_delay_increment_length_minus1;
    int sub_pic_cpb_params_in_pic_timing_sei_flag = 0;
    int dpb_output_delay_du_length_minus1;

    int bit_rate_scale;
    int cpb_size_scale;
    int cpb_size_du_scale;
    int initial_cpb_removal_delay_length_minus1 = 23;
    int au_cpb_removal_delay_length_minus1      = 23;
    int dpb_output_delay_length_minus1          = 23;

    bool fixed_pic_rate_general_flag[8]    = {0, 0, 0, 0, 0, 0, 0, 0};
    bool fixed_pic_rate_within_cvs_flag[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    int elemental_duration_in_tc_minus1[8];
    bool low_delay_hrd_flag[8]             = {0, 0, 0, 0, 0, 0, 0, 0};
    int cpb_cnt_minus1[8]                  = {0, 0, 0, 0, 0, 0, 0, 0};

    sub_layer_hrd_parameters nal_sub_hrd[8];
    sub_layer_hrd_parameters vcl_sub_hrd[8];
  };

  // 7.3.4 Scaling list data syntax
  struct scaling_list_data
  {
    void parse_scaling_list_data(sub_byte_reader &reader, TreeItem *root);

    bool scaling_list_pred_mode_flag[4][6];
    int scaling_list_pred_matrix_id_delta[4][6];
    int scaling_list_dc_coef_minus8[2][6];
  };

  // 7.3.6.3 Weighted prediction parameters syntax
  struct sps;
  struct slice;
  struct pred_weight_table
  {
    void parse_pred_weight_table(sub_byte_reader &reader, sps *actSPS, slice *actSlice, TreeItem *root);

    int luma_log2_weight_denom;
    int delta_chroma_log2_weight_denom;
    QList<bool> luma_weight_l0_flag;
    QList<bool> chroma_weight_l0_flag;
    QList<int> delta_luma_weight_l0;
    QList<int> luma_offset_l0;
    QList<int> delta_chroma_weight_l0;
    QList<int> delta_chroma_offset_l0;
    
    QList<bool> luma_weight_l1_flag;
    QList<bool> chroma_weight_l1_flag;
    QList<int> delta_luma_weight_l1;
    QList<int> luma_offset_l1;
    QList<int> delta_chroma_weight_l1;
    QList<int> delta_chroma_offset_l1;
  };

  // 7.3.7 Short-term reference picture set syntax
  struct st_ref_pic_set
  {
    void parse_st_ref_pic_set(sub_byte_reader &reader, int stRpsIdx, sps *actSPS, TreeItem *root);
    int NumPicTotalCurr(int CurrRpsIdx, slice *actSlice);

    bool inter_ref_pic_set_prediction_flag;
    int delta_idx_minus1;
    bool delta_rps_sign;
    int abs_delta_rps_minus1;
    QList<bool> used_by_curr_pic_flag;
    QList<bool> use_delta_flag;

    int num_negative_pics;
    int num_positive_pics;
    QList<int> delta_poc_s0_minus1;
    QList<bool> used_by_curr_pic_s0_flag;
    QList<int> delta_poc_s1_minus1;
    QList<bool> used_by_curr_pic_s1_flag;

    // Calculated values. These are static. They are used for reference picture set prediction.
    static int NumNegativePics[65];
    static int NumPositivePics[65];
    static int DeltaPocS0[65][16];
    static int DeltaPocS1[65][16];
    static bool UsedByCurrPicS0[65][16];
    static bool UsedByCurrPicS1[65][16];
    static int NumDeltaPocs[65];
  };

  struct vui_parameters
  {
    vui_parameters();
    void parse_vui_parameters(sub_byte_reader &reader, sps *actSPS, TreeItem *root);

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
    int matrix_coeffs;
    bool chroma_loc_info_present_flag;
    int chroma_sample_loc_type_top_field;
    int chroma_sample_loc_type_bottom_field;
    bool neutral_chroma_indication_flag;
    bool field_seq_flag;
    bool frame_field_info_present_flag;
    bool default_display_window_flag;
    int def_disp_win_left_offset;
    int def_disp_win_right_offset;
    int def_disp_win_top_offset;
    int def_disp_win_bottom_offset;
    
    bool vui_timing_info_present_flag;
    int vui_num_units_in_tick;
    int vui_time_scale;
    bool vui_poc_proportional_to_timing_flag;
    int vui_num_ticks_poc_diff_one_minus1;
    bool vui_hrd_parameters_present_flag;
    hrd_parameters vui_hrd_parameters;

    bool bitstream_restriction_flag;
    bool tiles_fixed_structure_flag;
    bool motion_vectors_over_pic_boundaries_flag;
    bool restricted_ref_pic_lists_flag;
    int min_spatial_segmentation_idc;
    int max_bytes_per_pic_denom;
    int max_bits_per_min_cu_denom;
    int log2_max_mv_length_horizontal;
    int log2_max_mv_length_vertical;

    // Calculated values
    double frameRate;
  };

  struct ref_pic_lists_modification
  {
    void parse_ref_pic_lists_modification(sub_byte_reader &reader, slice *actSlice, int NumPicTotalCurr, TreeItem *root);

    bool ref_pic_list_modification_flag_l0;
    QList<int> list_entry_l0;
    bool ref_pic_list_modification_flag_l1;
    QList<int> list_entry_l1;
  };
    
  // The video parameter set. 7.3.2.1
  struct vps : nal_unit_hevc
  {
    vps(const nal_unit_hevc &nal) : nal_unit_hevc(nal), vps_timing_info_present_flag(false), frameRate(0.0) {}

    void parse_vps(const QByteArray &parameterSetData, TreeItem *root);

    int vps_video_parameter_set_id;     /// vps ID
    bool vps_base_layer_internal_flag;
    bool vps_base_layer_available_flag;
    int vps_max_layers_minus1;          /// How many layers are there. Is this a scalable bitstream?
    int vps_max_sub_layers_minus1;
    bool vps_temporal_id_nesting_flag;
    int vps_reserved_0xffff_16bits;

    profile_tier_level ptl;

    bool vps_sub_layer_ordering_info_present_flag;
    bool vps_max_dec_pic_buffering_minus1[7];
    bool vps_max_num_reorder_pics[7];
    bool vps_max_latency_increase_plus1[7];
    int vps_max_layer_id;
    int vps_num_layer_sets_minus1;
    QList<bool> layer_id_included_flag[7];

    bool vps_timing_info_present_flag;
    int vps_num_units_in_tick;
    int vps_time_scale;
    bool vps_poc_proportional_to_timing_flag;
    int vps_num_ticks_poc_diff_one_minus1;
    int vps_num_hrd_parameters;
    QList<int>  hrd_layer_set_idx;
    QList<bool> cprms_present_flag;

    QList<hrd_parameters> vps_hrd_parameters;
    bool vps_extension_flag;

    // Calculated values
    double frameRate;
  };

  // The sequence parameter set.
  struct sps : nal_unit_hevc
  {
    sps(const nal_unit_hevc &nal);
    void parse_sps(const QByteArray &parameterSetData, TreeItem *root);

    int sps_video_parameter_set_id;
    int sps_max_sub_layers_minus1;
    bool sps_temporal_id_nesting_flag;
    profile_tier_level ptl;
    
    int sps_seq_parameter_set_id;
    int chroma_format_idc;
    int separate_colour_plane_flag;
    int pic_width_in_luma_samples;
    int pic_height_in_luma_samples;
    int conformance_window_flag;
  
    int conf_win_left_offset;
    int conf_win_right_offset;
    int conf_win_top_offset;
    int conf_win_bottom_offset;
  
    int bit_depth_luma_minus8;
    int bit_depth_chroma_minus8;
    int log2_max_pic_order_cnt_lsb_minus4;
    bool sps_sub_layer_ordering_info_present_flag;
    QList<int> sps_max_dec_pic_buffering_minus1;
    QList<int> sps_max_num_reorder_pics;
    QList<int> sps_max_latency_increase_plus1;

    int log2_min_luma_coding_block_size_minus3;
    int log2_diff_max_min_luma_coding_block_size;
    int log2_min_luma_transform_block_size_minus2;
    int log2_diff_max_min_luma_transform_block_size;
    int max_transform_hierarchy_depth_inter;
    int max_transform_hierarchy_depth_intra;
    bool scaling_list_enabled_flag;
    bool sps_scaling_list_data_present_flag;
    scaling_list_data sps_scaling_list_data;

    bool amp_enabled_flag;
    bool sample_adaptive_offset_enabled_flag;
    bool pcm_enabled_flag;
    int pcm_sample_bit_depth_luma_minus1;
    int pcm_sample_bit_depth_chroma_minus1;
    int log2_min_pcm_luma_coding_block_size_minus3;
    int log2_diff_max_min_pcm_luma_coding_block_size;
    bool pcm_loop_filter_disabled_flag;
    int num_short_term_ref_pic_sets;
    QList<st_ref_pic_set> sps_st_ref_pic_sets;
    bool long_term_ref_pics_present_flag;
    int num_long_term_ref_pics_sps;
    QList<int> lt_ref_pic_poc_lsb_sps;
    QList<bool> used_by_curr_pic_lt_sps_flag;

    bool sps_temporal_mvp_enabled_flag;
    bool strong_intra_smoothing_enabled_flag;
    
    bool vui_parameters_present_flag;
    vui_parameters sps_vui_parameters;

    bool sps_extension_present_flag;
    bool sps_range_extension_flag;
    bool sps_multilayer_extension_flag;
    bool sps_3d_extension_flag;
    int sps_extension_5bits;

    // Calculated values
    int ChromaArrayType;
    int SubWidthC, SubHeightC;
    int MinCbLog2SizeY, CtbLog2SizeY, CtbSizeY, PicWidthInCtbsY, PicHeightInCtbsY, PicSizeInCtbsY;  // 7.4.3.2.1
  
    // Get the actual size of the image that will be returned. Internally the image might be bigger.
    int get_conformance_cropping_width() const { return (pic_width_in_luma_samples - (SubWidthC * conf_win_right_offset) - SubWidthC * conf_win_left_offset); }
    int get_conformance_cropping_height() const { return (pic_height_in_luma_samples - (SubHeightC * conf_win_bottom_offset) - SubHeightC * conf_win_top_offset); }
  };

  struct pps;
  struct pps_range_extension
  {
    pps_range_extension();
    void parse_pps_range_extension(sub_byte_reader &reader, pps *actPPS, TreeItem *root);

    int log2_max_transform_skip_block_size_minus2;
    bool cross_component_prediction_enabled_flag;
    bool chroma_qp_offset_list_enabled_flag;
    int diff_cu_chroma_qp_offset_depth;
    int chroma_qp_offset_list_len_minus1;
    QList<int> cb_qp_offset_list;
    QList<int> cr_qp_offset_list;
    int log2_sao_offset_scale_luma;
    int log2_sao_offset_scale_chroma;
  };

  typedef QMap<int, QSharedPointer<vps>> vps_map;
  typedef QMap<int, QSharedPointer<sps>> sps_map;
  typedef QMap<int, QSharedPointer<pps>> pps_map;

  // The picture parameter set.
  struct pps : nal_unit_hevc
  {
    pps(const nal_unit_hevc &nal);
    void parse_pps(const QByteArray &parameterSetData, TreeItem *root);
    
    int pps_pic_parameter_set_id;
    int pps_seq_parameter_set_id;
    bool dependent_slice_segments_enabled_flag;
    bool output_flag_present_flag;
    int num_extra_slice_header_bits;
    bool sign_data_hiding_enabled_flag;
    bool cabac_init_present_flag;
    int num_ref_idx_l0_default_active_minus1;
    int num_ref_idx_l1_default_active_minus1;
    int init_qp_minus26;
    bool constrained_intra_pred_flag;
    bool transform_skip_enabled_flag;
    bool cu_qp_delta_enabled_flag;
    int diff_cu_qp_delta_depth;
    int pps_cb_qp_offset;
    int pps_cr_qp_offset;
    bool pps_slice_chroma_qp_offsets_present_flag;
    bool weighted_pred_flag;
    bool weighted_bipred_flag;
    bool transquant_bypass_enabled_flag;
    bool tiles_enabled_flag;
    bool entropy_coding_sync_enabled_flag;
    int num_tile_columns_minus1;
    int num_tile_rows_minus1;
    bool uniform_spacing_flag;
    QList<int> column_width_minus1;
    QList<int> row_height_minus1;
    bool loop_filter_across_tiles_enabled_flag;
    bool pps_loop_filter_across_slices_enabled_flag;
    bool deblocking_filter_control_present_flag;
    bool deblocking_filter_override_enabled_flag;
    bool pps_deblocking_filter_disabled_flag;
    int pps_beta_offset_div2;
    int pps_tc_offset_div2;
    bool pps_scaling_list_data_present_flag;
    scaling_list_data pps_scaling_list_data;
    bool lists_modification_present_flag;
    int log2_parallel_merge_level_minus2;
    bool slice_segment_header_extension_present_flag;
    bool pps_extension_present_flag;
    bool pps_range_extension_flag;
    bool pps_multilayer_extension_flag;
    bool pps_3d_extension_flag;
    int pps_extension_5bits;

    pps_range_extension range_extension;
  };

  // A slice NAL unit.
  struct slice : nal_unit_hevc
  {
    slice(const nal_unit_hevc &nal);
    void parse_slice(const QByteArray &sliceHeaderData, const sps_map &p_active_SPS_list, const pps_map &p_active_PPS_list, QSharedPointer<slice> firstSliceInSegment, TreeItem *root);
    virtual int getPOC() const override { return PicOrderCntVal; }
    
    bool first_slice_segment_in_pic_flag;
    bool no_output_of_prior_pics_flag;
    bool dependent_slice_segment_flag;
    int slice_pic_parameter_set_id;
    int slice_segment_address;
    QList<bool> slice_reserved_flag;
    int slice_type;
    bool pic_output_flag;
    int colour_plane_id;
    int slice_pic_order_cnt_lsb;
    bool short_term_ref_pic_set_sps_flag;
    st_ref_pic_set st_rps;
    int short_term_ref_pic_set_idx;
    int num_long_term_sps;
    int num_long_term_pics;
    QList<int> lt_idx_sps;
    QList<int> poc_lsb_lt;
    QList<bool> used_by_curr_pic_lt_flag;
    QList<bool> delta_poc_msb_present_flag;
    QList<int> delta_poc_msb_cycle_lt;
    bool slice_temporal_mvp_enabled_flag;
    bool slice_sao_luma_flag;
    bool slice_sao_chroma_flag;
    bool num_ref_idx_active_override_flag;
    int num_ref_idx_l0_active_minus1;
    int num_ref_idx_l1_active_minus1;

    ref_pic_lists_modification slice_rpl_mod;

    bool mvd_l1_zero_flag;
    bool cabac_init_flag;
    bool collocated_from_l0_flag;
    int collocated_ref_idx;
    pred_weight_table slice_pred_weight_table;
    int five_minus_max_num_merge_cand;
    int slice_qp_delta;
    int slice_cb_qp_offset;
    int slice_cr_qp_offset;
    bool cu_chroma_qp_offset_enabled_flag;
    bool deblocking_filter_override_flag;
    bool slice_deblocking_filter_disabled_flag;
    int slice_beta_offset_div2;
    int slice_tc_offset_div2;
    bool slice_loop_filter_across_slices_enabled_flag;
    
    int num_entry_point_offsets;
    int offset_len_minus1;
    QList<int> entry_point_offset_minus1;

    int slice_segment_header_extension_length;
    QList<int> slice_segment_header_extension_data_byte;

    // Calculated values
    int PicOrderCntVal; // The slice POC
    int PicOrderCntMsb;
    QList<int> UsedByCurrPicLt;
    bool NoRaslOutputFlag;

    int globalPOC;

    // Static variables for keeping track of the decoding order
    static bool bFirstAUInDecodingOrder;
    static int prevTid0Pic_slice_pic_order_cnt_lsb;
    static int prevTid0Pic_PicOrderCntMsb;

  private:
    // We will keep a pointer to the active SPS and PPS
    QSharedPointer<pps> actPPS;
    QSharedPointer<sps> actSPS;
  };

  // The PicOrderCntMsb may be reset to zero for IDR frames. In order to count the global POC, we store the maximum POC.
  int maxPOCCount;
  int pocCounterOffset;

  struct sei : nal_unit_hevc
  {
    sei(const nal_unit_hevc &nal) : nal_unit_hevc(nal) {}
    sei(QSharedPointer<sei> sei_src) : nal_unit_hevc(sei_src) { payloadType = sei_src->payloadType; last_payload_type_byte = sei_src->last_payload_type_byte; payloadSize = sei_src->payloadSize; last_payload_size_byte = sei_src->last_payload_size_byte; payloadTypeName = sei_src->payloadTypeName; }
    // Parse the SEI and return how many bytes were read
    int parse_sei_message(const QByteArray &sliceHeaderData, TreeItem *root);

    int payloadType;
    int last_payload_type_byte;
    int payloadSize;
    int last_payload_size_byte;
    QString payloadTypeName;
  };

  enum sei_parsing_return_t
  {
    SEI_PARSING_OK,                      // Parsing is done
    SEI_PARSING_WAIT_FOR_PARAMETER_SETS  // We have to wait for valid parameter sets before we can parse this SEI
  };

  struct user_data_sei : sei
  {
    user_data_sei(QSharedPointer<sei> sei_src) : sei(sei_src) {};
    void parse_user_data_sei(QByteArray &sliceHeaderData, TreeItem *root);

    QString user_data_UUID;
    QString user_data_message;
  };

  class active_parameter_sets_sei : public sei
  {
  public:
    active_parameter_sets_sei(QSharedPointer<sei> sei_src) : sei(sei_src) {};
    // Parsing might return SEI_PARSING_WAIT_FOR_VPS if the referenced VPS was not found (yet).
    // In this case we have to parse this SEI once the VPS was recieved (which should happen at the beginning of the bitstream).
    sei_parsing_return_t parse_active_parameter_sets_sei(QByteArray &sliceHeaderData, const vps_map &p_active_VPS_list, TreeItem *root);
    void reparse_active_parameter_sets_sei(const vps_map &p_active_VPS_list);

    int active_video_parameter_set_id;
    bool self_contained_cvs_flag;
    bool no_parameter_set_update_flag;
    int num_sps_ids_minus1;
    QList<int> active_seq_parameter_set_id;
    QList<int> layer_sps_idx;

  private:
    // These are used internally when parsing of the SEI must be prosponed until the VPS is received.
    bool parse(const vps_map &p_active_VPS_list, bool reparse);
    TreeItem *itemTree;
    QByteArray sei_data_storage;
  };

  class pic_timing_sei : public sei
  {
  public:
    pic_timing_sei(QSharedPointer<sei> sei_src) : sei(sei_src) {};
    // Parsing might return SEI_PARSING_WAIT_FOR_VPS if the referenced VPS was not found (yet).
    // In this case we have to parse this SEI once the VPS was recieved (which should happen at the beginning of the bitstream).
    sei_parsing_return_t parse_pic_timing_sei(QByteArray &sliceHeaderData, const vps_map &p_active_VPS_list, const sps_map &p_active_SPS_list, TreeItem *root);
    void reparse_pic_timing_sei(const vps_map &p_active_VPS_list, const sps_map &p_active_SPS_list);

    int pic_struct;
    int source_scan_type;
    bool duplicate_flag;

    int au_cpb_removal_delay_minus1;
    int pic_dpb_output_delay;
    int pic_dpb_output_du_delay;
    int num_decoding_units_minus1;
    bool du_common_cpb_removal_delay_flag;
    int du_common_cpb_removal_delay_increment_minus1;
    QList<int> num_nalus_in_du_minus1;
    QList<int> du_cpb_removal_delay_increment_minus1;

  private:
    // These are used internally when parsing of the SEI must be prosponed until the VPS is received.
    bool parse(const vps_map &p_active_VPS_list, const sps_map &p_active_SPS_list, bool reparse);
    TreeItem *itemTree;
    QByteArray sei_data_storage;
  };

  struct alternative_transfer_characteristics_sei : sei
  {
    alternative_transfer_characteristics_sei(QSharedPointer<sei> sei_src) : sei(sei_src) {};
    void parse_alternative_transfer_characteristics_sei(QByteArray &sliceHeaderData, TreeItem *root);

    int preferred_transfer_characteristics;
  };

  // Get the meaning/interpretation mapping of some values
  static QStringList get_colour_primaries_meaning();
  static QStringList get_transfer_characteristics_meaning();
  static QStringList get_matrix_coefficients_meaning();

  void parseAndAddNALUnit(int nalID) Q_DECL_OVERRIDE;

  // When we start to parse the bitstream we will remember the first RAP POC
  // so that we can disregard any possible RASL pictures.
  int firstPOCRandomAccess;

  // These maps hold the last active VPS, SPS and PPS. This is required for parsing
  // the parameter sets.
  vps_map active_VPS_list;
  sps_map active_SPS_list;
  pps_map active_PPS_list;
  // We keept a pointer to the last slice with first_slice_segment_in_pic_flag set. 
  // All following slices with dependent_slice_segment_flag set need this slice to infer some values.
  QSharedPointer<slice> lastFirstSliceSegmentInPic;
  // A list of seis that need to be parsed after the parameter sets were recieved.
  QList<QSharedPointer<sei>> reparse_sei;
};

#endif //FILESOURCEAVCANNEXBFILE_H
