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

#ifndef PARSERANNEXBMPEG2_H
#define PARSERANNEXBMPEG2_H

#include "parserAnnexB.h"

class parserAnnexBMpeg2 : public parserAnnexB
{
  Q_OBJECT
  
public:
  parserAnnexBMpeg2(QObject *parent = nullptr) : parserAnnexB(parent) {};
  ~parserAnnexBMpeg2() {};

  // Get properties
  double getFramerate() const Q_DECL_OVERRIDE;
  QSize getSequenceSizeSamples() const Q_DECL_OVERRIDE;
  yuvPixelFormat getPixelFormat() const Q_DECL_OVERRIDE;

  bool parseAndAddNALUnit(int nalID, QByteArray data, parserCommon::BitrateItemModel *bitrateModel, parserCommon::TreeItem *parent=nullptr, QUint64Pair nalStartEndPosFile = QUint64Pair(-1,-1), QString *nalTypeName=nullptr) Q_DECL_OVERRIDE;

  // TODO: Reading from raw mpeg2 streams not supported (yet? Is this even defined / possible?)
  QList<QByteArray> getSeekFrameParamerSets(int iFrameNr, uint64_t &filePos) Q_DECL_OVERRIDE { Q_UNUSED(iFrameNr); Q_UNUSED(filePos); return QList<QByteArray>(); }
  QByteArray getExtradata() Q_DECL_OVERRIDE { return QByteArray(); }
  QPair<int,int> getProfileLevel() Q_DECL_OVERRIDE;
  QPair<int,int> getSampleAspectRatio() Q_DECL_OVERRIDE;

private:

  // All the different NAL unit types (T-REC-H.262-199507 Page 24 Table 6-1)
  enum nal_unit_type_enum
  {
    UNSPECIFIED,
    PICTURE,
    SLICE,
    USER_DATA,
    SEQUENCE_HEADER,
    SEQUENCE_ERROR,
    EXTENSION_START,
    SEQUENCE_END,
    GROUP_START,
    SYSTEM_START_CODE,
    RESERVED
  };
  static const QStringList nal_unit_type_toString;

  /* The basic Mpeg2 NAL unit. Technically, there is no concept of NAL units in mpeg2 (h262) but there are start codes for some units
   * and there is a start code so we internally use the NAL concept.
   */
  struct nal_unit_mpeg2 : nal_unit
  {
    nal_unit_mpeg2(QUint64Pair filePosStartEnd, int nal_idx) : nal_unit(filePosStartEnd, nal_idx) {}
    nal_unit_mpeg2(QSharedPointer<nal_unit_mpeg2> nal_src);
    virtual ~nal_unit_mpeg2() {}

    // Parse the parameter set from the given data bytes. If a parserCommon::TreeItem pointer is provided, the values will be added to the tree as well.
    bool parse_nal_unit_header(const QByteArray &header_byte, parserCommon::TreeItem *root) Q_DECL_OVERRIDE;

    virtual QByteArray getNALHeader() const override;
    virtual bool isParameterSet() const override { return nal_unit_type == SEQUENCE_HEADER; }

    QStringList get_start_code_meanings();
    void interpreteStartCodeValue();

    nal_unit_type_enum nal_unit_type {UNSPECIFIED};
    unsigned int slice_id {0};         // in case of SLICE
    unsigned system_start_codes {0};   // in case of SYSTEM_START_CODE
    unsigned int start_code_value {0}; 
  };

  struct sequence_header : nal_unit_mpeg2
  {
    sequence_header(const nal_unit_mpeg2 &nal) : nal_unit_mpeg2(nal) {}
    bool parse_sequence_header(const QByteArray &parameterSetData, parserCommon::TreeItem *root);

    unsigned int sequence_header_code;
    unsigned int horizontal_size_value;
    unsigned int vertical_size_value;
    unsigned int aspect_ratio_information;
    unsigned int frame_rate_code;
    unsigned int bit_rate_value;
    bool marker_bit;
    unsigned int vbv_buffer_size_value;
    bool constrained_parameters_flag;
    bool load_intra_quantiser_matrix;
    unsigned int intra_quantiser_matrix[64];
    bool load_non_intra_quantiser_matrix;
    unsigned int non_intra_quantiser_matrix[64];
  };

  struct picture_header : nal_unit_mpeg2
  {
    picture_header(const nal_unit_mpeg2 &nal) : nal_unit_mpeg2(nal) {}
    bool parse_picture_header(const QByteArray &parameterSetData, parserCommon::TreeItem *root);
    bool isIntraPicture() const { return picture_coding_type == 1; };
    QString getPictureTypeString() const;

    unsigned int temporal_reference;
    unsigned int picture_coding_type;
    unsigned int vbv_delay;
    bool full_pel_forward_vector;
    unsigned int forward_f_code;
    bool full_pel_backward_vector;
    unsigned int backward_f_code;
    QList<int> extra_information_picture_list;
  };

  struct group_of_pictures_header : nal_unit_mpeg2
  {
    group_of_pictures_header(const nal_unit_mpeg2 &nal) : nal_unit_mpeg2(nal) {};
    bool parse_group_of_pictures_header(const QByteArray &parameterSetData, parserCommon::TreeItem *root);

    unsigned int time_code;
    bool closed_gop;
    bool broken_link;
  };

  struct user_data : nal_unit_mpeg2
  {
    user_data(const nal_unit_mpeg2 &nal) : nal_unit_mpeg2(nal) {}
    bool parse_user_data(const QByteArray &parameterSetData, parserCommon::TreeItem *root);
  };

  enum nal_extension_type
  {
    EXT_SEQUENCE,
    EXT_SEQUENCE_DISPLAY,
    EXT_QUANT_MATRIX,
    EXT_COPYRIGHT,
    EXT_SEQUENCE_SCALABLE,
    EXT_PICTURE_DISPLAY,
    EXT_PICTURE_CODING,
    EXT_PICTURE_SPATICAL_SCALABLE,
    EXT_PICTURE_TEMPORAL_SCALABLE,
    EXT_RESERVED
  };

  struct nal_extension : nal_unit_mpeg2
  {
    nal_extension(const nal_unit_mpeg2 &nal) : nal_unit_mpeg2(nal) {}
    nal_extension(QSharedPointer<nal_extension> src) : nal_unit_mpeg2(src) { extension_start_code_identifier = src->extension_start_code_identifier; extension_type = src->extension_type; }
    // Peek the extension start code identifier (4 bits) in the payload
    bool parse_extension_start_code(QByteArray &extension_payload, parserCommon::TreeItem *root);

    unsigned int extension_start_code_identifier;
    nal_extension_type extension_type;
    QString get_extension_function_name();
  };

  struct sequence_extension : nal_extension
  {
    sequence_extension(const nal_extension &nal) : nal_extension(nal) {}
    bool parse_sequence_extension(const QByteArray &parameterSetData, parserCommon::TreeItem *root);

    unsigned int profile_and_level_indication;
    bool progressive_sequence;
    unsigned int chroma_format;
    unsigned int horizontal_size_extension;
    unsigned int vertical_size_extension;
    unsigned int bit_rate_extension;
    bool marker_bit;
    unsigned int vbv_buffer_size_extension;
    bool low_delay;
    unsigned int frame_rate_extension_n {0};
    unsigned int frame_rate_extension_d {0};

    unsigned int profile_identification;
    unsigned int level_identification;
  };

  struct picture_coding_extension : nal_extension
  {
    picture_coding_extension(const nal_extension &nal) : nal_extension(nal) {}
    bool parse_picture_coding_extension(const QByteArray &parameterSetData, parserCommon::TreeItem *root);

    unsigned int f_code[2][2];
    unsigned int intra_dc_precision;
    unsigned int picture_structure;
    bool top_field_first;
    bool frame_pred_frame_dct;
    bool concealment_motion_vectors;
    bool q_scale_type;
    bool intra_vlc_format;
    bool alternate_scan;
    bool repeat_first_field;
    bool chroma_420_type;
    bool progressive_frame;
    bool composite_display_flag;
    bool v_axis;
    unsigned int field_sequence;
    bool sub_carrier;
    unsigned int burst_amplitude;
    unsigned int sub_carrier_phase;
  };

  // We will keep a pointer to the first sequence extension to be able to retrive some data
  QSharedPointer<sequence_extension> first_sequence_extension;
  QSharedPointer<sequence_header> first_sequence_header;

  unsigned int sizeCurrentAU{ 0 };
  int pocOffset{ 0 };
  int curFramePOC{ -1 };
  int lastFramePOC{ -1 };
  unsigned int counterAU{ 0 };
  bool lastAUStartBySequenceHeader{ false };
  bool currentAUAllSlicesIntra {true};
  QString currentAUAllSliceTypes;
  QSharedPointer<picture_header> lastPictureHeader;
};

#endif // PARSERANNEXBMPEG2_H