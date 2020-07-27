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

#include "FrameHeader.h"

#include "parser/common/ParserMacros.h"
#include "parser/common/ReaderHelper.h"

namespace AV1
{

constexpr auto NUM_REF_FRAMES = 8;
constexpr auto REFS_PER_FRAME = 7;
constexpr auto PRIMARY_REF_NONE = 7;
constexpr auto MAX_SEGMENTS = 8;
constexpr auto SEG_LVL_MAX = 8;
constexpr auto SEG_LVL_REF_FRAME = 5;
constexpr auto MAX_LOOP_FILTER =63;
constexpr auto SEG_LVL_ALT_Q = 0;
constexpr auto TOTAL_REFS_PER_FRAME = 8;

constexpr auto SUPERRES_DENOM_BITS = 3;
constexpr auto SUPERRES_DENOM_MIN = 9;
constexpr auto SUPERRES_NUM = 8;

// The indices into the RefFrame list
constexpr auto INTRA_FRAME = 0;
constexpr auto LAST_FRAME = 1;
constexpr auto LAST2_FRAME = 2;
constexpr auto LAST3_FRAME = 3;
constexpr auto GOLDEN_FRAME = 4;
constexpr auto BWDREF_FRAME = 5;
constexpr auto ALTREF2_FRAME = 6;
constexpr auto ALTREF_FRAME = 7;

constexpr auto MAX_TILE_WIDTH = 4096;
constexpr auto MAX_TILE_AREA = 4096 * 2304;
constexpr auto MAX_TILE_COLS = 64;
constexpr auto MAX_TILE_ROWS = 64;

constexpr auto SELECT_SCREEN_CONTENT_TOOLS = 2;
constexpr auto SELECT_INTEGER_MV = 2;

#define READDELTAQ(into) do { if (!read_delta_q(#into, into, reader)) return false; } while(0)

FrameType indexToFrameType(unsigned index)
{
  switch(index)
  {
    case 0:
      return FrameType::KEY_FRAME;
    case 1:
      return FrameType::INTER_FRAME;
    case 2:
      return FrameType::INTRA_ONLY_FRAME;
    case 3:
      return FrameType::SWITCH_FRAME;
    default:
      return FrameType::UNSPECIFIED;
  }
}

bool FrameHeader::parse(const QByteArray &frameHeaderData, TreeItem *root, QSharedPointer<SequenceHeader> seq_header, GlobalDecodingValues &decValues)
{
  this->obuPayload = frameHeaderData;
  ReaderHelper reader(frameHeaderData, root, "frame_header_obu()");

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

bool FrameHeader::parse_uncompressed_header(ReaderHelper &reader, QSharedPointer<SequenceHeader> seq_header, GlobalDecodingValues &decValues)
{
  ReaderSubLevel r(reader, "uncompressed_header()");
  
  int idLen = -1;
  if (seq_header->frame_id_numbers_present_flag)
  {
    idLen = seq_header->additional_frame_id_length_minus_1 + seq_header->delta_frame_id_length_minus_2 + 3;
  }
  const unsigned int allFrames = (1 << NUM_REF_FRAMES) - 1;
  
  if (seq_header->reduced_still_picture_header)
  {
    show_existing_frame = false;
    frame_type = FrameType::KEY_FRAME;
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
      if (frame_type == FrameType::KEY_FRAME)
        refresh_frame_flags = allFrames;
      if (seq_header->film_grain_params_present)
      {
        //load_grain_params(frame_to_show_map_idx);
        assert(false);
      }
      return true;
    }

    QStringList frame_type_meaning = QStringList() << "FrameType::KEY_FRAME" << "INTER_FRAME" << "INTRA_ONLY_FRAME" << "SWITCH_FRAME";
    READBITS_M_F(frame_type, 2, frame_type_meaning, indexToFrameType);
    FrameIsIntra = (frame_type == FrameType::INTRA_ONLY_FRAME || frame_type == FrameType::KEY_FRAME);
    READFLAG(show_frame);
    if (show_frame && seq_header->decoder_model_info_present_flag && !seq_header->timing_info.equal_picture_interval)
    {
      //temporal_point_info();
    }
    if (show_frame)
      showable_frame = frame_type != FrameType::KEY_FRAME;
    else
      READFLAG(showable_frame);
    if (frame_type == FrameType::SWITCH_FRAME || (frame_type == FrameType::KEY_FRAME && show_frame))
      error_resilient_mode = true;
    else
      READFLAG(error_resilient_mode);
  }

  if (frame_type == FrameType::KEY_FRAME && show_frame) 
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

  if (frame_type == FrameType::SWITCH_FRAME)
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

  if (frame_type == FrameType::SWITCH_FRAME || (frame_type == FrameType::KEY_FRAME && show_frame))
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

  if (frame_type == FrameType::KEY_FRAME)
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
    if (frame_type == FrameType::INTRA_ONLY_FRAME)
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

void FrameHeader::mark_ref_frames(int idLen, QSharedPointer<SequenceHeader> seq_header, GlobalDecodingValues &decValues)
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

bool FrameHeader::parse_frame_size(ReaderHelper &reader, QSharedPointer<SequenceHeader> seq_header)
{
  ReaderSubLevel r(reader, "frame_size()");

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

bool FrameHeader::parse_superres_params(ReaderHelper &reader, QSharedPointer<SequenceHeader> seq_header)
{
  ReaderSubLevel r(reader, "superres_params()");
  
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

void FrameHeader::compute_image_size()
{
  MiCols = 2 * ((FrameWidth + 7 ) >> 3);
  MiRows = 2 * ((FrameHeight + 7 ) >> 3);
}

bool FrameHeader::parse_render_size(ReaderHelper &reader)
{
  ReaderSubLevel r(reader, "render_size()");
  
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

bool FrameHeader::parse_frame_size_with_refs(ReaderHelper &reader, QSharedPointer<SequenceHeader> seq_header, GlobalDecodingValues &decValues)
{
  ReaderSubLevel r(reader, "frame_size_with_refs()");
  
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

bool FrameHeader::frame_refs_struct::set_frame_refs(ReaderHelper &reader, int OrderHintBits, bool enable_order_hint, int last_frame_idx, int gold_frame_idx, int OrderHint, GlobalDecodingValues &decValues)
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

int FrameHeader::frame_refs_struct::find_latest_backward(int curFrameHint)
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

int FrameHeader::frame_refs_struct::find_earliest_backward(int curFrameHint)
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

int FrameHeader::frame_refs_struct::find_latest_forward(int curFrameHint)
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

int FrameHeader::frame_refs_struct::get_relative_dist(int a, int b, bool enable_order_hint, int OrderHintBits)
{
  if (!enable_order_hint)
    return 0;

  int diff = a - b;
  int m = 1 << (OrderHintBits - 1);
  diff = (diff & (m - 1)) - (diff & m);
  return diff;
}

bool FrameHeader::read_interpolation_filter(ReaderHelper &reader)
{
  ReaderSubLevel r(reader, "read_interpolation_filter()");

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

bool FrameHeader::tile_info_struct::parse_tile_info(int MiCols, int MiRows, ReaderHelper &reader, QSharedPointer<SequenceHeader> seq_header)
{
  ReaderSubLevel r(reader, "tile_info()");
  
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

bool FrameHeader::quantization_params_struct::parse_quantization_params(ReaderHelper &reader, QSharedPointer<SequenceHeader> seq_header)
{
  ReaderSubLevel r(reader, "quantization_params()");
  
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

bool FrameHeader::quantization_params_struct::read_delta_q(QString deltaValName, int &delta_q, ReaderHelper &reader)
{
  ReaderSubLevel r(reader, deltaValName);

  bool delta_coded;
  READFLAG(delta_coded);
  if (delta_coded)
    READSU(delta_q, 1+6);
  else
    delta_q = 0;

  return true;
}

bool FrameHeader::segmentation_params_struct::parse_segmentation_params(int primary_ref_frame, ReaderHelper &reader)
{
  ReaderSubLevel r(reader, "segmentation_params()");

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

bool FrameHeader::delta_q_params_struct::parse_delta_q_params(int base_q_idx, ReaderHelper &reader)
{
  ReaderSubLevel r(reader, "delta_q_params()");
  
  delta_q_res = 0;
  delta_q_present = false;
  if (base_q_idx > 0)
    READFLAG(delta_q_present);
  if (delta_q_present)
    READBITS(delta_q_res, 2);
  
  return true;
}

bool FrameHeader::delta_lf_params_struct::parse_delta_lf_params(bool delta_q_present, bool allow_intrabc, ReaderHelper &reader)
{
  ReaderSubLevel r(reader, "delta_lf_params()");
  
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

int FrameHeader::get_qindex(bool ignoreDeltaQ, int segmentId) const
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

bool FrameHeader::loop_filter_params_struct::parse_loop_filter_params(bool CodedLossless, bool allow_intrabc, ReaderHelper &reader, QSharedPointer<SequenceHeader> seq_header)
{
  ReaderSubLevel r(reader, "loop_filter_params()");
  
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

bool FrameHeader::cdef_params_struct::parse_cdef_params(bool CodedLossless, bool allow_intrabc, ReaderHelper &reader, QSharedPointer<SequenceHeader> seq_header)
{
  ReaderSubLevel r(reader, "cdef_params()");
  
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

}