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

#include "NalUnit.h"

namespace MPEG2
{

enum class ExtensionType
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

struct NalExtension : NalUnit
{
  NalExtension(const NalUnit &nal) : NalUnit(nal) {}
  NalExtension(QSharedPointer<NalExtension> src) :
    NalUnit(src), 
    extensionStartCodeIdentifier(src->extensionStartCodeIdentifier),
    extensionType(src->extensionType) 
    {}

  // Peek the extension start code identifier (4 bits) in the payload
  bool parseStartCode(QByteArray &extension_payload, TreeItem *root);

  unsigned extensionStartCodeIdentifier;
  ExtensionType extensionType;
  QString getExtensionFunctionName();
};

struct SequenceExtension : NalExtension
{
  SequenceExtension(const NalExtension &nal) : NalExtension(nal) {}
  bool parse(const QByteArray &parameterSetData, TreeItem *root);

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

struct PictureCodingExtension : NalExtension
{
  PictureCodingExtension(const NalExtension &nal) : NalExtension(nal) {}
  bool parse(const QByteArray &parameterSetData, TreeItem *root);

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

} // namespace MPEG2
