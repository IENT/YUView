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

#include "parserAnnexBMpeg2.h"

#include "parserCommonMacros.h"

using namespace parserCommon;

const QStringList parserAnnexBMpeg2::nal_unit_type_toString = QStringList()
  << "UNSPECIFIED" << "PICTURE" << "SLICE" << "USER_DATA" << "SEQUENCE_HEADER" << "SEQUENCE_ERROR" << "EXTENSION_START" << "SEQUENCE_END"
  << "GROUP_START" << "SYSTEM_START_CODE" << "RESERVED";

bool parserAnnexBMpeg2::nal_unit_mpeg2::parse_nal_unit_header(const QByteArray &header_byte, TreeItem *root)
{
  // Create a sub byte parser to access the bits
  reader_helper reader(header_byte, root, "header_code()");

  if (header_byte.length() != 1)
    return reader.addErrorMessageChildItem("The header code must be one byte only.");

  READBITS_M(start_code_value, 8, get_start_code_meanings());

  return true;
}

QByteArray parserAnnexBMpeg2::nal_unit_mpeg2::getNALHeader() const
{
  QByteArray ret;
  ret.append(start_code_value);
  return ret;
}

QStringList parserAnnexBMpeg2::nal_unit_mpeg2::get_start_code_meanings()
{
  QStringList meanings = QStringList();
  meanings.append("picture_start_code");
  // 01 through AF
  for (int i=0x01; i<0xaf; i++)
    meanings.append(QString("slice_start_code - slice %1").arg(i));
  meanings.append("reserved");
  meanings.append("reserved");
  meanings.append("user_data_start_code");
  meanings.append("sequence_header_code");
  meanings.append("sequence_error_code");
  meanings.append("extension_start_code");
  meanings.append("reserved");
  meanings.append("sequence_end_code");
  meanings.append("group_start_code");
  meanings.append("system start codes");
  
  return meanings;
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

bool parserAnnexBMpeg2::parseAndAddNALUnit(int nalID, QByteArray data, TreeItem *parent, QUint64Pair nalStartEndPosFile, QString *nalTypeName)
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
  else if (!packetModel->isNull())
    nalRoot = new TreeItem(packetModel->getRootItem());

  // Create a nal_unit and read the header
  nal_unit_mpeg2 nal_mpeg2(nalStartEndPosFile, nalID);
  if (!nal_mpeg2.parse_nal_unit_header(nalHeaderBytes, nalRoot))
    return false;

  bool parsingSuccess = true;
  if (nal_mpeg2.nal_unit_type == SEQUENCE_HEADER)
  {
    // A sequence header
    auto new_sequence_header = QSharedPointer<sequence_header>(new sequence_header(nal_mpeg2));
    parsingSuccess = new_sequence_header->parse_sequence_header(payload, nalRoot);

    specificDescription = parsingSuccess ? " Sequence Header" : " Sequence Header (Error)";
    if (nalTypeName)
      *nalTypeName = parsingSuccess ? " SeqHeader" : " SeqHeader(Err)";
    if (parsingSuccess && !first_sequence_header)
      first_sequence_header = new_sequence_header;
  }
  else if (nal_mpeg2.nal_unit_type == PICTURE)
  {
    auto new_picture_header = QSharedPointer<picture_header>(new picture_header(nal_mpeg2));
    parsingSuccess = new_picture_header->parse_picture_header(payload, nalRoot);

    specificDescription = parsingSuccess ? " Picture Header" : " Picture Header (Error)";
    if (nalTypeName)
      *nalTypeName = parsingSuccess ? " PicHeader" : " picHeader(Err)";
  }
  else if (nal_mpeg2.nal_unit_type == GROUP_START)
  {
    auto new_group_of_pictures_header = QSharedPointer<group_of_pictures_header>(new group_of_pictures_header(nal_mpeg2));
    parsingSuccess = new_group_of_pictures_header->parse_group_of_pictures_header(payload, nalRoot);

    specificDescription = parsingSuccess ? " Group of Pictures" : " Group of Pictures (Error)";
    if (nalTypeName)
      *nalTypeName = parsingSuccess ? " PicGroup" : " PicGroup(Err)";
  }
  else if (nal_mpeg2.nal_unit_type == USER_DATA)
  {
    auto new_user_data = QSharedPointer<user_data>(new user_data(nal_mpeg2));
    parsingSuccess = new_user_data->parse_user_data(payload, nalRoot);

    specificDescription = parsingSuccess ? " User Data" : " User Data (Error)";
    if (nalTypeName)
      *nalTypeName = parsingSuccess ? " UserData" : " UserData(Err)";
  }
  else if (nal_mpeg2.nal_unit_type == EXTENSION_START)
  {
    TreeItem *const message_tree = nalRoot ? new TreeItem("", nalRoot) : nullptr;

    // An extension
    auto new_extension = QSharedPointer<nal_extension>(new nal_extension(nal_mpeg2));
    if (!new_extension->parse_extension_start_code(payload, message_tree))
      return false;

    if (message_tree)
      message_tree->itemData[0] = new_extension->get_extension_function_name();

    if (new_extension->extension_type == EXT_SEQUENCE)
    {
      auto new_sequence_extension = QSharedPointer<sequence_extension>(new sequence_extension(new_extension));
      parsingSuccess = new_sequence_extension->parse_sequence_extension(payload, message_tree);

      specificDescription = parsingSuccess ? " Sequence Extension": " Sequence Extension (Error)";
      if (nalTypeName)
        *nalTypeName = parsingSuccess ? " SeqExt" : " SeqExt(Err)";
      
      if (!first_sequence_extension)
        first_sequence_extension = new_sequence_extension;
    }
    else if (new_extension->extension_type == EXT_PICTURE_CODING)
    {
      auto new_picture_coding_extension = QSharedPointer<picture_coding_extension>(new picture_coding_extension(new_extension));
      parsingSuccess = new_picture_coding_extension->parse_picture_coding_extension(payload, message_tree);

      specificDescription = parsingSuccess ? " Picture Coding Extension" : " Picture Coding Extension (Error)";
      if (nalTypeName)
        *nalTypeName = parsingSuccess ? " PicCodExt" : " PicCodExt(Err)";
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

  return parsingSuccess;
}

bool parserAnnexBMpeg2::sequence_header::parse_sequence_header(const QByteArray & parameterSetData, TreeItem * root)
{
  nalPayload = parameterSetData;
  reader_helper reader(parameterSetData, root, "sequence_header()");

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
    reader.addErrorMessageChildItem("The marker_bit shall be set to 1 to prevent emulation of start codes.");
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
  return true;
}

bool parserAnnexBMpeg2::picture_header::parse_picture_header(const QByteArray & parameterSetData, TreeItem * root)
{
  nalPayload = parameterSetData;
  reader_helper reader(parameterSetData, root, "picture_header");

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
      unsigned int extra_information_picture;
      READBITS(extra_information_picture, 8);
    }
  }
  return true;
}

bool parserAnnexBMpeg2::group_of_pictures_header::parse_group_of_pictures_header(const QByteArray & parameterSetData, TreeItem * root)
{
  nalPayload = parameterSetData;
  reader_helper reader(parameterSetData, root, "group_of_pictures_header()");

  READBITS(time_code, 25);
  READFLAG(closed_gop);
  READFLAG(broken_link);

  return true;
}

bool parserAnnexBMpeg2::user_data::parse_user_data(const QByteArray & parameterSetData, TreeItem * root)
{
  nalPayload = parameterSetData;

  // Create a new TreeItem root for the item
  // The macros will use this variable to add all the parsed variables
  TreeItem *const itemTree = root ? new TreeItem("user_data()", root) : nullptr;

  if (itemTree)
  {
    // Log the user data bytes
    int i=0;
    for (char c : parameterSetData)
    {
      QString code;
      for (int i = 7; i >= 0; i--)
        code += (c & (1 << i)) ? "1" : "0";
       new TreeItem(QString("byte[%1]").arg(i++), c, QString("u(v) -> u(8)"), code, itemTree);
    }
  }
  return true;
}

bool parserAnnexBMpeg2::nal_extension::parse_extension_start_code(QByteArray & extension_payload, TreeItem * itemTree)
{
  reader_helper reader(extension_payload, itemTree);

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

  return true;
}

QString parserAnnexBMpeg2::nal_extension::get_extension_function_name()
{
  switch (extension_type)
  {
  case parserAnnexBMpeg2::EXT_SEQUENCE:
    return "sequence_extension()";
  case parserAnnexBMpeg2::EXT_SEQUENCE_DISPLAY:
    return "sequence_display_extension()";
  case parserAnnexBMpeg2::EXT_QUANT_MATRIX:
    return "quant_matrix_extension()";
  case parserAnnexBMpeg2::EXT_COPYRIGHT:
    return "copyright_extension()";
  case parserAnnexBMpeg2::EXT_SEQUENCE_SCALABLE:
    return "sequence_scalable_extension()";
  case parserAnnexBMpeg2::EXT_PICTURE_DISPLAY:
    return "picture_display_extension()";
  case parserAnnexBMpeg2::EXT_PICTURE_CODING:
    return "picture_coding_extension()";
  case parserAnnexBMpeg2::EXT_PICTURE_SPATICAL_SCALABLE:
    return "picture_spatial_scalable_extension()";
  case parserAnnexBMpeg2::EXT_PICTURE_TEMPORAL_SCALABLE:
    return "picture_temporal_scalable_extension()";
  default:
    return "";
  }
}

bool parserAnnexBMpeg2::sequence_extension::parse_sequence_extension(const QByteArray & parameterSetData, TreeItem *root)
{
  nalPayload = parameterSetData;
  reader_helper reader(parameterSetData, root);
  
  IGNOREBITS(4);  // The extension_start_code_identifier was already read
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

bool parserAnnexBMpeg2::picture_coding_extension::parse_picture_coding_extension(const QByteArray & parameterSetData, TreeItem *itemTree)
{
  nalPayload = parameterSetData;
  reader_helper reader(parameterSetData, itemTree);

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

  return true;
}

QPair<int,int> parserAnnexBMpeg2::getProfileLevel()
{
  if (first_sequence_extension)
    return QPair<int,int>(first_sequence_extension->profile_identification, first_sequence_extension->level_identification);
  return QPair<int,int>(0,0);
}

double parserAnnexBMpeg2::getFramerate() const
{
  double frame_rate = 0.0;
  if (first_sequence_header && first_sequence_header->frame_rate_code > 0 && first_sequence_header->frame_rate_code <= 8)
  {
    QList<double> frame_rates = QList<double>() << 0.0 << 24000/1001 << 24 << 25 << 30000/1001 << 30 << 50 << 60000/1001 << 60;
    frame_rate = frame_rates[first_sequence_header->frame_rate_code];

    if (first_sequence_extension)
    {
      frame_rate *= (first_sequence_extension->frame_rate_extension_n + 1) / (first_sequence_extension->frame_rate_extension_d + 1);
    }
  }

  return frame_rate;
}

QSize parserAnnexBMpeg2::getSequenceSizeSamples() const
{
  int w = 0, h = 0;
  if (first_sequence_header)
  {
    w = first_sequence_header->horizontal_size_value;
    h = first_sequence_header->vertical_size_value;

    if (first_sequence_extension)
    {
      w += first_sequence_extension->horizontal_size_extension << 12;
      h += first_sequence_extension->vertical_size_extension << 12;
    }
  }
  return QSize(w, h);
}

yuvPixelFormat parserAnnexBMpeg2::getPixelFormat() const
{
  if (first_sequence_extension)
  {
    int c = first_sequence_extension->chroma_format;
    if (c == 1)
      return yuvPixelFormat(YUV_420, 8);
    if (c == 2)
      return yuvPixelFormat(YUV_422, 8);
    if (c == 3)
      return yuvPixelFormat(YUV_444, 8);
  }
  return yuvPixelFormat();
}

QPair<int,int> parserAnnexBMpeg2::getSampleAspectRatio()
{
  if (first_sequence_header)
  {
    const int ratio = first_sequence_header->aspect_ratio_information;
    if (ratio == 2)
      return QPair<int,int>(3, 4);
    if (ratio == 3)
      return QPair<int,int>(9, 16);
    if (ratio == 4)
      return QPair<int,int>(100, 221);
    if (ratio == 2)
      return QPair<int,int>(3, 4);
  }
  return QPair<int,int>(1, 1);
}