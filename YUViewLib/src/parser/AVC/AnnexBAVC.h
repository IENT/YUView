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

#include <QSharedPointer>

#include "common/ReaderHelper.h"
#include "video/videoHandlerYUV.h"
#include "AnnexB.h"
#include "NalUnit.h"
#include "sequence_parameter_set.h"

using namespace YUV_Internals;

namespace parser
{

// This class knows how to parse the bitrstream of HEVC annexB files
class AnnexBAVC : public AnnexB
{
  Q_OBJECT
  
public:
  AnnexBAVC(QObject *parent = nullptr) : AnnexB(parent) { curFrameFileStartEndPos = pairUint64(-1, -1); };
  ~AnnexBAVC() {};

  // Get properties
  double getFramerate() const override;
  QSize getSequenceSizeSamples() const override;
  yuvPixelFormat getPixelFormat() const override;

  ParseResult parseAndAddNALUnit(int nalID, QByteArray data, std::optional<BitratePlotModel::BitrateEntry> bitrateEntry, std::optional<pairUint64> nalStartEndPosFile={}, TreeItem *parent=nullptr) override;

  QList<QByteArray> getSeekFrameParamerSets(int iFrameNr, uint64_t &filePos) override;
  QByteArray getExtradata() override;
  IntPair getProfileLevel() override;
  Ratio getSampleAspectRatio() override;

protected:
  // ----- Some nested classes that are only used in the scope of this file handler class

  typedef std::map<unsigned, std::shared_ptr<sequence_parameter_set>> sps_map;

  // The picture parameter set.
  struct pps : nal_unit_avc
  {
    pps(const nal_unit_avc &nal) : nal_unit_avc(nal) {};
    bool parse_pps(const QByteArray &parameterSetData, TreeItem *root, const sps_map &active_SPS_list);

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
    bool parse_slice_header(const QByteArray &sliceHeaderData, const sps_map &active_SPS_list, const pps_map &active_PPS_list, QSharedPointer<slice_header> prev_pic, TreeItem *root);
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
      bool parse_ref_pic_list_mvc_modification(ReaderHelper &reader, slice_type_enum slicy_type);

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
      bool parse_ref_pic_list_modification(ReaderHelper &reader, slice_type_enum slicy_type);

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
      bool parse_pred_weight_table(ReaderHelper & reader, slice_type_enum slicy_type, int ChromaArrayType, int num_ref_idx_l0_active_minus1, int num_ref_idx_l1_active_minus1);

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
      bool parse_dec_ref_pic_marking(ReaderHelper & reader, bool IdrPicFlag);

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
    int parse_sei_header(QByteArray &sliceHeaderData, TreeItem *root);
    // If parsing of a special SEI is not implemented, this function can just parse/show the raw bytes.
    sei_parsing_return_t parser_sei_bytes(QByteArray &data, TreeItem *root);

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
    sei_parsing_return_t parse_buffering_period_sei(QByteArray &data, const sps_map &active_SPS_list, TreeItem *root);
    void reparse_buffering_period_sei(const sps_map &active_SPS_list) { parse(active_SPS_list, true); }

    unsigned int seq_parameter_set_id;
    QList<unsigned int> initial_cpb_removal_delay;
    QList<unsigned int> initial_cpb_removal_delay_offset;

  private:
    // These are used internally when parsing of the SEI must be prosponed until the SPS is received.
    bool parse(const sps_map &active_SPS_list, bool reparse);
    TreeItem *itemTree;
    QByteArray sei_data_storage;
  };

  class pic_timing_sei : public sei
  {
  public:
    pic_timing_sei(QSharedPointer<sei> sei_src) : sei(sei_src) {};
    // Parsing might return SEI_PARSING_WAIT_FOR_PARAMETER_SETS if the referenced SPS was not found (yet).
    // In this case we have to parse this SEI once the VPS was recieved (which should happen at the beginning of the bitstream).
    sei_parsing_return_t parse_pic_timing_sei(QByteArray &data, const sps_map &active_SPS_list, bool CpbDpbDelaysPresentFlag, TreeItem *root);
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
    TreeItem *itemTree;
    QByteArray sei_data_storage;
  };

  class user_data_sei : sei
  {
  public:
    user_data_sei(QSharedPointer<sei> sei_src) : sei(sei_src) {};
    sei_parsing_return_t parse_user_data_sei(QByteArray &sliceHeaderData, TreeItem *root) { return parse_internal(sliceHeaderData, root) ? SEI_PARSING_OK : SEI_PARSING_ERROR; }

    QString user_data_UUID;
    QString user_data_message;
  private:
    bool parse_internal(QByteArray &sliceHeaderData, TreeItem *root);
  };

  static bool read_scaling_list(ReaderHelper &reader, int *scalingList, int sizeOfScalingList, bool *useDefaultScalingMatrixFlag);

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

  // When new SEIs come in and they don't initialize the HRD, they are not accessed until the current AU is processed by the HRD.
  QSharedPointer<buffering_period_sei> lastBufferingPeriodSEI;
  QSharedPointer<pic_timing_sei> lastPicTimingSEI;
  QSharedPointer<buffering_period_sei> newBufferingPeriodSEI;
  QSharedPointer<pic_timing_sei> newPicTimingSEI;
  bool nextAUIsFirstAUInBufferingPeriod {false};

  // In an SEI, the number of bytes indicated do not consider the emulation prevention. This function
  // can determine the real number of bytes that we need to read from the input considering the emulation prevention
  int determineRealNumberOfBytesSEIEmulationPrevention(QByteArray &in, int nrBytes);

  bool CpbDpbDelaysPresentFlag {false};

  // For every frame, we save the file position where the NAL unit of the first slice starts and where the NAL of the last slice ends (if known).
  // This is used by getNextFrameNALUnits to return all information (NAL units) for a specific frame.
  std::optional<pairUint64> curFrameFileStartEndPos;  //< Save the file start/end position of the current frame (in case the frame has multiple NAL units)

  // The POC of the current frame. We save this when we encounter a NAL from the next POC; then we add it.
  int curFramePOC {-1};
  bool curFrameIsRandomAccess {false};
  
  struct auDelimiterDetector_t
  {
    bool isStartOfNewAU(nal_unit_avc &nal_avc, int curFramePOC);
    int lastSlicePoc {-1};
    bool primaryCodedPictureInAuEncountered {false};
  };
  auDelimiterDetector_t auDelimiterDetector;

  unsigned int sizeCurrentAU {0};
  int lastFramePOC{-1};
  int counterAU {0};
  bool currentAUAllSlicesIntra {true};
  QMap<QString, unsigned int> currentAUSliceTypes;

  class HRD
  {
  public:
    HRD() = default;
    void addAU(unsigned auBits, unsigned poc, QSharedPointer<sps> const &sps, QSharedPointer<buffering_period_sei> const &lastBufferingPeriodSEI, QSharedPointer<pic_timing_sei> const &lastPicTimingSEI, HRDPlotModel *plotModel);
    void endOfFile(HRDPlotModel *plotModel);
  
    bool isFirstAUInBufferingPeriod {true};
  private:
    typedef long double time_t;

    // We keep a list of frames which will be removed in the future
    struct HRDFrameToRemove
    {
        HRDFrameToRemove(time_t t_r, int bits, int poc)
            : t_r(t_r)
            , bits(bits)
            , poc(poc)
        {}
        time_t t_r;
        unsigned int bits;
        int poc;
    };
    QList<HRDFrameToRemove> framesToRemove;

    // The access unit count (n) for this HRD. The HRD is initialized with au n=0.
    uint64_t au_n {0};
    // Final arrival time (t_af for n minus 1)
    time_t t_af_nm1 {0};
    // t_r,n(nb) is the nominal removal time of the first access unit of the previous buffering period
    time_t t_r_nominal_n_first;

    QList<HRDFrameToRemove> popRemoveFramesInTimeInterval(time_t from, time_t to);
    void addToBufferAndCheck(unsigned bufferAdd, unsigned bufferSize, int poc, time_t t_begin, time_t t_end, HRDPlotModel *plotModel);
    void removeFromBufferAndCheck(const HRDFrameToRemove &frame, int poc, time_t removalTime, HRDPlotModel *plotModel);
    void addConstantBufferLine(int poc, time_t t_begin, time_t t_end, HRDPlotModel *plotModel);

    int64_t decodingBufferLevel {0};
  };
  HRD hrd;
};

} //namespace parser