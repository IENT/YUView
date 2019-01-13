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

#include "parserAV1OBU.h"

#include <algorithm>
#include "parserCommonMacros.h"

using namespace parserCommon;

#define READDELTAQ(into) do { if (!read_delta_q(#into, into, reader)) return false; } while(0)

#define SELECT_SCREEN_CONTENT_TOOLS 2
#define SELECT_INTEGER_MV 2
#define NUM_REF_FRAMES 8
#define REFS_PER_FRAME 7
#define PRIMARY_REF_NONE 7
#define MAX_SEGMENTS 8
#define SEG_LVL_MAX 8
#define SEG_LVL_REF_FRAME 5
#define MAX_LOOP_FILTER 63
#define SEG_LVL_ALT_Q 0
#define TOTAL_REFS_PER_FRAME 8

#define SUPERRES_DENOM_BITS 3
#define SUPERRES_DENOM_MIN 9
#define SUPERRES_NUM 8

// The indices into the RefFrame list
#define INTRA_FRAME 0
#define LAST_FRAME 1
#define LAST2_FRAME 2
#define LAST3_FRAME 3
#define GOLDEN_FRAME 4
#define BWDREF_FRAME 5
#define ALTREF2_FRAME 6
#define ALTREF_FRAME 7

#define MAX_TILE_WIDTH 4096
#define MAX_TILE_AREA 4096 * 2304
#define MAX_TILE_COLS 64
#define MAX_TILE_ROWS 64

const QStringList parserAV1OBU::obu_type_toString = QStringList()
  << "RESERVED" << "OBU_SEQUENCE_HEADER" << "OBU_TEMPORAL_DELIMITER" << "OBU_FRAME_HEADER" << "OBU_TILE_GROUP" 
  << "OBU_METADATA" << "OBU_FRAME" << "OBU_REDUNDANT_FRAME_HEADER" << "OBU_TILE_LIST" << "OBU_PADDING";

parserAV1OBU::parserAV1OBU(QObject *parent) : parserBase(parent)
{
  // Reset all values in parserAV1OBU
  memset(&decValues, 0, sizeof(global_decoding_values));
  decValues.PrevFrameID = -1;
}

parserAV1OBU::obu_unit::obu_unit(QSharedPointer<obu_unit> obu_src)
{
  // Copy all members but the payload. The payload is only saved for specific obus.
  filePosStartEnd = obu_src->filePosStartEnd;
  obu_idx = obu_src->obu_idx;
  obu_type = obu_src->obu_type;
  obu_extension_flag = obu_src->obu_extension_flag;
  obu_has_size_field = obu_src->obu_has_size_field;
  temporal_id = obu_src->temporal_id;
  spatial_id = obu_src->spatial_id;
  obu_size = obu_src->obu_size;
}

bool parserAV1OBU::obu_unit::parse_obu_header(const QByteArray &header_data, unsigned int &nrBytesHeader, TreeItem *root)
{
  // Create a sub byte parser to access the bits
  reader_helper reader(header_data, root, "obu_header()");

  if (header_data.length() == 0)
    return reader.addErrorMessageChildItem("The OBU header must have at least one byte");

  READZEROBITS(1, "obu_forbidden_bit");

  QStringList obu_type_id_meaning = QStringList()
    << "Reserved"
    << "OBU_SEQUENCE_HEADER"
    << "OBU_TEMPORAL_DELIMITER"
    << "OBU_FRAME_HEADER"
    << "OBU_TILE_GROUP"
    << "OBU_METADATA"
    << "OBU_FRAME"
    << "OBU_REDUNDANT_FRAME_HEADER"
    << "OBU_TILE_LIST"
    << "Reserved"
    << "Reserved"
    << "Reserved"
    << "Reserved"
    << "Reserved"
    << "Reserved"
    << "OBU_PADDING";
  unsigned int obu_type_idx;
  READBITS_M(obu_type_idx, 4, obu_type_id_meaning);
  if (obu_type_idx == 15)
    obu_type = OBU_PADDING;
  else if (obu_type_idx > 8)
    obu_type = RESERVED;
  else
    obu_type = (obu_type_enum)obu_type_idx;

  READFLAG(obu_extension_flag);
  READFLAG(obu_has_size_field);

  READZEROBITS(1, "obu_reserved_1bit");
  
  if (obu_extension_flag)
  {
    if (header_data.length() == 1)
      return reader.addErrorMessageChildItem("The OBU header has an obu_extension_header and must have at least two byte");
    READBITS(temporal_id, 3);
    READBITS(spatial_id, 2);

    READZEROBITS(3, "extension_header_reserved_3bits");
  }

  if (obu_has_size_field)
  {
    READLEB128(obu_size);
  }

  nrBytesHeader = reader.nrBytesRead();
  return true;
}

unsigned int parserAV1OBU::parseAndAddOBU(int obuID, QByteArray data, TreeItem *parent, QUint64Pair obuStartEndPosFile, QString *obuTypeName)
{
  // Use the given tree item. If it is not set, use the nalUnitMode (if active). 
  // We don't set data (a name) for this item yet. 
  // We want to parse the item and then set a good description.
  QString specificDescription;
  TreeItem *obuRoot = nullptr;
  if (parent)
    obuRoot = new TreeItem(parent);
  else if (!packetModel->isNull())
    obuRoot = new TreeItem(packetModel->getRootItem());

  // Read the OBU header
  obu_unit obu(obuStartEndPosFile, obuID);
  unsigned int nrBytesHeader;
  if (!obu.parse_obu_header(data, nrBytesHeader, obuRoot))
    return false;

  // Get the payload of the OBU
  QByteArray obuData = data.mid(nrBytesHeader, obu.obu_size);

  bool parsingSuccess = true;
  if (obu.obu_type == OBU_TEMPORAL_DELIMITER)
  {
    decValues.SeenFrameHeader = false;
  }
  if (obu.obu_type == OBU_SEQUENCE_HEADER)
  {
    // A sequence parameter set
    auto new_sequence_header = QSharedPointer<sequence_header>(new sequence_header(obu));
    parsingSuccess = new_sequence_header->parse_sequence_header(obuData, obuRoot);

    active_sequence_header = new_sequence_header;

    if (obuTypeName)
      *obuTypeName = parsingSuccess ? "SEQ_HEAD" : "SEQ_HEAD(ERR)";
  }
  else if (obu.obu_type == OBU_FRAME || obu.obu_type == OBU_FRAME_HEADER)
  {
    auto new_frame_header = QSharedPointer<frame_header>(new frame_header(obu));
    parsingSuccess = new_frame_header->parse_frame_header(obuData, obuRoot, active_sequence_header, decValues);

    if (obuTypeName)
      *obuTypeName = parsingSuccess ? "FRAME" : "FRAME(ERR)";
  }

  if (obuRoot)
    // Set a useful name of the TreeItem (the root for this NAL)
    obuRoot->itemData.append(QString("OBU %1: %2").arg(obu.obu_idx).arg(obu_type_toString.value(obu.obu_type)) + specificDescription);

  return nrBytesHeader + (int)obu.obu_size;
}

bool parserAV1OBU::sequence_header::parse_sequence_header(const QByteArray &sequenceHeaderData, TreeItem *root)
{
  obuPayload = sequenceHeaderData;
  reader_helper reader(sequenceHeaderData, root, "sequence_header_obu()");

  QStringList seq_profile_meaning = QStringList()
    << "Main Profile: Bit depth 8 or 10 bit, Monochrome support, Subsampling YUV 4:2:0"
    << "High Profile: Bit depth 8 or 10 bit, No Monochrome support, Subsampling YUV 4:2:0 or 4:4:4"
    << "Professional Profile: Bit depth 8, 10 or 12 bit, Monochrome support, Subsampling YUV 4:2:0, 4:2:2 or 4:4:4"
    << "Reserved";
  READBITS_M(seq_profile, 3, seq_profile_meaning);
  READFLAG(still_picture);
  READFLAG(reduced_still_picture_header);

  if (reduced_still_picture_header) 
  {
    timing_info_present_flag = false;
    decoder_model_info_present_flag = false;
    initial_display_delay_present_flag = false;
    operating_points_cnt_minus_1 = 0;
    operating_point_idc.append(0);
    READBITS_A(seq_level_idx, 5, 0);
    seq_tier.append(false);
    decoder_model_present_for_this_op.append(false);
    initial_display_delay_present_for_this_op.append(false);
  }
  else
  {
    READFLAG(timing_info_present_flag);
    if (timing_info_present_flag)
    {
      if (!timing_info.parse_timing_info(reader))
        return false;
      READFLAG(decoder_model_info_present_flag);
      if (decoder_model_info_present_flag)
        if (!decoder_model_info.parse_decoder_model(reader))
          return false;
    }
    else
    {
      decoder_model_info_present_flag = false;
    }
    READFLAG(initial_display_delay_present_flag);
    READBITS(operating_points_cnt_minus_1, 5);
    for (unsigned int i = 0; i <= operating_points_cnt_minus_1; i++)
    {
      READBITS_A(operating_point_idc, 12, i);
      READBITS_A(seq_level_idx, 5, i);
      if (seq_level_idx[i] > 7)
      {
        READFLAG_A(seq_tier, i);
      }
      else 
      {
        seq_tier.append(false);
      }
      if (decoder_model_info_present_flag)
      {
        READFLAG_A(decoder_model_present_for_this_op, i);
        if (decoder_model_present_for_this_op[i])
        {
          if (!operating_parameters_info.parse_operating_parameters_info(reader, i, decoder_model_info))
            return false;
        }
      }
      else
      {
        decoder_model_present_for_this_op.append(0);
      }
      if (initial_display_delay_present_flag)
      {
        READFLAG_A(initial_display_delay_present_for_this_op, i);
        if (initial_display_delay_present_for_this_op[i])
        {
          READBITS_A(initial_display_delay_minus_1, 4, i);
        }
      }
    }
  }

  operatingPoint = choose_operating_point();
  OperatingPointIdc = operating_point_idc[operatingPoint];

  READBITS(frame_width_bits_minus_1, 4);
  READBITS(frame_height_bits_minus_1, 4);
  READBITS(max_frame_width_minus_1, frame_width_bits_minus_1+1);
  READBITS(max_frame_height_minus_1, frame_height_bits_minus_1+1);
  if (reduced_still_picture_header)
    frame_id_numbers_present_flag = 0;
  else
    READFLAG(frame_id_numbers_present_flag);
  if (frame_id_numbers_present_flag)
  {
    READBITS(delta_frame_id_length_minus_2, 4);
    READBITS(additional_frame_id_length_minus_1, 3);
  }
  READFLAG(use_128x128_superblock);
  READFLAG(enable_filter_intra);
  READFLAG(enable_intra_edge_filter);

  if (reduced_still_picture_header)
  {
    enable_interintra_compound = false;
    enable_masked_compound = false;
    enable_warped_motion = false;
    enable_dual_filter = false;
    enable_order_hint = false;
    enable_jnt_comp = false;
    enable_ref_frame_mvs = false;
    seq_force_screen_content_tools = SELECT_SCREEN_CONTENT_TOOLS;
    seq_force_integer_mv = SELECT_INTEGER_MV;
    OrderHintBits = 0;
  }
  else
  {
    READFLAG(enable_interintra_compound);
    READFLAG(enable_masked_compound);
    READFLAG(enable_warped_motion);
    READFLAG(enable_dual_filter);
    READFLAG(enable_order_hint);
    if (enable_order_hint)
    {
      READFLAG(enable_jnt_comp);
      READFLAG(enable_ref_frame_mvs);
    }
    else
    {
      enable_jnt_comp = false;
      enable_ref_frame_mvs = false;
    }
    READFLAG(seq_choose_screen_content_tools);
    if (seq_choose_screen_content_tools)
    {
      seq_force_screen_content_tools = SELECT_SCREEN_CONTENT_TOOLS;
      LOGVAL(seq_force_screen_content_tools);
    }
    else
      READBITS(seq_force_screen_content_tools, 1);
    
    if (seq_force_screen_content_tools > 0)
    {
      READFLAG(seq_choose_integer_mv);
      if (seq_choose_integer_mv)
      {
        seq_force_integer_mv = SELECT_INTEGER_MV;
        LOGVAL(seq_force_integer_mv);
      }
      else
        READBITS(seq_force_integer_mv, 1);
    }
    else
    {
      seq_force_integer_mv = SELECT_INTEGER_MV;
      LOGVAL(seq_force_integer_mv);
    }
    if (enable_order_hint)
    {
      READBITS(order_hint_bits_minus_1, 3);
      OrderHintBits = order_hint_bits_minus_1 + 1;
    }
    else
    {
      OrderHintBits = 0;
    }
    LOGVAL(OrderHintBits);
  }

  READFLAG(enable_superres);
  READFLAG(enable_cdef);
  READFLAG(enable_restoration);

  if (!color_config.parse_color_config(reader, seq_profile))
    return false;

  READFLAG(film_grain_params_present);

  // Calculate the RFC6381 'Codecs' parameter as it is needed in DASH
  {
    // We add the parameters from back to front. The last parameters can be omitted if they are the default values
    QString rfc6381CodecsParameter, rfc6381CodecsParameterShortened;
    bool add = false;

    // videoFullRangeFlag (1 digit)
    QString range = (color_config.color_range ? ".1" : ".0");
    rfc6381CodecsParameter.prepend(range);
    add |= (range != ".0");
    if (add)
      rfc6381CodecsParameterShortened.prepend(range);

    // matrixCoefficients	(2 digits)
    QString matrix = (color_config.color_description_present_flag ? QString(".%1").arg(color_config.matrix_coefficients, 2, 10, QChar('0')) : ".01");
    rfc6381CodecsParameter.prepend(matrix);
    add |= (matrix != ".01");
    if (add)
      rfc6381CodecsParameterShortened.prepend(matrix);

    // transfer_characteristics (2 digits)
    QString transfer = (color_config.color_description_present_flag ? QString(".%1").arg(color_config.transfer_characteristics, 2, 10, QChar('0')) : ".01");
    rfc6381CodecsParameter.prepend(transfer);
    add |= (transfer != ".01");
    if (add)
      rfc6381CodecsParameterShortened.prepend(transfer);

    // color_primaries (2 digits)
    QString primaries = (color_config.color_description_present_flag ? QString(".%1").arg(color_config.color_primaries, 2, 10, QChar('0')) : ".01");
    rfc6381CodecsParameter.prepend(primaries);
    add |= (primaries != ".01");
    if (add)
      rfc6381CodecsParameterShortened.prepend(primaries);

    // chromaSubsampling (3 digits)
    QString subsampling = QString(".%1%2%3").arg(color_config.subsampling_x ? "1" : "0").arg(color_config.subsampling_y ? "1" : "0").arg(color_config.subsampling_x && color_config.subsampling_y ? color_config.chroma_sample_position : 0);
    rfc6381CodecsParameter.prepend(subsampling);
    add |= (subsampling != ".110");
    if (add)
      rfc6381CodecsParameterShortened.prepend(subsampling);

    // Monochrome (1 digit)
    QString monochrome = QString(".%1").arg(color_config.mono_chrome ? "1" : "0");
    rfc6381CodecsParameter.prepend(monochrome);
    add |= (monochrome != ".0");
    if (add)
      rfc6381CodecsParameterShortened.prepend(monochrome);

    // From here, all values are mandatory
    QString remainder = QString("av01.%1.%2%3.%4").arg(seq_profile).arg(seq_level_idx[0], 2, 10, QChar('0')).arg(seq_tier[0] ? "H" : "M").arg(color_config.BitDepth, 2, 10, QChar('0'));
    rfc6381CodecsParameter.prepend(remainder);
    rfc6381CodecsParameterShortened.prepend(remainder);
    
    LOGVAL(rfc6381CodecsParameter);
    LOGVAL(rfc6381CodecsParameterShortened);
  }

  return true;
}

bool parserAV1OBU::sequence_header::timing_info_struct::parse_timing_info(reader_helper &reader)
{
  reader_sub_level r(reader, "timing_info()");

  READBITS(num_units_in_display_tick, 32);
  READBITS(time_scale, 32);
  READFLAG(equal_picture_interval);
  if (equal_picture_interval)
    READUVLC(num_ticks_per_picture_minus_1);

  return true;
}

bool parserAV1OBU::sequence_header::decoder_model_info_struct::parse_decoder_model(reader_helper &reader)
{
  reader_sub_level r(reader, "decoder_model_info()");

  READBITS(buffer_delay_length_minus_1, 5);
  READBITS(num_units_in_decoding_tick, 32);
  READBITS(buffer_removal_time_length_minus_1, 5);
  READBITS(frame_presentation_time_length_minus_1, 5);

  return true;
}

bool parserAV1OBU::sequence_header::operating_parameters_info_struct::parse_operating_parameters_info(reader_helper &reader, int op, decoder_model_info_struct &dmodel)
{
  reader_sub_level r(reader, "operating_parameters_info()");

  int n = dmodel.buffer_delay_length_minus_1 + 1;
  READBITS_A(decoder_buffer_delay, n, op);
  READBITS_A(encoder_buffer_delay, n, op);
  READFLAG_A(low_delay_mode_flag, op);

  return false;
}

bool parserAV1OBU::sequence_header::color_config_struct::parse_color_config(reader_helper &reader, int seq_profile)
{
  reader_sub_level r(reader, "color_config()");

  READFLAG(high_bitdepth);
  if (seq_profile == 2 && high_bitdepth) 
  {
    READFLAG(twelve_bit);
    BitDepth = twelve_bit ? 12 : 10;
  }
  else if (seq_profile <= 2) 
  {
    BitDepth = high_bitdepth ? 10 : 8;
  }
  LOGVAL(BitDepth);

  if (seq_profile == 1) 
    mono_chrome = 0;
  else 
    READFLAG(mono_chrome);

  NumPlanes = mono_chrome ? 1 : 3;
  LOGVAL(NumPlanes);

  READFLAG(color_description_present_flag);
  if (color_description_present_flag) 
  {
    QStringList color_primaries_meaning = QStringList()
      << "Unused"
      << "BT.709" 
      << "Unspecified"
      << "Unused"
      << "BT.470 System M (historical)"
      << "BT.470 System B, G (historical)"
      << "BT.601"
      << "SMPTE 240"
      << "Generic film (color filters using illuminant C)"
      << "BT.2020, BT.2100"
      << "SMPTE 428 (CIE 1921 XYZ)"
      << "SMPTE RP 431-2"
      << "SMPTE EG 432-1"
      << "Unused" << "Unused" << "Unused" << "Unused" << "Unused" << "Unused" << "Unused" << "Unused" << "Unused"
      << "EBU Tech. 3213-E"
      << "Unused";
    READBITS_M_E(color_primaries, 8, color_primaries_meaning, color_primaries_enum);

    QStringList transfer_characteristics_meaning = QStringList()
      << "For future use"
      << "BT.709"
      << "Unspecified"
      << "For future use"
      << "BT.470 System M (historical)"
      << "BT.470 System B, G (historical)"
      << "BT.601"
      << "SMPTE 240 M"
      << "Linear"
      << "Logarithmic (100 : 1 range)"
      << "Logarithmic (100 * Sqrt(10) : 1 range)"
      << "IEC 61966-2-4"
      << "BT.1361"
      << "sRGB or sYCC"
      << "BT.2020 10-bit systems"
      << "BT.2020 12-bit systems"
      << "SMPTE ST 2084, ITU BT.2100 PQ"
      << "SMPTE ST 428"
      << "BT.2100 HLG, ARIB STD-B67"
      << "Unused";
    READBITS_M_E(transfer_characteristics, 8, transfer_characteristics_meaning, transfer_characteristics_enum);

    QStringList matrix_coefficients_meaning = QStringList()
      << "Identity matrix"
      << "BT.709"
      << "Unspecified"
      << "For future use"
      << "US FCC 73.628"
      << "BT.470 System B, G (historical)"
      << "BT.601"
      << "SMPTE 240 M"
      << "YCgCo"
      << "BT.2020 non-constant luminance, BT.2100 YCbCr"
      << "BT.2020 constant luminance"
      << "SMPTE ST 2085 YDzDx"
      << "Chromaticity-derived non-constant luminance"
      << "Chromaticity-derived constant luminance"
      << "BT.2100 ICtCp"
      << "Unused";
    READBITS_M_E(matrix_coefficients, 8, matrix_coefficients_meaning, matrix_coefficients_enum);
  }
  else
  {
    color_primaries = CP_UNSPECIFIED;
    transfer_characteristics = TC_UNSPECIFIED;
    matrix_coefficients = MC_UNSPECIFIED;
  }

  if (mono_chrome)
  {
    READFLAG(color_range);
    subsampling_x = true;
    subsampling_y = true;
    chroma_sample_position = CSP_UNKNOWN;
    separate_uv_delta_q = false;
  }
  else if (color_primaries == CP_BT_709 && transfer_characteristics == TC_SRGB && matrix_coefficients == MC_IDENTITY)
  {
    color_range = 1;
    subsampling_x = false;
    subsampling_y = false;
  }
  else
  {
    READFLAG(color_range);
    if (seq_profile == 0) 
    {
      subsampling_x = true;
      subsampling_y = true;
    } 
    else if (seq_profile == 1) 
    {
      subsampling_x = false;
      subsampling_y = false;
    }
    else
    {
      if (BitDepth == 12) 
      {
        READFLAG(subsampling_x);
        if (subsampling_x)
          READFLAG(subsampling_y);
        else
          subsampling_y = false;
      } 
      else 
      {
        subsampling_x = true;
        subsampling_y = false;
      }
    }
    if (subsampling_x && subsampling_y)
    {
      QStringList chroma_sample_position_meaning = QStringList()
        << "Unknown (in this case the source video transfer function must be signaled outside the AV1 bitstream)"
        << "Horizontally co-located with (0, 0) luma sample, vertical position in the middle between two luma samples"
        << "co-located with (0, 0) luma sample"
        << "Reserved";
      READBITS_M_E(chroma_sample_position, 2, chroma_sample_position_meaning, chroma_sample_position_enum);
    }
  }
  if (!mono_chrome)
    READFLAG(separate_uv_delta_q);

  QString subsamplingFormat = "Unknown";
  if (subsampling_x && subsampling_y)
    subsamplingFormat = "4:2:0";
  else if (subsampling_x && !subsampling_y)
    subsamplingFormat = "4:2:2";
  else if (!subsampling_x && !subsampling_y)
    subsamplingFormat = "4:4:4";
  LOGSTRVAL("Subsampling format", subsamplingFormat);

  return true;
}

bool parserAV1OBU::frame_header::parse_frame_header(const QByteArray &frameHeaderData, TreeItem *root, QSharedPointer<sequence_header> seq_header, global_decoding_values &decValues)
{
  obuPayload = frameHeaderData;
  reader_helper reader(frameHeaderData, root, "frame_header_obu()");

  if (decValues.SeenFrameHeader)
  {
    // TODO: Is it meant like this?
    //frame_header_copy();
    if (!parse_uncompressed_header(reader, seq_header, decValues))
      return false;
  } 
  else 
  {
    decValues.SeenFrameHeader = true;
    if (!parse_uncompressed_header(reader, seq_header, decValues))
      return false;
    if (show_existing_frame) 
    {
      // decode_frame_wrapup()
      decValues.SeenFrameHeader = false;
    } 
    else 
    {
      //TileNum = 0;
      decValues.SeenFrameHeader = true;
    }
  }

  return true;
}

bool parserAV1OBU::frame_header::parse_uncompressed_header(reader_helper &reader, QSharedPointer<sequence_header> seq_header, global_decoding_values &decValues)
{
  reader_sub_level r(reader, "uncompressed_header()");
  
  int idLen = -1;
  if (seq_header->frame_id_numbers_present_flag)
  {
    idLen = seq_header->additional_frame_id_length_minus_1 + seq_header->delta_frame_id_length_minus_2 + 3;
  }
  const unsigned int allFrames = (1 << NUM_REF_FRAMES) - 1;
  
  if (seq_header->reduced_still_picture_header)
  {
    show_existing_frame = false;
    frame_type = KEY_FRAME;
    FrameIsIntra = true;
    show_frame = true;
    showable_frame = false;
  }
  else
  {
    READFLAG(show_existing_frame);
    if (show_existing_frame)
    {
      READBITS(frame_to_show_map_idx, 3);
      if (seq_header->decoder_model_info_present_flag && !seq_header->timing_info.equal_picture_interval)
      {
        //temporal_point_info();
      }
      refresh_frame_flags = 0;
      if (seq_header->frame_id_numbers_present_flag) 
      {
        READBITS(display_frame_id, idLen);
      }
      frame_type = decValues.RefFrameType[frame_to_show_map_idx];
      if (frame_type == KEY_FRAME)
        refresh_frame_flags = allFrames;
      if (seq_header->film_grain_params_present)
      {
        //load_grain_params(frame_to_show_map_idx);
        assert(false);
      }
      return true;
    }

    QStringList frame_type_meaning = QStringList() << "KEY_FRAME" << "INTER_FRAME" << "INTRA_ONLY_FRAME" << "SWITCH_FRAME";
    READBITS_M_E(frame_type, 2, frame_type_meaning, frame_type_enum);
    FrameIsIntra = (frame_type == INTRA_ONLY_FRAME || frame_type == KEY_FRAME);
    READFLAG(show_frame);
    if (show_frame && seq_header->decoder_model_info_present_flag && !seq_header->timing_info.equal_picture_interval)
    {
      //temporal_point_info();
    }
    if (show_frame)
      showable_frame = frame_type != KEY_FRAME;
    else
      READFLAG(showable_frame);
    if (frame_type == SWITCH_FRAME || (frame_type == KEY_FRAME && show_frame))
      error_resilient_mode = true;
    else
      READFLAG(error_resilient_mode);
  }

  if (frame_type == KEY_FRAME && show_frame) 
  {
    for (int i = 0; i < NUM_REF_FRAMES; i++)
    {
      decValues.RefValid[i] = 0;
      decValues.RefOrderHint[i] = 0;
    }
    for (int i = 0; i < REFS_PER_FRAME; i++)
      decValues.OrderHints[LAST_FRAME + i] = 0;
  }

  READFLAG(disable_cdf_update);
  if (seq_header->seq_force_screen_content_tools == SELECT_SCREEN_CONTENT_TOOLS) 
    READBITS(allow_screen_content_tools, 1);
  else
    allow_screen_content_tools = seq_header->seq_force_screen_content_tools;

  if (allow_screen_content_tools)
  {
    if (seq_header->seq_force_integer_mv == SELECT_INTEGER_MV)
      READBITS(force_integer_mv, 1);
    else
      force_integer_mv = seq_header->seq_force_integer_mv;
  }
  else
    force_integer_mv = 0;
  
  if (FrameIsIntra)
    force_integer_mv = 1;
  
  if (seq_header->frame_id_numbers_present_flag)
  {
    decValues.PrevFrameID = decValues.current_frame_id;
    READBITS(current_frame_id, idLen);
    decValues.current_frame_id = current_frame_id;
    mark_ref_frames(idLen, seq_header, decValues);
  }
  else
    current_frame_id = 0;

  if (frame_type == SWITCH_FRAME)
    frame_size_override_flag = true;
  else if (seq_header->reduced_still_picture_header)
    frame_size_override_flag = false;
  else
    READFLAG(frame_size_override_flag);

  READBITS(order_hint, seq_header->OrderHintBits);
  OrderHint = order_hint;

  if (FrameIsIntra || error_resilient_mode)
    primary_ref_frame = PRIMARY_REF_NONE;
  else
    READBITS(primary_ref_frame, 3);

  if (seq_header->decoder_model_info_present_flag)
  {
    READFLAG(buffer_removal_time_present_flag);
    if (buffer_removal_time_present_flag)
    {
      for (unsigned int opNum = 0; opNum <= seq_header->operating_points_cnt_minus_1; opNum++)
      {
        if (seq_header->decoder_model_present_for_this_op[opNum])
        {
          opPtIdc = seq_header->operating_point_idc[opNum];
          inTemporalLayer = (opPtIdc >> temporal_id) & 1;
          inSpatialLayer = (opPtIdc >> (spatial_id + 8)) & 1;
          if (opPtIdc == 0 || (inTemporalLayer && inSpatialLayer))
          {
            int n = seq_header->decoder_model_info.buffer_removal_time_length_minus_1 + 1;
            READBITS_A(buffer_removal_time, n, opNum);
          }
        }
      }
    }
  }

  allow_high_precision_mv = false;
  use_ref_frame_mvs = false;
  allow_intrabc = false;

  if (frame_type == SWITCH_FRAME || (frame_type == KEY_FRAME && show_frame))
    refresh_frame_flags = allFrames;
  else
    READBITS(refresh_frame_flags, 8);
  
  if (!FrameIsIntra || refresh_frame_flags != allFrames)
  {
    if (error_resilient_mode && seq_header->enable_order_hint)
    {
      for (int i = 0; i < NUM_REF_FRAMES; i++) 
      {
        READBITS_A(ref_order_hint, seq_header->OrderHintBits, i);
        if (ref_order_hint[i] != decValues.RefOrderHint[i])
          decValues.RefValid[i] = false;
      } 
    }
  }

  if (frame_type == KEY_FRAME)
  {
    if (!parse_frame_size(reader, seq_header))
      return false;
    if (!parse_render_size(reader))
      return false;
    if (allow_screen_content_tools && UpscaledWidth == FrameWidth)
      READFLAG(allow_intrabc);
  }
  else
  {
    if (frame_type == INTRA_ONLY_FRAME)
    {
      if (!parse_frame_size(reader, seq_header))
        return false;
      if (!parse_render_size(reader))
        return false;
      if (allow_screen_content_tools && UpscaledWidth == FrameWidth)
        READFLAG(allow_intrabc);
    }
    else
    {
      if (!seq_header->enable_order_hint)
        frame_refs_short_signaling = false;
      else
      {
        READFLAG(frame_refs_short_signaling);
        if (frame_refs_short_signaling)
        {
          READBITS(last_frame_idx, 3);
          READBITS(gold_frame_idx, 3);
          if (!frame_refs.set_frame_refs(reader, seq_header->OrderHintBits, seq_header->enable_order_hint, last_frame_idx, gold_frame_idx, OrderHint, decValues))
            return false;
        }
      }
      for (int i = 0; i < REFS_PER_FRAME; i++)
      {
        if (!frame_refs_short_signaling)
        {
          unsigned int ref_frame_idx;
          READBITS(ref_frame_idx, 3);
          frame_refs.ref_frame_idx[i] = ref_frame_idx;
        }
        if (seq_header->frame_id_numbers_present_flag)
        {
          int n = seq_header->delta_frame_id_length_minus_2 + 2;
          READBITS(delta_frame_id_minus_1, n);
          unsigned int DeltaFrameId = delta_frame_id_minus_1 + 1;
          expectedFrameId[i] = ((current_frame_id + (1 << idLen) - DeltaFrameId) % (1 << idLen));
        }
      }
      if (frame_size_override_flag && !error_resilient_mode)
      {
        if (!parse_frame_size_with_refs(reader, seq_header, decValues))
          return false;
      }
      else
      {
        if (!parse_frame_size(reader, seq_header))
          return false;
        if (!parse_render_size(reader))
          return false;
      }
      if (force_integer_mv)
        allow_high_precision_mv = false;
      else
        READFLAG(allow_high_precision_mv);
      if (!read_interpolation_filter(reader))
        return false;
      READFLAG(is_motion_mode_switchable);
      if (error_resilient_mode || !seq_header->enable_ref_frame_mvs)
        use_ref_frame_mvs = false;
      else
        READFLAG(use_ref_frame_mvs);
    }
  }

  if (seq_header->reduced_still_picture_header || disable_cdf_update)
    disable_frame_end_update_cdf = true;
  else
    READFLAG(disable_frame_end_update_cdf);
  if (primary_ref_frame == PRIMARY_REF_NONE)
  {
    LOGINFO("init_non_coeff_cdfs()");
    LOGINFO("setup_past_independence()");
  }
  else
  {
    LOGINFO("load_cdfs(ref_frame_idx[primary_ref_frame])");
    LOGINFO("load_previous()");
  }
  if (use_ref_frame_mvs)
    LOGINFO("motion_field_estimation()");
  
  if (!tile_info.parse_tile_info(MiCols, MiRows, reader, seq_header))
    return false;
  if (!quantization_params.parse_quantization_params(reader, seq_header))
    return false;
  if (!segmentation_params.parse_segmentation_params(primary_ref_frame, reader))
    return false;
  if (!delta_q_params.parse_delta_q_params(quantization_params.base_q_idx, reader))
    return false;
  if (!delta_lf_params.parse_delta_lf_params(delta_q_params.delta_q_present, allow_intrabc, reader))
    return false;
  // if (primary_ref_frame == PRIMARY_REF_NONE)
  //   init_coeff_cdfs()
  // else
  //   load_previous_segment_ids();
  CodedLossless = true;
  for (int segmentId = 0; segmentId < MAX_SEGMENTS; segmentId++)
  {
    int qindex = get_qindex(true, segmentId);
    LosslessArray[segmentId] = (qindex == 0 && quantization_params.DeltaQYDc == 0 && quantization_params.DeltaQUAc == 0 && quantization_params.DeltaQUDc == 0 && quantization_params.DeltaQVAc == 0 && quantization_params.DeltaQVDc == 0);
    if (!LosslessArray[segmentId])
      CodedLossless = false;
    if (quantization_params.using_qmatrix)
    {
      if (LosslessArray[segmentId]) 
      {
        SegQMLevel[0][segmentId] = 15;
        SegQMLevel[1][segmentId] = 15;
        SegQMLevel[2][segmentId] = 15;
      }
      else
      {
        SegQMLevel[0][segmentId] = quantization_params.qm_y;
        SegQMLevel[1][segmentId] = quantization_params.qm_u;
        SegQMLevel[2][segmentId] = quantization_params.qm_v;
      } 
    }
  }
  AllLossless = CodedLossless && (FrameWidth == UpscaledWidth);
  if (!loop_filter_params.parse_loop_filter_params(CodedLossless, allow_intrabc, reader, seq_header))
    return false;
  if (!cdef_params.parse_cdef_params(CodedLossless, allow_intrabc, reader, seq_header))
    return false;
  // lr_params
  // read_tx_mode
  // frame_reference_mode
  // skip_mode_params

  if (FrameIsIntra || error_resilient_mode || !seq_header->enable_warped_motion)
    allow_warped_motion = false;
  else
    READFLAG(allow_warped_motion);
  READFLAG(reduced_tx_set);
  //global_motion_params( )
  //film_grain_params( )

  return true;
}

void parserAV1OBU::frame_header::mark_ref_frames(int idLen, QSharedPointer<sequence_header> seq_header, global_decoding_values &decValues)
{
  unsigned int diffLen = seq_header->delta_frame_id_length_minus_2 + 2;
  for (unsigned int i = 0; i < NUM_REF_FRAMES; i++)
  {
    if (current_frame_id > (unsigned int)(1 << diffLen))
    {
      if (decValues.RefFrameId[i] > (int)current_frame_id || decValues.RefFrameId[i] < ((int)current_frame_id - (1 << diffLen)))
        decValues.RefValid[i] = 0;
    }
    else
    {
      if (decValues.RefFrameId[i] > (int)current_frame_id && decValues.RefFrameId[i] < (int)((1 << idLen) + current_frame_id - (1 << diffLen)))
        decValues.RefValid[i] = 0;
    }
  }
}

bool parserAV1OBU::frame_header::parse_frame_size(reader_helper &reader, QSharedPointer<sequence_header> seq_header)
{
  reader_sub_level r(reader, "frame_size()");

  if (frame_size_override_flag)
  {
    READBITS(frame_width_minus_1, seq_header->frame_width_bits_minus_1+1);
    READBITS(frame_height_minus_1, seq_header->frame_height_bits_minus_1+1);
    FrameWidth = frame_width_minus_1 + 1;
    FrameHeight = frame_height_minus_1 + 1;
  }
  else
  {
    FrameWidth = seq_header->max_frame_width_minus_1 + 1;
    FrameHeight = seq_header->max_frame_height_minus_1 + 1;
  }

  LOGVAL(FrameWidth);
  LOGVAL(FrameHeight);

  if (!parse_superres_params(reader, seq_header))
    return false;
  compute_image_size();

  return true;
}

bool parserAV1OBU::frame_header::parse_superres_params(reader_helper &reader, QSharedPointer<sequence_header> seq_header)
{
  reader_sub_level r(reader, "superres_params()");
  
  if (seq_header->enable_superres)
    READFLAG(use_superres);
  else
    use_superres = false;

  if (use_superres)
  {
    READBITS(coded_denom, SUPERRES_DENOM_BITS);
    SuperresDenom = coded_denom + SUPERRES_DENOM_MIN;
  }
  else
    SuperresDenom = SUPERRES_NUM;

  UpscaledWidth = FrameWidth;
  FrameWidth =  (UpscaledWidth * SUPERRES_NUM + (SuperresDenom / 2)) / SuperresDenom;
  LOGVAL(FrameWidth);

  return true;
}

void parserAV1OBU::frame_header::compute_image_size()
{
  MiCols = 2 * ((FrameWidth + 7 ) >> 3);
  MiRows = 2 * ((FrameHeight + 7 ) >> 3);
}

bool parserAV1OBU::frame_header::parse_render_size(reader_helper &reader)
{
  reader_sub_level r(reader, "render_size()");
  
  READFLAG(render_and_frame_size_different);
  if (render_and_frame_size_different)
  {
    READBITS(render_width_minus_1, 16);
    READBITS(render_height_minus_1, 16);
    RenderWidth = render_width_minus_1 + 1;
    RenderHeight = render_height_minus_1 + 1;
  }
  else
  {
    RenderWidth = UpscaledWidth;
    RenderHeight = FrameHeight;
  }
  LOGVAL(RenderWidth);
  LOGVAL(RenderHeight);

  return true;
}

bool parserAV1OBU::frame_header::parse_frame_size_with_refs(reader_helper &reader, QSharedPointer<sequence_header> seq_header, global_decoding_values &decValues)
{
  reader_sub_level r(reader, "frame_size_with_refs()");
  
  bool ref_found = false;
  for (int i = 0; i < REFS_PER_FRAME; i++)
  {
    bool found_ref;
    READFLAG(found_ref);
    if (found_ref)
    {
      UpscaledWidth = decValues.RefUpscaledWidth[frame_refs.ref_frame_idx[i]];
      FrameWidth = UpscaledWidth;
      FrameHeight = decValues.RefFrameHeight[frame_refs.ref_frame_idx[i]];
      RenderWidth = decValues.RefRenderWidth[frame_refs.ref_frame_idx[i]];
      RenderHeight = decValues.RefRenderHeight[frame_refs.ref_frame_idx[i]];
      ref_found = true;
      LOGVAL(FrameWidth);
      LOGVAL(FrameHeight);
      LOGVAL(RenderWidth);
      LOGVAL(RenderHeight);
      break;
    }
  }
  if (!ref_found)
  {
    if (!parse_frame_size(reader, seq_header))
      return false;
    if (!parse_render_size(reader))
      return false;
  }
  else
  {
    if (!parse_superres_params(reader, seq_header))
      return false;
    compute_image_size();
  }

  return true;
}

bool parserAV1OBU::frame_header::frame_refs_struct::set_frame_refs(reader_helper &reader, int OrderHintBits, bool enable_order_hint, int last_frame_idx, int gold_frame_idx, int OrderHint, global_decoding_values &decValues)
{
  for (int i = 0; i < REFS_PER_FRAME; i++)
    ref_frame_idx[i] = -1;
  ref_frame_idx[LAST_FRAME - LAST_FRAME] = last_frame_idx;
  ref_frame_idx[GOLDEN_FRAME - LAST_FRAME] = gold_frame_idx;

  for (int i = 0; i < NUM_REF_FRAMES; i++)
    usedFrame[i] = false;
  usedFrame[last_frame_idx] = true;
  usedFrame[gold_frame_idx] = true;

  int curFrameHint = 1 << (OrderHintBits - 1);
  for (int i = 0; i < NUM_REF_FRAMES; i++)
    shiftedOrderHints[i] = curFrameHint + get_relative_dist(decValues.RefOrderHint[i], OrderHint, enable_order_hint, OrderHintBits);

  int lastOrderHint = shiftedOrderHints[last_frame_idx];
  if (lastOrderHint >= curFrameHint)
    return reader.addErrorMessageChildItem("It is a requirement of bitstream conformance that lastOrderHint is strictly less than curFrameHint.");

  int goldOrderHint = shiftedOrderHints[gold_frame_idx];
  if (goldOrderHint >= curFrameHint)
    return reader.addErrorMessageChildItem("It is a requirement of bitstream conformance that goldOrderHint is strictly less than curFrameHint.");

  int ref = find_latest_backward(curFrameHint);
  if (ref >= 0)
  {
    ref_frame_idx[ALTREF_FRAME - LAST_FRAME] = ref;
    usedFrame[ref] = true;
  }

  ref = find_earliest_backward(curFrameHint);
  if (ref >= 0) 
  {
    ref_frame_idx[BWDREF_FRAME - LAST_FRAME] = ref;
    usedFrame[ref] = true;
  }

  ref = find_earliest_backward(curFrameHint);
  if (ref >= 0)
  {
    ref_frame_idx[ALTREF2_FRAME - LAST_FRAME] = ref;
    usedFrame[ref] = true;
  }

  int Ref_Frame_List[REFS_PER_FRAME - 2] = {LAST2_FRAME, LAST3_FRAME, BWDREF_FRAME, ALTREF2_FRAME, ALTREF_FRAME};
  for (int i = 0; i < REFS_PER_FRAME - 2; i++)
  {
    int refFrame = Ref_Frame_List[i];
    if (ref_frame_idx[refFrame - LAST_FRAME] < 0)
    {
      ref = find_latest_forward(curFrameHint);
      if (ref >= 0)
      {
        ref_frame_idx[refFrame - LAST_FRAME] = ref;
        usedFrame[ref] = true;
      }
    }
  }

  // Finally, any remaining references are set to the reference frame with smallest output order as follows:
  ref = -1;
  for (int i = 0; i < NUM_REF_FRAMES; i++)
  {
    int hint = shiftedOrderHints[i];
    if (ref < 0 || hint < earliestOrderHint)
    {
      ref = i;
      earliestOrderHint = hint;
    }
  }
  for (unsigned int i = 0; i < REFS_PER_FRAME; i++)
  {
    if (ref_frame_idx[i] < 0 )
      ref_frame_idx[i] = ref;
  }

  return true;
}

int parserAV1OBU::frame_header::frame_refs_struct::find_latest_backward(int curFrameHint)
{
  int ref = -1;
  for (int i = 0; i < NUM_REF_FRAMES; i++) 
  {
    int hint = shiftedOrderHints[i];
    if (!usedFrame[i] && hint >= curFrameHint && (ref < 0 || hint >= latestOrderHint)) 
    {
      ref = i;
      latestOrderHint = hint;
    }
  }
  return ref;
}

int parserAV1OBU::frame_header::frame_refs_struct::find_earliest_backward(int curFrameHint)
{
  int ref = -1;
  for (int i = 0; i < NUM_REF_FRAMES; i++)
  {
    int hint = shiftedOrderHints[i];
    if (!usedFrame[i] && hint >= curFrameHint && (ref < 0 || hint < earliestOrderHint))
    {
      ref = i;
      earliestOrderHint = hint;
    }
  }
  return ref;
}

int parserAV1OBU::frame_header::frame_refs_struct::find_latest_forward(int curFrameHint)
{
  int ref = -1;
  for (int i = 0; i < NUM_REF_FRAMES; i++)
  {
    int hint = shiftedOrderHints[i];
    if (!usedFrame[i] && hint < curFrameHint && (ref < 0 || hint >= latestOrderHint))
    {
      ref = i;
      latestOrderHint = hint;
    }
  }
  return ref;
}

int parserAV1OBU::frame_header::frame_refs_struct::get_relative_dist(int a, int b, bool enable_order_hint, int OrderHintBits)
{
  if (!enable_order_hint)
    return 0;

  int diff = a - b;
  int m = 1 << (OrderHintBits - 1);
  diff = (diff & (m - 1)) - (diff & m);
  return diff;
}

bool parserAV1OBU::frame_header::read_interpolation_filter(reader_helper &reader)
{
  reader_sub_level r(reader, "read_interpolation_filter()");

  READFLAG(is_filter_switchable);
  if (is_filter_switchable)
    interpolation_filter = SWITCHABLE;
  else
  {
    QStringList interpolation_filter_meaning = QStringList() << "EIGHTTAP" << "EIGHTTAP_SMOOTH" << "EIGHTTAP_SHARP" << "BILINEAR" << "SWITCHABLE";
    READBITS_M_E(interpolation_filter, 2, interpolation_filter_meaning, interpolation_filter_enum);
  }

  return true;
}

int tile_log2(int blkSize, int target)
{ 
  int k;
  for (k=0; (blkSize << k) < target; k++)
  {
  }
  return k;
}

bool parserAV1OBU::frame_header::tile_info_struct::parse_tile_info(int MiCols, int MiRows, reader_helper &reader, QSharedPointer<sequence_header> seq_header)
{
  reader_sub_level r(reader, "tile_info()");
  
  sbCols = seq_header->use_128x128_superblock ? ( ( MiCols + 31 ) >> 5 ) : ( ( MiCols + 15 ) >> 4 );
  sbRows = seq_header->use_128x128_superblock ? ( ( MiRows + 31 ) >> 5 ) : ( ( MiRows + 15 ) >> 4 );
  sbShift = seq_header->use_128x128_superblock ? 5 : 4;
  int sbSize = sbShift + 2;
  maxTileWidthSb = MAX_TILE_WIDTH >> sbSize;
  maxTileAreaSb = MAX_TILE_AREA >> ( 2 * sbSize );
  minLog2TileCols = tile_log2(maxTileWidthSb, sbCols);
  maxLog2TileCols = tile_log2(1, std::min(sbCols, MAX_TILE_COLS));
  maxLog2TileRows = tile_log2(1, std::min(sbRows, MAX_TILE_ROWS));
  minLog2Tiles = std::max(minLog2TileCols, tile_log2(maxTileAreaSb, sbRows * sbCols));

  READFLAG(uniform_tile_spacing_flag);
  if (uniform_tile_spacing_flag)
  {
    TileColsLog2 = minLog2TileCols;
    while (TileColsLog2 < maxLog2TileCols)
    {
      READFLAG(increment_tile_cols_log2);
      if (increment_tile_cols_log2)
        TileColsLog2++;
      else
        break;
    }
    tileWidthSb = (sbCols + (1 << TileColsLog2) - 1) >> TileColsLog2;
    TileCols = 0;
    for (int startSb = 0; startSb < sbCols; startSb += tileWidthSb)
    {
      MiColStarts.append(startSb << sbShift);      
      TileCols++;
    }
    MiColStarts.append(MiCols);

    minLog2TileRows = std::max(minLog2Tiles - TileColsLog2, 0);
    TileRowsLog2 = minLog2TileRows;
    while (TileRowsLog2 < maxLog2TileRows)
    {
      bool increment_tile_rows_log2;
      READFLAG(increment_tile_rows_log2);
      if (increment_tile_rows_log2)
        TileRowsLog2++;
      else
        break;
    }
    tileHeightSb = (sbRows + (1 << TileRowsLog2) - 1) >> TileRowsLog2;
    TileRows = 0;
    for (int startSb = 0; startSb < sbRows; startSb += tileHeightSb)
    {
      MiRowStarts.append(startSb << sbShift);
      TileRows++;
    }
    MiRowStarts.append(MiRows);
  }
  else
  {
    widestTileSb = 0;
    TileCols = 0;
    for (int startSb = 0; startSb < sbCols;)
    {
      MiColStarts.append(startSb << sbShift);
      int maxWidth = std::min(sbCols - startSb, maxTileWidthSb);
      READNS(width_in_sbs_minus_1, maxWidth);
      int sizeSb = width_in_sbs_minus_1 + 1;
      widestTileSb = std::max(sizeSb, widestTileSb);
      startSb += sizeSb;
      TileCols++;
    }
    MiColStarts.append(MiCols);
    TileColsLog2 = tile_log2(1, TileCols);

    if (minLog2Tiles > 0)
      maxTileAreaSb = (sbRows * sbCols) >> (minLog2Tiles + 1);
    else
      maxTileAreaSb = sbRows * sbCols;
    maxTileHeightSb = std::max(maxTileAreaSb / widestTileSb, 1);

    TileRows = 0;
    for (int startSb = 0; startSb < sbRows;)
    {
      MiRowStarts.append(startSb << sbShift);
      int maxHeight = std::min(sbRows - startSb, maxTileHeightSb);
      READNS(height_in_sbs_minus_1, maxHeight);
      int sizeSb = height_in_sbs_minus_1 + 1;
      startSb += sizeSb;
      TileRows++;
    }
    MiRowStarts.append(MiRows);
    TileRowsLog2 = tile_log2(1, TileRows);
  }
  if (TileColsLog2 > 0 || TileRowsLog2 > 0)
  {
    READBITS(context_update_tile_id, TileRowsLog2 + TileColsLog2);
    READBITS(tile_size_bytes_minus_1, 2);
    TileSizeBytes = tile_size_bytes_minus_1 + 1;
  }
  else
    context_update_tile_id = 0;

  LOGVAL(TileColsLog2);
  LOGVAL(TileRowsLog2);

  return true;
}

bool parserAV1OBU::frame_header::quantization_params_struct::parse_quantization_params(reader_helper &reader, QSharedPointer<sequence_header> seq_header)
{
  reader_sub_level r(reader, "quantization_params()");
  
  READBITS(base_q_idx, 8);
  READDELTAQ(DeltaQYDc);
  if (seq_header->color_config.NumPlanes > 1)
  {
    if (seq_header->color_config.separate_uv_delta_q)
      READFLAG(diff_uv_delta);
    else
      diff_uv_delta = false;
    READDELTAQ(DeltaQUDc);
    READDELTAQ(DeltaQUAc);
    if (diff_uv_delta)
    {
      READDELTAQ(DeltaQVDc);
      READDELTAQ(DeltaQVAc);
    }
    else
    {
      DeltaQVDc = DeltaQUDc;
      DeltaQVAc = DeltaQUAc;
    }
  }
  else
  {
    DeltaQUDc = 0;
    DeltaQUAc = 0;
    DeltaQVDc = 0;
    DeltaQVAc = 0;
  }
  READFLAG(using_qmatrix);
  if (using_qmatrix)
  {
    READBITS(qm_y, 4);
    READBITS(qm_u, 4);
    if (!seq_header->color_config.separate_uv_delta_q)
      qm_v = qm_u;
    else
      READBITS(qm_v, 4);
  }
  return true;
}

bool parserAV1OBU::frame_header::quantization_params_struct::read_delta_q(QString deltaValName, int &delta_q, reader_helper &reader)
{
  reader_sub_level r(reader, deltaValName);

  bool delta_coded;
  READFLAG(delta_coded);
  if (delta_coded)
    READSU(delta_q, 1+6);
  else
    delta_q = 0;

  return true;
}

bool parserAV1OBU::frame_header::segmentation_params_struct::parse_segmentation_params(int primary_ref_frame, reader_helper &reader)
{
  reader_sub_level r(reader, "segmentation_params()");

  int Segmentation_Feature_Bits[SEG_LVL_MAX] = { 8, 6, 6, 6, 6, 3, 0, 0 };
  int Segmentation_Feature_Signed[SEG_LVL_MAX] = { 1, 1, 1, 1, 1, 0, 0, 0 };
  int Segmentation_Feature_Max[SEG_LVL_MAX] = { 255, MAX_LOOP_FILTER, MAX_LOOP_FILTER, MAX_LOOP_FILTER, MAX_LOOP_FILTER, 7, 0, 0 };

  READFLAG(segmentation_enabled);
  if (segmentation_enabled)
  {
    if (primary_ref_frame == PRIMARY_REF_NONE)
    {
      segmentation_update_map = 1;
      segmentation_temporal_update = 0;
      segmentation_update_data = 1;
    }
    else
    {
      READFLAG(segmentation_update_map);
      if (segmentation_update_map)
        READFLAG(segmentation_temporal_update);
      READFLAG(segmentation_update_data);
    }
    if (segmentation_update_data)
    {
      for (int i = 0; i < MAX_SEGMENTS; i++)
      {
        for (int j = 0; j < SEG_LVL_MAX; j++)
        {
          bool feature_enabled;
          READFLAG(feature_enabled);
          FeatureEnabled[i][j] = feature_enabled;
          unsigned int clippedValue = 0;
          if (feature_enabled)
          {
            int bitsToRead = Segmentation_Feature_Bits[j];
            int limit = Segmentation_Feature_Max[j];
            if (Segmentation_Feature_Signed[j])
            {
              int feature_value = 0;
              READSU(feature_value, 1+bitsToRead);
              clippedValue = clip(feature_value, -limit, limit);
            }
            else
            {
              unsigned int feature_value = 0;
              READBITS(feature_value, bitsToRead);
              clippedValue = clip((int)feature_value, 0, limit);
            }
          }
          FeatureData[i][j] = clippedValue;
        }
      }
    }
  }
  else
  {
    for (int i = 0; i < MAX_SEGMENTS; i++)
    {
      for (int j = 0; j < SEG_LVL_MAX; j++)
      {
        FeatureEnabled[i][j] = 0;
        FeatureData[i][j] = 0;
      }
    }
  }
  SegIdPreSkip = false;
  LastActiveSegId = 0;
  for (int i = 0; i < MAX_SEGMENTS; i++)
  {
    for (int j = 0; j < SEG_LVL_MAX; j++)
    {
      if (FeatureEnabled[i][j])
      {
        LastActiveSegId = i;
        if (j >= SEG_LVL_REF_FRAME)
          SegIdPreSkip = true;
      }
    }
  }

  return true;
}

bool parserAV1OBU::frame_header::delta_q_params_struct::parse_delta_q_params(int base_q_idx, reader_helper &reader)
{
  reader_sub_level r(reader, "delta_q_params()");
  
  delta_q_res = 0;
  delta_q_present = false;
  if (base_q_idx > 0)
    READFLAG(delta_q_present);
  if (delta_q_present)
    READBITS(delta_q_res, 2);
  
  return true;
}

bool parserAV1OBU::frame_header::delta_lf_params_struct::parse_delta_lf_params(bool delta_q_present, bool allow_intrabc, reader_helper &reader)
{
  reader_sub_level r(reader, "delta_lf_params()");
  
  delta_lf_present = false;
  delta_lf_res = 0;
  delta_lf_multi = false;
  if (delta_q_present)
  {
    if (!allow_intrabc)
      READFLAG(delta_lf_present);
    if (delta_lf_present)
    {
      READBITS(delta_lf_res, 2);
      READFLAG(delta_lf_multi);
    }
  }

  return true;
}

int parserAV1OBU::frame_header::get_qindex(bool ignoreDeltaQ, int segmentId) const
{
  // TODO: This must be set somewhere!
  int CurrentQIndex = 0;

  if (seg_feature_active_idx(segmentId, SEG_LVL_ALT_Q))
  {
    int data = segmentation_params.FeatureData[segmentId][SEG_LVL_ALT_Q];
    int qindex = quantization_params.base_q_idx + data;
    if (!ignoreDeltaQ && delta_q_params.delta_q_present)
      qindex = CurrentQIndex + data;
    return clip(qindex, 0, 255);
  }
  else if (!ignoreDeltaQ && delta_q_params.delta_q_present)
    return CurrentQIndex;
  return quantization_params.base_q_idx;
}

bool parserAV1OBU::frame_header::loop_filter_params_struct::parse_loop_filter_params(bool CodedLossless, bool allow_intrabc, reader_helper &reader, QSharedPointer<sequence_header> seq_header)
{
  reader_sub_level r(reader, "loop_filter_params()");
  
  if (CodedLossless || allow_intrabc)
  {
    loop_filter_level[0] = 0;
    loop_filter_level[1] = 0;
    loop_filter_ref_deltas[INTRA_FRAME] = 1;
    loop_filter_ref_deltas[LAST_FRAME] = 0;
    loop_filter_ref_deltas[LAST2_FRAME] = 0;
    loop_filter_ref_deltas[LAST3_FRAME] = 0;
    loop_filter_ref_deltas[BWDREF_FRAME] = 0;
    loop_filter_ref_deltas[GOLDEN_FRAME] = -1;
    loop_filter_ref_deltas[ALTREF_FRAME] = -1;
    loop_filter_ref_deltas[ALTREF2_FRAME] = -1;
    for (int i = 0; i < 2; i++)
      loop_filter_mode_deltas[i] = 0;
    return true;
  }
  READBITS(loop_filter_level[0], 6);
  READBITS(loop_filter_level[1], 6);
  if (seq_header->color_config.NumPlanes > 1)
  {
    if (loop_filter_level[0] || loop_filter_level[1])
    {
      READBITS(loop_filter_level[2], 6);
      READBITS(loop_filter_level[3], 6);
    }
  }
  READBITS(loop_filter_sharpness, 3);
  READFLAG(loop_filter_delta_enabled);
  if (loop_filter_delta_enabled)
  {
    READFLAG(loop_filter_delta_update);
    if (loop_filter_delta_update)
    {
      for (int i = 0; i < TOTAL_REFS_PER_FRAME; i++)
      {
        bool update_ref_delta;
        READFLAG(update_ref_delta);
        if (update_ref_delta)
          READSU(loop_filter_ref_deltas[i], 1+6);
      }
      for (int i = 0; i < 2; i++)
      {
        bool update_mode_delta;
        READFLAG(update_mode_delta);
        if (update_mode_delta)
          READSU(loop_filter_mode_deltas[i], 1+6);
      }
    }
  }

  return true;
}

bool parserAV1OBU::frame_header::cdef_params_struct::parse_cdef_params(bool CodedLossless, bool allow_intrabc, reader_helper &reader, QSharedPointer<sequence_header> seq_header)
{
  reader_sub_level r(reader, "cdef_params()");
  
  if (CodedLossless || allow_intrabc || !seq_header->enable_cdef)
  {
    cdef_bits = 0;
    cdef_y_pri_strength[0] = 0;
    cdef_y_sec_strength[0] = 0;
    cdef_uv_pri_strength[0] = 0;
    cdef_uv_sec_strength[0] = 0;
    CdefDamping = 3;
    return true;
  }
  READBITS(cdef_damping_minus_3, 2);
  CdefDamping = cdef_damping_minus_3 + 3;
  READBITS(cdef_bits, 2);
  for (int i = 0; i < (1 << cdef_bits); i++)
  {
    READBITS(cdef_y_pri_strength[i], 4);
    READBITS(cdef_y_sec_strength[i], 2);
    if (cdef_y_sec_strength[i] == 3)
      cdef_y_sec_strength[i]++;
    if (seq_header->color_config.NumPlanes > 1)
    {
      READBITS(cdef_uv_pri_strength[i], 4);
      READBITS(cdef_uv_sec_strength[i], 4);
      if (cdef_uv_sec_strength[i] == 3)
        cdef_uv_sec_strength[i]++;
    }
  }

  return true;
}