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

#include "uncompressed_header.h"

#include "parser/common/functions.h"
#include "sequence_header_obu.h"
#include "typedef.h"

namespace parser::av1
{

using namespace reader;

void uncompressed_header::parse(SubByteReaderLogging &               reader,
                                std::shared_ptr<sequence_header_obu> seqHeader,
                                GlobalDecodingValues &               decValues,
                                unsigned                             temporal_id,
                                unsigned                             spatial_id)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "uncompressed_header()");

  int idLen = -1;
  if (seqHeader->frame_id_numbers_present_flag)
  {
    idLen = seqHeader->additional_frame_id_length_minus_1 +
            seqHeader->delta_frame_id_length_minus_2 + 3;
  }
  const auto allFrames = (1 << NUM_REF_FRAMES) - 1;

  if (seqHeader->reduced_still_picture_header)
  {
    this->show_existing_frame = false;
    this->frame_type          = FrameType::KEY_FRAME;
    this->FrameIsIntra        = true;
    this->show_frame          = true;
    this->showable_frame      = false;
  }
  else
  {
    this->show_existing_frame = reader.readFlag("show_existing_frame");
    if (show_existing_frame)
    {
      this->frame_to_show_map_idx = reader.readBits("frame_to_show_map_idx", 3);
      if (seqHeader->decoder_model_info_present_flag &&
          !seqHeader->timing_info.equal_picture_interval)
      {
        // temporal_point_info();
      }
      this->refresh_frame_flags = 0;
      if (seqHeader->frame_id_numbers_present_flag)
      {
        this->display_frame_id = reader.readBits("display_frame_id", idLen);
      }
      this->frame_type = decValues.RefFrameType[frame_to_show_map_idx];
      if (this->frame_type == FrameType::KEY_FRAME)
        this->refresh_frame_flags = allFrames;
      if (seqHeader->film_grain_params_present)
      {
        // load_grain_params(frame_to_show_map_idx);
        throw std::logic_error("Not implemented yet.");
      }
      return;
    }

    {
      auto frame_type_idx =
          reader.readBits("frame_type",
                          2,
                          Options().withMeaningVector(
                              {"KEY_FRAME", "INTER_FRAME", "INTRA_ONLY_FRAME", "SWITCH_FRAME"}));
      auto frame_type_coding = std::vector<FrameType>({FrameType::KEY_FRAME,
                                                       FrameType::INTER_FRAME,
                                                       FrameType::INTRA_ONLY_FRAME,
                                                       FrameType::SWITCH_FRAME});
      this->frame_type       = frame_type_coding[frame_type_idx];
    }

    this->FrameIsIntra = (this->frame_type == FrameType::INTRA_ONLY_FRAME ||
                          this->frame_type == FrameType::KEY_FRAME);
    this->show_frame   = reader.readFlag("show_frame");
    if (show_frame && seqHeader->decoder_model_info_present_flag &&
        !seqHeader->timing_info.equal_picture_interval)
    {
      // temporal_point_info();
      throw std::logic_error("Not implemented yet.");
    }
    if (this->show_frame)
      this->showable_frame = this->frame_type != FrameType::KEY_FRAME;
    else
      this->showable_frame = reader.readFlag("showable_frame");
    if (this->frame_type == FrameType::SWITCH_FRAME ||
        (this->frame_type == FrameType::KEY_FRAME && this->show_frame))
      this->error_resilient_mode = true;
    else
      this->error_resilient_mode = reader.readFlag("error_resilient_mode");
  }

  if (this->frame_type == FrameType::KEY_FRAME && this->show_frame)
  {
    for (unsigned i = 0; i < NUM_REF_FRAMES; i++)
    {
      decValues.RefValid[i]     = 0;
      decValues.RefOrderHint[i] = 0;
    }
    for (unsigned i = 0; i < REFS_PER_FRAME; i++)
      decValues.OrderHints[LAST_FRAME + i] = 0;
  }

  this->disable_cdf_update = reader.readFlag("disable_cdf_update");
  if (seqHeader->seq_force_screen_content_tools == SELECT_SCREEN_CONTENT_TOOLS)
    this->allow_screen_content_tools = reader.readBits("allow_screen_content_tools", 1);
  else
    this->allow_screen_content_tools = seqHeader->seq_force_screen_content_tools;

  if (this->allow_screen_content_tools)
  {
    if (seqHeader->seq_force_integer_mv == SELECT_INTEGER_MV)
      this->force_integer_mv = reader.readBits("force_integer_mv", 1);
    else
      this->force_integer_mv = seqHeader->seq_force_integer_mv;
  }
  else
    this->force_integer_mv = 0;

  if (this->FrameIsIntra)
    this->force_integer_mv = 1;

  if (seqHeader->frame_id_numbers_present_flag)
  {
    decValues.PrevFrameID      = decValues.current_frame_id;
    this->current_frame_id     = reader.readBits("current_frame_id", idLen);
    decValues.current_frame_id = this->current_frame_id;
    this->mark_ref_frames(idLen, seqHeader, decValues);
  }
  else
    this->current_frame_id = 0;

  if (this->frame_type == FrameType::SWITCH_FRAME)
    this->frame_size_override_flag = true;
  else if (seqHeader->reduced_still_picture_header)
    this->frame_size_override_flag = false;
  else
    this->frame_size_override_flag = reader.readFlag("frame_size_override_flag");

  this->order_hint = reader.readBits("order_hint", seqHeader->OrderHintBits);
  this->OrderHint  = this->order_hint;

  if (this->FrameIsIntra || this->error_resilient_mode)
    this->primary_ref_frame = PRIMARY_REF_NONE;
  else
    this->primary_ref_frame = reader.readBits("primary_ref_frame", 3);

  if (seqHeader->decoder_model_info_present_flag)
  {
    this->buffer_removal_time_present_flag = reader.readFlag("buffer_removal_time_present_flag");
    if (this->buffer_removal_time_present_flag)
    {
      for (unsigned opNum = 0; opNum <= seqHeader->operating_points_cnt_minus_1; opNum++)
      {
        if (seqHeader->decoder_model_present_for_this_op[opNum])
        {
          this->opPtIdc.push_back(seqHeader->operating_point_idc[opNum]);
          this->inTemporalLayer.push_back((opPtIdc[opNum] >> temporal_id) & 1);
          this->inSpatialLayer.push_back((opPtIdc[opNum] >> (spatial_id + 8)) & 1);
          if (this->opPtIdc[opNum] == 0 ||
              (this->inTemporalLayer[opNum] && this->inSpatialLayer[opNum]))
          {
            auto numBits = seqHeader->decoder_model_info.buffer_removal_time_length_minus_1 + 1;
            this->buffer_removal_time.push_back(
                reader.readBits(formatArray("buffer_removal_time", opNum), numBits));
          }
        }
      }
    }
  }

  this->allow_high_precision_mv = false;
  this->use_ref_frame_mvs       = false;
  this->allow_intrabc           = false;

  if (this->frame_type == FrameType::SWITCH_FRAME ||
      (this->frame_type == FrameType::KEY_FRAME && this->show_frame))
    this->refresh_frame_flags = allFrames;
  else
    this->refresh_frame_flags = reader.readBits("refresh_frame_flags", 8);

  if (!this->FrameIsIntra || this->refresh_frame_flags != allFrames)
  {
    if (this->error_resilient_mode && seqHeader->enable_order_hint)
    {
      for (unsigned i = 0; i < NUM_REF_FRAMES; i++)
      {
        this->ref_order_hint.push_back(
            reader.readBits(formatArray("ref_order_hint", i), seqHeader->OrderHintBits));
        if (this->ref_order_hint[i] != decValues.RefOrderHint[i])
          decValues.RefValid[i] = false;
      }
    }
  }

  if (this->frame_type == FrameType::KEY_FRAME)
  {
    frameSize.parse(reader, seqHeader, this->frame_size_override_flag);
    frameSize.renderSize.parse(reader, this->frameSize.UpscaledWidth, this->frameSize.FrameHeight);
    if (this->allow_screen_content_tools &&
        this->frameSize.UpscaledWidth == this->frameSize.FrameWidth)
      this->allow_intrabc = reader.readFlag("allow_intrabc");
  }
  else
  {
    if (frame_type == FrameType::INTRA_ONLY_FRAME)
    {
      this->frameSize.parse(reader, seqHeader, this->frame_size_override_flag);
      this->frameSize.renderSize.parse(
          reader, this->frameSize.UpscaledWidth, this->frameSize.FrameHeight);
      if (this->allow_screen_content_tools &&
          this->frameSize.UpscaledWidth == this->frameSize.FrameWidth)
        this->allow_intrabc = reader.readFlag("allow_intrabc");
    }
    else
    {
      if (!seqHeader->enable_order_hint)
        this->frame_refs_short_signaling = false;
      else
      {
        this->frame_refs_short_signaling = reader.readFlag("frame_refs_short_signaling");
        if (frame_refs_short_signaling)
        {
          this->last_frame_idx = reader.readBits("last_frame_idx", 3);
          this->gold_frame_idx = reader.readBits("gold_frame_idx", 3);
          this->frameRefs.set_frame_refs(seqHeader->OrderHintBits,
                                         seqHeader->enable_order_hint,
                                         this->last_frame_idx,
                                         this->gold_frame_idx,
                                         this->OrderHint,
                                         decValues);
        }
      }
      for (unsigned i = 0; i < REFS_PER_FRAME; i++)
      {
        if (!this->frame_refs_short_signaling)
        {
          this->frameRefs.ref_frame_idx[i] = reader.readBits("ref_frame_idx", 3);
        }
        if (seqHeader->frame_id_numbers_present_flag)
        {
          auto nrBits = seqHeader->delta_frame_id_length_minus_2 + 2;
          this->delta_frame_id_minus_1.push_back(reader.readBits("delta_frame_id_minus_1", nrBits));
          auto DeltaFrameId = this->delta_frame_id_minus_1[i] + 1;
          this->expectedFrameId.push_back(
              ((current_frame_id + (1 << idLen) - DeltaFrameId) % (1 << idLen)));
        }
      }
      if (frame_size_override_flag && !error_resilient_mode)
      {
        this->frameSize.parse_with_refs(
            reader, seqHeader, decValues, this->frameRefs, this->frame_size_override_flag);
      }
      else
      {
        frameSize.parse(reader, seqHeader, this->frame_size_override_flag);
        frameSize.renderSize.parse(
            reader, this->frameSize.UpscaledWidth, this->frameSize.FrameHeight);
      }
      if (force_integer_mv)
        allow_high_precision_mv = false;
      else
        this->allow_high_precision_mv = reader.readFlag("allow_high_precision_mv");
      interpolationFilter.parse(reader);
      this->is_motion_mode_switchable = reader.readFlag("is_motion_mode_switchable");
      if (this->error_resilient_mode || !seqHeader->enable_ref_frame_mvs)
        this->use_ref_frame_mvs = false;
      else
        this->use_ref_frame_mvs = reader.readFlag("use_ref_frame_mvs");
    }
  }

  if (seqHeader->reduced_still_picture_header || this->disable_cdf_update)
    this->disable_frame_end_update_cdf = true;
  else
    this->disable_frame_end_update_cdf = reader.readFlag("disable_frame_end_update_cdf");
  if (this->primary_ref_frame == PRIMARY_REF_NONE)
  {
    reader.logArbitrary("init_non_coeff_cdfs");
    reader.logArbitrary("setup_past_independence()");
  }
  else
  {
    reader.logArbitrary("load_cdfs(ref_frame_idx[primary_ref_frame])");
    reader.logArbitrary("load_previous()");
  }
  if (use_ref_frame_mvs)
    reader.logArbitrary("motion_field_estimation()");

  this->tileInfo.parse(reader, seqHeader, frameSize.MiCols, frameSize.MiRows);
  this->quantizationParams.parse(reader, seqHeader);
  this->segmentationParams.parse(reader, seqHeader, this->primary_ref_frame);
  this->deltaQParams.parse(reader, this->quantizationParams.base_q_idx);
  this->deltaLfParams.parse(reader, this->deltaQParams.delta_q_present, this->allow_intrabc);

  if (this->primary_ref_frame == PRIMARY_REF_NONE)
    reader.logArbitrary("init_coeff_cdfs()");
  else
    reader.logArbitrary("load_previous_segment_ids()");

  this->CodedLossless = true;
  for (unsigned segmentId = 0; segmentId < MAX_SEGMENTS; segmentId++)
  {
    auto qindex = this->get_qindex(true, segmentId);
    this->LosslessArray[segmentId] =
        (qindex == 0 && this->quantizationParams.DeltaQYDc == 0 &&
         this->quantizationParams.DeltaQUAc == 0 && this->quantizationParams.DeltaQUDc == 0 &&
         this->quantizationParams.DeltaQVAc == 0 && this->quantizationParams.DeltaQVDc == 0);
    if (!this->LosslessArray[segmentId])
      this->CodedLossless = false;
    if (this->quantizationParams.using_qmatrix)
    {
      if (this->LosslessArray[segmentId])
      {
        this->SegQMLevel[0][segmentId] = 15;
        this->SegQMLevel[1][segmentId] = 15;
        this->SegQMLevel[2][segmentId] = 15;
      }
      else
      {
        this->SegQMLevel[0][segmentId] = this->quantizationParams.qm_y;
        this->SegQMLevel[1][segmentId] = this->quantizationParams.qm_u;
        this->SegQMLevel[2][segmentId] = this->quantizationParams.qm_v;
      }
    }
  }
  this->AllLossless =
      this->CodedLossless && (this->frameSize.FrameWidth == this->frameSize.UpscaledWidth);

  this->loopFilterParams.parse(reader, seqHeader, this->CodedLossless, this->allow_intrabc);
  this->cdefParams.parse(reader, seqHeader, this->CodedLossless, this->allow_intrabc);
  // lr_params
  // read_tx_mode
  // frame_reference_mode
  // skip_mode_params

  if (this->FrameIsIntra || this->error_resilient_mode || !seqHeader->enable_warped_motion)
    this->allow_warped_motion = false;
  else
    this->allow_warped_motion = reader.readFlag("allow_warped_motion");
  this->reduced_tx_set = reader.readFlag("reduced_tx_set");

  // global_motion_params( )
  // film_grain_params( )
}

void uncompressed_header::mark_ref_frames(int                                  idLen,
                                          std::shared_ptr<sequence_header_obu> seqHeader,
                                          GlobalDecodingValues &               decValues)
{
  unsigned int diffLen = seqHeader->delta_frame_id_length_minus_2 + 2;
  for (unsigned int i = 0; i < NUM_REF_FRAMES; i++)
  {
    if (current_frame_id > (unsigned int)(1 << diffLen))
    {
      if (decValues.RefFrameId[i] > (int)current_frame_id ||
          decValues.RefFrameId[i] < ((int)current_frame_id - (1 << diffLen)))
        decValues.RefValid[i] = 0;
    }
    else
    {
      if (decValues.RefFrameId[i] > (int)current_frame_id &&
          decValues.RefFrameId[i] < (int)((1 << idLen) + current_frame_id - (1 << diffLen)))
        decValues.RefValid[i] = 0;
    }
  }
}

int uncompressed_header::get_qindex(bool ignoreDeltaQ, int segmentId) const
{
  // TODO: This must be set somewhere!
  int CurrentQIndex = 0;

  if (this->seg_feature_active_idx(segmentId, SEG_LVL_ALT_Q))
  {
    auto data   = this->segmentationParams.FeatureData[segmentId][SEG_LVL_ALT_Q];
    auto qindex = int(this->quantizationParams.base_q_idx) + data;
    if (!ignoreDeltaQ && this->deltaQParams.delta_q_present)
      qindex = CurrentQIndex + data;
    return clip(qindex, 0, 255);
  }
  else if (!ignoreDeltaQ && this->deltaQParams.delta_q_present)
    return CurrentQIndex;
  return this->quantizationParams.base_q_idx;
}

bool uncompressed_header::seg_feature_active_idx(int idx, int feature) const
{
  return this->segmentationParams.segmentation_enabled &&
         this->segmentationParams.FeatureEnabled[idx][feature];
}

} // namespace parser::av1