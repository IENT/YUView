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
  fileSourceAVCAnnexBFile() : fileSourceAnnexBFile() { firstPOCRandomAccess = INT_MAX; }
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
  enum nal_unit_type
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
    nal_unit_avc(quint64 filePos, int nal_idx) : nal_unit(filePos, nal_idx), nal_type(UNSPECIFIED) {}
    virtual ~nal_unit_avc() {}

    // Parse the parameter set from the given data bytes. If a TreeItem pointer is provided, the values will be added to the tree as well.
    void parse_nal_unit_header(const QByteArray &parameterSetData, TreeItem *root) Q_DECL_OVERRIDE;

    bool isRandomAccess() { return nal_type == CODED_SLICE_IDR; }
    bool isSlice()        { return nal_type >= CODED_SLICE_NON_IDR && nal_type <= CODED_SLICE_IDR; }
    virtual QByteArray getNALHeader() const override;
    virtual bool isParameterSet() const override { return nal_type == SPS || nal_type == PPS; }
    
    /// The information of the NAL unit header
    int nal_ref_idc;
    nal_unit_type nal_type;
  };

  // The sequence parameter set.
  struct sps : nal_unit_avc
  {
    sps(const nal_unit_avc &nal);
    void parse_sps(const QByteArray &parameterSetData, TreeItem *root);
    void read_scaling_list(sub_byte_reader &reader, int *scalingList, int sizeOfScalingList, bool *useDefaultScalingMatrixFlag, TreeItem *root);
    void read_vui_parameters(sub_byte_reader &reader, TreeItem *root);

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
    bool residual_colour_transform_flag;
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
  };

  // The picture parameter set.
  struct pps : nal_unit_avc
  {
    pps(const nal_unit_avc &nal);
    void parse_pps(const QByteArray &parameterSetData, TreeItem *root);
  };

  // A slice NAL unit.
  struct slice : nal_unit_avc
  {
    slice(const nal_unit_avc &nal);
    void parse_slice(const QByteArray &sliceHeaderData, const QMap<int, sps*> &p_active_SPS_list, const QMap<int, pps*> &p_active_PPS_list, slice *firstSliceInSegment, TreeItem *root);
  };

  struct sei : nal_unit_avc
  {
    sei(const nal_unit_avc &nal) : nal_unit_avc(nal) {}
    void parse_sei_message(const QByteArray &sliceHeaderData, TreeItem *root);

    int payloadType;
    int last_payload_type_byte;
    int payloadSize;
    int last_payload_size_byte;
  };

  void parseAndAddNALUnit(int nalID) Q_DECL_OVERRIDE;

  // When we start to parse the bitstream we will remember the first RAP POC
  // so that we can disregard any possible RASL pictures.
  int firstPOCRandomAccess;

  // These maps hold the last active VPS, SPS and PPS. This is required for parsing
  // the parameter sets.
  QMap<int, sps*> active_SPS_list;
  QMap<int, pps*> active_PPS_list;
};

#endif //FILESOURCEHEVCANNEXBFILE_H
