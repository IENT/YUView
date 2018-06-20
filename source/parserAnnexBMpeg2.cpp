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

#include "parserAnnexBMpeg2.h"

// Read "numBits" bits into the variable "into". 
#define READBITS(into,numBits) {QString code; into=reader.readBits(numBits, &code); if (itemTree) new TreeItem(#into,into,QString("u(v) -> u(%1)").arg(numBits),code, itemTree);}
#define READBITS_M(into,numBits,meanings) {QString code; into=reader.readBits(numBits, &code); if (itemTree) new TreeItem(#into,into,QString("u(v) -> u(%1)").arg(numBits),code, meanings,itemTree);}
#define READBITS_A(into,numBits,i) {QString code; int v=reader.readBits(numBits,&code); into.append(v); if (itemTree) new TreeItem(QString(#into)+QString("[%1]").arg(i),v,QString("u(v) -> u(%1)").arg(numBits),code, itemTree);}
// Read a flag (1 bit) into the variable "into".
#define READFLAG(into) {into=(reader.readBits(1)!=0); if (itemTree) new TreeItem(#into,into,QString("u(1)"),(into!=0)?"1":"0",itemTree);}
#define READFLAG_M(into,meanings) {into=(reader.readBits(1)!=0); if (itemTree) new TreeItem(#into,into,QString("u(1)"),(into!=0)?"1":"0",meanings,itemTree);}
#define READFLAG_A(into,i) {bool b=(reader.readBits(1)!=0); into.append(b); if (itemTree) new TreeItem(QString(#into)+QString("[%1]").arg(i),b,QString("u(1)"),b?"1":"0",itemTree);}
// Read "numBits" bits and ignore them.
#define IGNOREBITS(numBits) {int val = reader.readBits(numBits);}

const QStringList parserAnnexBMpeg2::nal_unit_type_toString = QStringList()
  << "UNSPECIFIED" << "PICTURE" << "SLICE" << "USER_DATA" << "SEQUENCE_HEADER" << "SEQUENCE_ERROR" << "EXTENSION_START" << "SEQUENCE_END"
  << "GROUP_START" << "SYSTEM_START_CODE" << "RESERVED";

parserAnnexBMpeg2::parserAnnexBMpeg2()
{
}

void parserAnnexBMpeg2::nal_unit_mpeg2::parse_nal_unit_header(const QByteArray &header_byte, TreeItem *root)
{
  if (header_byte.length() != 1)
    throw std::logic_error("The AVC header must have only one byte.");

  // Create a sub byte parser to access the bits
  sub_byte_reader reader(header_byte);

  // Create a new TreeItem root for the item
  // The macros will use this variable to add all the parsed variables
  TreeItem *const itemTree = root ? new TreeItem("header_code()", root) : nullptr;

  // Parse the start code
  QString code;
  start_code_value = reader.readBits(8, &code);
  QString start_code_value_meaning = interprete_start_code(start_code_value);
  
  if (itemTree) 
    new TreeItem("start_code_value", start_code_value, QString("u(v) -> u(8)"), code, start_code_value_meaning, itemTree);
}

QByteArray parserAnnexBMpeg2::nal_unit_mpeg2::getNALHeader() const
{
  QByteArray ret;
  ret.append(start_code_value);
  return ret;
}

QString parserAnnexBMpeg2::nal_unit_mpeg2::interprete_start_code(int start_code)
{
  if (start_code == 0)
  {
    nal_unit_type = PICTURE;
    return "picture_start_code";
  }
  else if (start_code >= 0x01 && start_code <= 0xaf)
  {
    nal_unit_type = SLICE;
    slice_id = start_code - 1;
    return "slice_start_code";
  }
  else if (start_code == 0xb0 || start_code == 0xb1 || start_code == 0xb6)
  {
    nal_unit_type = RESERVED;
    return "reserved";
  }
  else if (start_code == 0xb2)
  {
    nal_unit_type = USER_DATA;
    return "user_data_start_code";
  }
  else if (start_code == 0xb3)
  {
    nal_unit_type = SEQUENCE_HEADER;
    return "sequence_header_code";
  }
  else if (start_code == 0xb4)
  {
    nal_unit_type = SEQUENCE_ERROR;
    return "sequence_error_code";
  }
  else if (start_code == 0xb5)
  {
    nal_unit_type = EXTENSION_START;
    return "extension_start_code";
  }
  else if (start_code == 0xb7)
  {
    nal_unit_type = SEQUENCE_END;
    return "sequence_end_code";
  }
  else if (start_code == 0xb8)
  {
    nal_unit_type = GROUP_START;
    return "group_start_code";
  }
  else if (start_code >= 0xb9)
  {
    nal_unit_type = SYSTEM_START_CODE;
    system_start_codes = start_code - 0xb9;
    return "system start codes";
  }
  
  return "";
}

void parserAnnexBMpeg2::parseAndAddNALUnit(int nalID, QByteArray data, TreeItem *parent, QUint64Pair nalStartEndPosFile, QString *nalTypeName)
{
  // Skip the NAL unit header
  int skip = 0;
  if (data.at(0) == (char)0 && data.at(1) == (char)0 && data.at(2) == (char)1)
    skip = 3;
  else if (data.at(0) == (char)0 && data.at(1) == (char)0 && data.at(2) == (char)0 && data.at(3) == (char)1)
    skip = 4;
  else
    // No NAL header found
    skip = 0;

  // Read ony byte (the NAL header) (technically there is no NAL in mpeg2 but it works pretty similarly)
  QByteArray nalHeaderBytes = data.mid(skip, 1);
  QByteArray payload = data.mid(skip + 1);

  // Use the given tree item. If it is not set, use the nalUnitMode (if active). 
  // We don't set data (a name) for this item yet. 
  // We want to parse the item and then set a good description.
  QString specificDescription;
  TreeItem *nalRoot = nullptr;
  if (parent)
    nalRoot = new TreeItem(parent);
  else if (!nalUnitModel.rootItem.isNull())
    nalRoot = new TreeItem(nalUnitModel.rootItem.data());

  // Create a nal_unit and read the header
  nal_unit_mpeg2 nal_mpeg2(nalStartEndPosFile, nalID);
  nal_mpeg2.parse_nal_unit_header(nalHeaderBytes, nalRoot);

  if (nal_mpeg2.nal_unit_type == SEQUENCE_HEADER)
  {
    // A sequence header
    auto new_sequence_header = QSharedPointer<sequence_header>(new sequence_header(nal_mpeg2));
    new_sequence_header->parse_sequence_header(payload, nalRoot);
    specificDescription = QString(" Sequence Header");
    if (nalTypeName)
      *nalTypeName = QString(" SeqHeader");
  }
  else if (nal_mpeg2.nal_unit_type == PICTURE)
  {
    auto new_picture_header = QSharedPointer<picture_header>(new picture_header(nal_mpeg2));
    new_picture_header->parse_picture_header(payload, nalRoot);
    specificDescription = QString(" Picture Header");
    if (nalTypeName)
      *nalTypeName = QString(" PicHeader");
  }
  else if (nal_mpeg2.nal_unit_type == EXTENSION_START)
  {
    TreeItem *const message_tree = nalRoot ? new TreeItem("", nalRoot) : nullptr;

    // A sequence_extension
    auto new_extension = QSharedPointer<nal_extension>(new nal_extension(nal_mpeg2));
    new_extension->parse_extension_start_code(payload, message_tree);

    if (message_tree)
      message_tree->itemData[0] = QString("extension %1").arg(new_extension->extension_name);

    if (new_extension->extension_type == EXT_SEQUENCE)
    {
      auto new_sequence_extension = QSharedPointer<sequence_extension>(new sequence_extension(new_extension));
      new_sequence_extension->parse_sequence_extension(payload, message_tree);
      if (nalTypeName)
        *nalTypeName = QString(" SeqExt");
      specificDescription = QString(" Sequence Extension");
    }
    else if (new_extension->extension_type == EXT_PICTURE_CODING)
    {
      auto new_picture_coding_extension = QSharedPointer<picture_coding_extension>(new picture_coding_extension(new_extension));
      new_picture_coding_extension->parse_picture_coding_extension(payload, message_tree);
      if (nalTypeName)
        *nalTypeName = QString(" PicCodExt");
      specificDescription = QString(" Picture Coding Extension");
    }
    else
    {
      if (nalTypeName)
        *nalTypeName = QString(" Ext");
      specificDescription = QString(" Extension");
    }
  }

  if (nalRoot)
    // Set a useful name of the TreeItem (the root for this NAL)
    nalRoot->itemData.append(QString("NAL %1: %2").arg(nal_mpeg2.nal_idx).arg(nal_unit_type_toString.value(nal_mpeg2.nal_unit_type)) + specificDescription);
}

parserAnnexBMpeg2::sequence_header::sequence_header(const nal_unit_mpeg2 & nal) : nal_unit_mpeg2(nal)
{
}

void parserAnnexBMpeg2::sequence_header::parse_sequence_header(const QByteArray & parameterSetData, TreeItem * root)
{
  nalPayload = parameterSetData;
  sub_byte_reader reader(parameterSetData);

  // Create a new TreeItem root for the item
  // The macros will use this variable to add all the parsed variables
  TreeItem *const itemTree = root ? new TreeItem("sequence_header()", root) : nullptr;

  READBITS(horizontal_size_value, 12);
  READBITS(vertical_size_value, 12);
  QStringList aspect_ratio_information_meaning = QStringList()
    << "Forbidden"
    << "SAR 1.0 (Square Sample)"
    << "DAR 3:4"
    << "DAR 9:16"
    << "DAR 1:2.21"
    << "Reserved";
  READBITS_M(aspect_ratio_information, 4, aspect_ratio_information_meaning);
  QStringList frame_rate_code_meaning = QStringList()
    << "Forbidden"
    << "24000:1001 (23.976...)"
    << "24"
    << "25"
    << "30000:1001 (29.97...)"
    << "30"
    << "50"
    << "60000:1001 (59.94)"
    << "60"
    << "Reserved";
  READBITS_M(frame_rate_code, 4, frame_rate_code_meaning);
  READBITS_M(bit_rate_value, 18, "The lower 18 bits of bit_rate.");
  READFLAG(marker_bit);
  if (!marker_bit)
    throw std::logic_error("The marker_bit shall be set to 1 to prevent emulation of start codes.");
  READBITS_M(vbv_buffer_size_value, 10, "the lower 10 bits of vbv_buffer_size");
  READFLAG(constrained_parameters_flag);
  READFLAG(load_intra_quantiser_matrix);
  if (load_intra_quantiser_matrix)
  {
    for (int i=0; i<64; i++)
      READBITS(intra_quantiser_matrix[i], 8);
  }
  READFLAG(load_non_intra_quantiser_matrix);
  if (load_non_intra_quantiser_matrix)
  {
    for (int i=0; i<64; i++)
      READBITS(non_intra_quantiser_matrix[i], 8);
  }
}

parserAnnexBMpeg2::picture_header::picture_header(const nal_unit_mpeg2 & nal) : nal_unit_mpeg2(nal)
{
}

void parserAnnexBMpeg2::picture_header::parse_picture_header(const QByteArray & parameterSetData, TreeItem * root)
{
  nalPayload = parameterSetData;
  sub_byte_reader reader(parameterSetData);

  // Create a new TreeItem root for the item
  // The macros will use this variable to add all the parsed variables
  TreeItem *const itemTree = root ? new TreeItem("picture_header()", root) : nullptr;

  READBITS(temporal_reference, 10);
  QStringList picture_coding_type_meaning = QStringList()
    << "Forbidden" << "intra-coded (I)" << "predictive-coded (P)" << "bidirectionally-predictive-coded (B)" << "dc intra-coded (D)" << "Reserved";
  READBITS_M(picture_coding_type, 3, picture_coding_type_meaning);
  READBITS(vbv_delay, 16);
  if (picture_coding_type == 2 || picture_coding_type == 3)
  {
    READFLAG(full_pel_forward_vector);
    READBITS(forward_f_code, 3);
  }
  if (picture_coding_type == 3)
  {
    READFLAG(full_pel_backward_vector);
    READBITS(backward_f_code, 3);
  }

  bool abort = false;
  while (reader.testReadingBits(9)) 
  {
    bool extra_bit_picture;
    READFLAG(extra_bit_picture);
    if (!extra_bit_picture)
      abort = false;
    
    if (!abort)
    {
      int extra_information_picture;
      READBITS(extra_information_picture, 8);
    }
  }
}

void parserAnnexBMpeg2::nal_extension::parse_extension_start_code(QByteArray & extension_payload, TreeItem * root)
{
  sub_byte_reader reader(extension_payload);

  // Create a new TreeItem root for the item
  // The macros will use this variable to add all the parsed variables
  TreeItem *const itemTree = root ? new TreeItem("nal_extension()", root) : nullptr;

  QStringList extension_start_code_identifier_meaning = QStringList()
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
  READBITS_M(extension_start_code_identifier, 4, extension_start_code_identifier_meaning);

  if (extension_start_code_identifier == 1)
    extension_type = EXT_SEQUENCE;
  else if (extension_start_code_identifier == 2)
    extension_type = EXT_SEQUENCE_DISPLAY;
  else if (extension_start_code_identifier == 3)
    extension_type = EXT_QUANT_MATRIX;
  else if (extension_start_code_identifier == 4)
    extension_type = EXT_COPYRIGHT;
  else if (extension_start_code_identifier == 5)
    extension_type = EXT_SEQUENCE_SCALABLE;
  else if (extension_start_code_identifier == 7)
    extension_type = EXT_PICTURE_DISPLAY;
  else if (extension_start_code_identifier == 8)
    extension_type = EXT_PICTURE_CODING;
  else if (extension_start_code_identifier == 9)
    extension_type = EXT_PICTURE_SPATICAL_SCALABLE;
  else if (extension_start_code_identifier == 10)
    extension_type = EXT_PICTURE_TEMPORAL_SCALABLE;
  else
    extension_type = EXT_RESERVED;

  if (extension_start_code_identifier_meaning.length() > extension_start_code_identifier)
    extension_name = extension_start_code_identifier_meaning[extension_start_code_identifier];
  else
    extension_name = "Reserved";
}

parserAnnexBMpeg2::sequence_extension::sequence_extension(const nal_extension & nal) : nal_extension(nal)
{
}

void parserAnnexBMpeg2::sequence_extension::parse_sequence_extension(const QByteArray & parameterSetData, TreeItem * root)
{
  nalPayload = parameterSetData;
  sub_byte_reader reader(parameterSetData);

  // Create a new TreeItem root for the item
  // The macros will use this variable to add all the parsed variables
  TreeItem *const itemTree = root ? new TreeItem("sequence_extension()", root) : nullptr;
  
  IGNOREBITS(4);  // The extension_start_code_identifier was already read
  READBITS(profile_and_level_indication, 8);
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
    throw std::logic_error("The marker_bit shall be set to 1 to prevent emulation of start codes.");
  READBITS_M(vbv_buffer_size_extension, 8, "most significant bits from vbv_buffer_size");
  QStringList low_delay_meaning = QStringList()
    << "sequence may contain B-pictures, the frame re-ordering delay is present in the VBV description and the bitstream shall not contain big pictures"
    << "sequence does not contain any B-pictures, the frame e-ordering delay is not present in the VBV description and the bitstream may contain 'big pictures'";
  READFLAG_M(low_delay, low_delay_meaning)
  READBITS(frame_rate_extension_n, 2);
  READBITS(frame_rate_extension_d, 5);
}

parserAnnexBMpeg2::picture_coding_extension::picture_coding_extension(const nal_extension & nal) : nal_extension(nal)
{
}

void parserAnnexBMpeg2::picture_coding_extension::parse_picture_coding_extension(const QByteArray & parameterSetData, TreeItem * root)
{
  nalPayload = parameterSetData;
  sub_byte_reader reader(parameterSetData);

  // Create a new TreeItem root for the item
  // The macros will use this variable to add all the parsed variables
  TreeItem *const itemTree = root ? new TreeItem("picture_coding_extension()", root) : nullptr;

  IGNOREBITS(4);  // The extension_start_code_identifier was already read
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

}


