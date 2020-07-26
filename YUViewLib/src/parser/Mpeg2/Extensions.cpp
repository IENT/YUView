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

#include "Extensions.h"

#include "parser/common/parserMacros.h"
#include "parser/common/ReaderHelper.h"

namespace MPEG2
{

bool NalExtension::parseStartCode(QByteArray & extension_payload, TreeItem * itemTree)
{
  ReaderHelper reader(extension_payload, itemTree);

  QStringList extensionStartCodeIdentifierMeaning = QStringList()
    << "Reserved"
    << "Sequence Extension ID"
    << "Sequence Display Extension ID"
    << "Quant Matrix Extension ID"
    << "Copyright Extension ID"
    << "Sequence Scalable Extension ID"
    << "Reserved"
    << "Picture Display Extension ID"
    << "Picture Coding Extension ID"
    << "Picture Spatial Scalable Extension ID"
    << "Picture Temporal Scalable Extension ID"
    << "Reserved";
  READBITS_M(extensionStartCodeIdentifier, 4, extensionStartCodeIdentifierMeaning);

  if (extensionStartCodeIdentifier == 1)
    extensionType = ExtensionType::EXT_SEQUENCE;
  else if (extensionStartCodeIdentifier == 2)
    extensionType = ExtensionType::EXT_SEQUENCE_DISPLAY;
  else if (extensionStartCodeIdentifier == 3)
    extensionType = ExtensionType::EXT_QUANT_MATRIX;
  else if (extensionStartCodeIdentifier == 4)
    extensionType = ExtensionType::EXT_COPYRIGHT;
  else if (extensionStartCodeIdentifier == 5)
    extensionType = ExtensionType::EXT_SEQUENCE_SCALABLE;
  else if (extensionStartCodeIdentifier == 7)
    extensionType = ExtensionType::EXT_PICTURE_DISPLAY;
  else if (extensionStartCodeIdentifier == 8)
    extensionType = ExtensionType::EXT_PICTURE_CODING;
  else if (extensionStartCodeIdentifier == 9)
    extensionType = ExtensionType::EXT_PICTURE_SPATICAL_SCALABLE;
  else if (extensionStartCodeIdentifier == 10)
    extensionType = ExtensionType::EXT_PICTURE_TEMPORAL_SCALABLE;
  else
    extensionType = ExtensionType::EXT_RESERVED;

  return true;
}

QString NalExtension::getExtensionFunctionName()
{
  switch (extensionType)
  {
  case ExtensionType::EXT_SEQUENCE:
    return "sequence_extension()";
  case ExtensionType::EXT_SEQUENCE_DISPLAY:
    return "sequence_display_extension()";
  case ExtensionType::EXT_QUANT_MATRIX:
    return "quant_matrix_extension()";
  case ExtensionType::EXT_COPYRIGHT:
    return "copyright_extension()";
  case ExtensionType::EXT_SEQUENCE_SCALABLE:
    return "sequence_scalable_extension()";
  case ExtensionType::EXT_PICTURE_DISPLAY:
    return "picture_display_extension()";
  case ExtensionType::EXT_PICTURE_CODING:
    return "picture_coding_extension()";
  case ExtensionType::EXT_PICTURE_SPATICAL_SCALABLE:
    return "picture_spatial_scalable_extension()";
  case ExtensionType::EXT_PICTURE_TEMPORAL_SCALABLE:
    return "picture_temporal_scalable_extension()";
  default:
    return "";
  }
}

bool SequenceExtension::parse(const QByteArray & parameterSetData, TreeItem *root)
{
  this->nalPayload = parameterSetData;
  ReaderHelper reader(parameterSetData, root);
  
  IGNOREBITS(4);  // The extensioextensionStartCodeIdentifiernType was already read
  READBITS(profile_and_level_indication, 8);

  profile_identification = (profile_and_level_indication >> 4) & 0x07;
  QStringList profile_identification_meaning = QStringList()
    << "Reserved" << "High" << "Spatially Scalable" << "SNR Scalable" << "Main" << "Simple" << "Reserved";
  LOGVAL_M(profile_identification, profile_identification_meaning);
  level_identification = profile_and_level_indication & 0x03;
  QMap<int, QString> level_identification_meaning;
  level_identification_meaning.insert(0b0100, "High");
  level_identification_meaning.insert(0b0110, "High 1440");
  level_identification_meaning.insert(0b1000, "Main");
  level_identification_meaning.insert(0b1010, "Low");
  LOGVAL_M(level_identification, level_identification_meaning);

  QStringList progressive_sequence_meaning = QStringList()
    << "the coded video sequence may contain both frame-pictures and field-pictures, and frame-picture may be progressive or interlaced frames."
    << "the coded video sequence contains only progressive frame-pictures";
  READFLAG_M(progressive_sequence, progressive_sequence_meaning);
  QStringList chroma_format_meaning = QStringList()
    << "Reserved" << "4:2:0" << "4:2:2" << "4:4:4";
  READBITS_M(chroma_format, 2, chroma_format_meaning);
  READBITS_M(horizontal_size_extension, 2, "most significant bits from horizontal_size");
  READBITS_M(vertical_size_extension, 2, "most significant bits from vertical_size");
  READBITS_M(bit_rate_extension, 12, "12 most significant bits from bit_rate");
  READFLAG(marker_bit);
  if (!marker_bit)
    return reader.addErrorMessageChildItem("The marker_bit shall be set to 1 to prevent emulation of start codes.");
  READBITS_M(vbv_buffer_size_extension, 8, "most significant bits from vbv_buffer_size");
  QStringList low_delay_meaning = QStringList()
    << "sequence may contain B-pictures, the frame re-ordering delay is present in the VBV description and the bitstream shall not contain big pictures"
    << "sequence does not contain any B-pictures, the frame e-ordering delay is not present in the VBV description and the bitstream may contain 'big pictures'";
  READFLAG_M(low_delay, low_delay_meaning);
  READBITS(frame_rate_extension_n, 2);
  READBITS(frame_rate_extension_d, 5);

  return true;
}

bool PictureCodingExtension::parse(const QByteArray & parameterSetData, TreeItem *itemTree)
{
  this->nalPayload = parameterSetData;
  ReaderHelper reader(parameterSetData, itemTree);

  IGNOREBITS(4);  // The extensioextensionStartCodeIdentifiernType was already read
  READBITS(f_code[0][0], 4); // forward horizontal
  READBITS(f_code[0][1], 4); // forward vertical
  READBITS(f_code[1][0], 4); // backward horizontal
  READBITS(f_code[1][1], 4); // backward vertical

  QStringList intra_dc_precision_meaning = QStringList() << "Precision 8 bit" << "Precision 9 bit" << "Precision 10 bit" << "Precision 11 bit";
  READBITS_M(intra_dc_precision, 2, intra_dc_precision_meaning);
  QStringList picture_structure_meaning = QStringList() << "Reserved" << "Top Field" << "Bottom Field" << "Frame picture";
  READBITS_M(picture_structure, 2, picture_structure_meaning);

  READFLAG(top_field_first);
  READFLAG(frame_pred_frame_dct);
  READFLAG(concealment_motion_vectors);
  READFLAG(q_scale_type);
  READFLAG(intra_vlc_format);
  READFLAG(alternate_scan);
  READFLAG(repeat_first_field);
  READFLAG(chroma_420_type);
  READFLAG(progressive_frame);
  READFLAG(composite_display_flag);
  if (composite_display_flag)
  {
    READFLAG(v_axis);
    READBITS(field_sequence, 3);
    READFLAG(sub_carrier);
    READBITS(burst_amplitude, 7);
    READBITS(sub_carrier_phase, 8);
  }

  return true;
}

} // namespace MPEG2