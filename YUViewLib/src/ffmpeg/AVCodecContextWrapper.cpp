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

#include "AVCodecContextWrapper.h"
#include <stdexcept>

namespace FFmpeg
{

namespace
{

typedef struct AVCodecContext_56
{
  const AVClass *av_class;
  int            log_level_offset;

  AVMediaType             codec_type;
  const struct AVCodec *  codec;
  char                    codec_name[32];
  AVCodecID               codec_id;
  unsigned int            codec_tag;
  unsigned int            stream_codec_tag;
  void *                  priv_data;
  struct AVCodecInternal *internal;
  void *                  opaque;
  int                     bit_rate;
  int                     bit_rate_tolerance;
  int                     global_quality;
  int                     compression_level;
  int                     flags;
  int                     flags2;
  uint8_t *               extradata;
  int                     extradata_size;
  AVRational              time_base;
  int                     ticks_per_frame;
  int                     delay;
  int                     width, height;
  int                     coded_width, coded_height;
  int                     gop_size;
  AVPixelFormat           pix_fmt;
  int                     me_method;
  void (*draw_horiz_band)(struct AVCodecContext *s,
                          const AVFrame *        src,
                          int                    offset[AV_NUM_DATA_POINTERS],
                          int                    y,
                          int                    type,
                          int                    height);
  AVPixelFormat (*get_format)(struct AVCodecContext *s, const enum AVPixelFormat *fmt);
  int                           max_b_frames;
  float                         b_quant_factor;
  int                           rc_strategy;
  int                           b_frame_strategy;
  float                         b_quant_offset;
  int                           has_b_frames;
  int                           mpeg_quant;
  float                         i_quant_factor;
  float                         i_quant_offset;
  float                         lumi_masking;
  float                         temporal_cplx_masking;
  float                         spatial_cplx_masking;
  float                         p_masking;
  float                         dark_masking;
  int                           slice_count;
  int                           prediction_method;
  int *                         slice_offset;
  AVRational                    sample_aspect_ratio;
  int                           me_cmp;
  int                           me_sub_cmp;
  int                           mb_cmp;
  int                           ildct_cmp;
  int                           dia_size;
  int                           last_predictor_count;
  int                           pre_me;
  int                           me_pre_cmp;
  int                           pre_dia_size;
  int                           me_subpel_quality;
  int                           dtg_active_format;
  int                           me_range;
  int                           intra_quant_bias;
  int                           inter_quant_bias;
  int                           slice_flags;
  int                           xvmc_acceleration;
  int                           mb_decision;
  uint16_t *                    intra_matrix;
  uint16_t *                    inter_matrix;
  int                           scenechange_threshold;
  int                           noise_reduction;
  int                           me_threshold;
  int                           mb_threshold;
  int                           intra_dc_precision;
  int                           skip_top;
  int                           skip_bottom;
  float                         border_masking;
  int                           mb_lmin;
  int                           mb_lmax;
  int                           me_penalty_compensation;
  int                           bidir_refine;
  int                           brd_scale;
  int                           keyint_min;
  int                           refs;
  int                           chromaoffset;
  int                           scenechange_factor;
  int                           mv0_threshold;
  int                           b_sensitivity;
  AVColorPrimaries              color_primaries;
  AVColorTransferCharacteristic color_trc;
  AVColorSpace                  colorspace;
  AVColorRange                  color_range;
  AVChromaLocation              chroma_sample_location;

  // Actually, there is more here, but the variables above are the only we need.
} AVCodecContext_56;

typedef struct AVCodecContext_57
{
  const AVClass *         av_class;
  int                     log_level_offset;
  AVMediaType             codec_type;
  const struct AVCodec *  codec;
  char                    codec_name[32];
  AVCodecID               codec_id;
  unsigned int            codec_tag;
  unsigned int            stream_codec_tag;
  void *                  priv_data;
  struct AVCodecInternal *internal;
  void *                  opaque;
  int64_t                 bit_rate;
  int                     bit_rate_tolerance;
  int                     global_quality;
  int                     compression_level;
  int                     flags;
  int                     flags2;
  uint8_t *               extradata;
  int                     extradata_size;
  AVRational              time_base;
  int                     ticks_per_frame;
  int                     delay;
  int                     width, height;
  int                     coded_width, coded_height;
  int                     gop_size;
  AVPixelFormat           pix_fmt;
  int                     me_method;
  void (*draw_horiz_band)(struct AVCodecContext *s,
                          const AVFrame *        src,
                          int                    offset[AV_NUM_DATA_POINTERS],
                          int                    y,
                          int                    type,
                          int                    height);
  AVPixelFormat (*get_format)(struct AVCodecContext *s, const AVPixelFormat *fmt);
  int                           max_b_frames;
  float                         b_quant_factor;
  int                           rc_strategy;
  int                           b_frame_strategy;
  float                         b_quant_offset;
  int                           has_b_frames;
  int                           mpeg_quant;
  float                         i_quant_factor;
  float                         i_quant_offset;
  float                         lumi_masking;
  float                         temporal_cplx_masking;
  float                         spatial_cplx_masking;
  float                         p_masking;
  float                         dark_masking;
  int                           slice_count;
  int                           prediction_method;
  int *                         slice_offset;
  AVRational                    sample_aspect_ratio;
  int                           me_cmp;
  int                           me_sub_cmp;
  int                           mb_cmp;
  int                           ildct_cmp;
  int                           dia_size;
  int                           last_predictor_count;
  int                           pre_me;
  int                           me_pre_cmp;
  int                           pre_dia_size;
  int                           me_subpel_quality;
  int                           dtg_active_format;
  int                           me_range;
  int                           intra_quant_bias;
  int                           inter_quant_bias;
  int                           slice_flags;
  int                           xvmc_acceleration;
  int                           mb_decision;
  uint16_t *                    intra_matrix;
  uint16_t *                    inter_matrix;
  int                           scenechange_threshold;
  int                           noise_reduction;
  int                           me_threshold;
  int                           mb_threshold;
  int                           intra_dc_precision;
  int                           skip_top;
  int                           skip_bottom;
  float                         border_masking;
  int                           mb_lmin;
  int                           mb_lmax;
  int                           me_penalty_compensation;
  int                           bidir_refine;
  int                           brd_scale;
  int                           keyint_min;
  int                           refs;
  int                           chromaoffset;
  int                           scenechange_factor;
  int                           mv0_threshold;
  int                           b_sensitivity;
  AVColorPrimaries              color_primaries;
  AVColorTransferCharacteristic color_trc;
  AVColorSpace                  colorspace;
  AVColorRange                  color_range;
  AVChromaLocation              chroma_sample_location;

  // Actually, there is more here, but the variables above are the only we need.
} AVCodecContext_57;

typedef struct AVCodecContext_58
{
  const AVClass *         av_class;
  int                     log_level_offset;
  enum AVMediaType        codec_type;
  const struct AVCodec *  codec;
  enum AVCodecID          codec_id;
  unsigned int            codec_tag;
  void *                  priv_data;
  struct AVCodecInternal *internal;
  void *                  opaque;
  int64_t                 bit_rate;
  int                     bit_rate_tolerance;
  int                     global_quality;
  int                     compression_level;
  int                     flags;
  int                     flags2;
  uint8_t *               extradata;
  int                     extradata_size;
  AVRational              time_base;
  int                     ticks_per_frame;
  int                     delay;
  int                     width, height;
  int                     coded_width, coded_height;
  int                     gop_size;
  enum AVPixelFormat      pix_fmt;
  void (*draw_horiz_band)(struct AVCodecContext *s,
                          const AVFrame *        src,
                          int                    offset[AV_NUM_DATA_POINTERS],
                          int                    y,
                          int                    type,
                          int                    height);
  enum AVPixelFormat (*get_format)(struct AVCodecContext *s, const enum AVPixelFormat *fmt);
  int                                max_b_frames;
  float                              b_quant_factor;
  int                                b_frame_strategy;
  float                              b_quant_offset;
  int                                has_b_frames;
  int                                mpeg_quant;
  float                              i_quant_factor;
  float                              i_quant_offset;
  float                              lumi_masking;
  float                              temporal_cplx_masking;
  float                              spatial_cplx_masking;
  float                              p_masking;
  float                              dark_masking;
  int                                slice_count;
  int                                prediction_method;
  int *                              slice_offset;
  AVRational                         sample_aspect_ratio;
  int                                me_cmp;
  int                                me_sub_cmp;
  int                                mb_cmp;
  int                                ildct_cmp;
  int                                dia_size;
  int                                last_predictor_count;
  int                                pre_me;
  int                                me_pre_cmp;
  int                                pre_dia_size;
  int                                me_subpel_quality;
  int                                me_range;
  int                                slice_flags;
  int                                mb_decision;
  uint16_t *                         intra_matrix;
  uint16_t *                         inter_matrix;
  int                                scenechange_threshold;
  int                                noise_reduction;
  int                                intra_dc_precision;
  int                                skip_top;
  int                                skip_bottom;
  int                                mb_lmin;
  int                                mb_lmax;
  int                                me_penalty_compensation;
  int                                bidir_refine;
  int                                brd_scale;
  int                                keyint_min;
  int                                refs;
  int                                chromaoffset;
  int                                mv0_threshold;
  int                                b_sensitivity;
  enum AVColorPrimaries              color_primaries;
  enum AVColorTransferCharacteristic color_trc;
  enum AVColorSpace                  colorspace;
  enum AVColorRange                  color_range;
  enum AVChromaLocation              chroma_sample_location;
  int                                slices;

  // Actually, there is more here, but the variables above are the only we need.
} AVCodecContext_58;

typedef struct AVCodecContext_59_60
{
  const AVClass *         av_class;
  int                     log_level_offset;
  enum AVMediaType        codec_type;
  const struct AVCodec *  codec;
  enum AVCodecID          codec_id;
  unsigned int            codec_tag;
  void *                  priv_data;
  struct AVCodecInternal *internal;
  void *                  opaque;
  int64_t                 bit_rate;
  int                     bit_rate_tolerance;
  int                     global_quality;
  int                     compression_level;
  int                     flags;
  int                     flags2;
  uint8_t *               extradata;
  int                     extradata_size;
  AVRational              time_base;
  int                     ticks_per_frame;
  int                     delay;
  int                     width, height;
  int                     coded_width, coded_height;
  int                     gop_size;
  enum AVPixelFormat      pix_fmt;
  void (*draw_horiz_band)(struct AVCodecContext *s,
                          const AVFrame *        src,
                          int                    offset[AV_NUM_DATA_POINTERS],
                          int                    y,
                          int                    type,
                          int                    height);
  enum AVPixelFormat (*get_format)(struct AVCodecContext *s, const enum AVPixelFormat *fmt);
  int                                max_b_frames;
  float                              b_quant_factor;
  float                              b_quant_offset;
  int                                has_b_frames;
  float                              i_quant_factor;
  float                              i_quant_offset;
  float                              lumi_masking;
  float                              temporal_cplx_masking;
  float                              spatial_cplx_masking;
  float                              p_masking;
  float                              dark_masking;
  int                                slice_count;
  int *                              slice_offset;
  AVRational                         sample_aspect_ratio;
  int                                me_cmp;
  int                                me_sub_cmp;
  int                                mb_cmp;
  int                                ildct_cmp;
  int                                dia_size;
  int                                last_predictor_count;
  int                                me_pre_cmp;
  int                                pre_dia_size;
  int                                me_subpel_quality;
  int                                me_range;
  int                                slice_flags;
  int                                mb_decision;
  uint16_t *                         intra_matrix;
  uint16_t *                         inter_matrix;
  int                                intra_dc_precision;
  int                                skip_top;
  int                                skip_bottom;
  int                                mb_lmin;
  int                                mb_lmax;
  int                                bidir_refine;
  int                                keyint_min;
  int                                refs;
  int                                mv0_threshold;
  enum AVColorPrimaries              color_primaries;
  enum AVColorTransferCharacteristic color_trc;
  enum AVColorSpace                  colorspace;
  enum AVColorRange                  color_range;
  enum AVChromaLocation              chroma_sample_location;
  int                                slices;

  // Actually, there is more here, but the variables above are the only we need.
} AVCodecContext_59_60;

} // namespace

AVCodecContextWrapper::AVCodecContextWrapper()
{
  this->codec = nullptr;
}

AVCodecContextWrapper::AVCodecContextWrapper(AVCodecContext *c, LibraryVersion v)
{
  this->codec  = c;
  this->libVer = v;
  this->update();
}

AVMediaType AVCodecContextWrapper::getCodecType()
{
  this->update();
  return this->codec_type;
}

AVCodecID AVCodecContextWrapper::getCodecID()
{
  this->update();
  return this->codec_id;
}

AVCodecContext *AVCodecContextWrapper::getCodec()
{
  return this->codec;
}

AVPixelFormat AVCodecContextWrapper::getPixelFormat()
{
  this->update();
  return this->pix_fmt;
}

Size AVCodecContextWrapper::getSize()
{
  this->update();
  return {this->width, this->height};
}

AVColorSpace AVCodecContextWrapper::getColorspace()
{
  this->update();
  return this->colorspace;
}

AVRational AVCodecContextWrapper::getTimeBase()
{
  this->update();
  return this->time_base;
}

QByteArray AVCodecContextWrapper::getExtradata()
{
  this->update();
  return this->extradata;
}

void AVCodecContextWrapper::update()
{
  if (this->codec == nullptr)
    return;

  if (this->libVer.avcodec.major == 56)
  {
    auto p                        = reinterpret_cast<AVCodecContext_56 *>(this->codec);
    this->codec_type              = p->codec_type;
    this->codec_name              = QString(p->codec_name);
    this->codec_id                = p->codec_id;
    this->codec_tag               = p->codec_tag;
    this->stream_codec_tag        = p->stream_codec_tag;
    this->bit_rate                = p->bit_rate;
    this->bit_rate_tolerance      = p->bit_rate_tolerance;
    this->global_quality          = p->global_quality;
    this->compression_level       = p->compression_level;
    this->flags                   = p->flags;
    this->flags2                  = p->flags2;
    this->extradata               = QByteArray((const char *)p->extradata, p->extradata_size);
    this->time_base               = p->time_base;
    this->ticks_per_frame         = p->ticks_per_frame;
    this->delay                   = p->delay;
    this->width                   = p->width;
    this->height                  = p->height;
    this->coded_width             = p->coded_width;
    this->coded_height            = p->coded_height;
    this->gop_size                = p->gop_size;
    this->pix_fmt                 = p->pix_fmt;
    this->me_method               = p->me_method;
    this->max_b_frames            = p->max_b_frames;
    this->b_quant_factor          = p->b_quant_factor;
    this->rc_strategy             = p->rc_strategy;
    this->b_frame_strategy        = p->b_frame_strategy;
    this->b_quant_offset          = p->b_quant_offset;
    this->has_b_frames            = p->has_b_frames;
    this->mpeg_quant              = p->mpeg_quant;
    this->i_quant_factor          = p->i_quant_factor;
    this->i_quant_offset          = p->i_quant_offset;
    this->lumi_masking            = p->lumi_masking;
    this->temporal_cplx_masking   = p->temporal_cplx_masking;
    this->spatial_cplx_masking    = p->spatial_cplx_masking;
    this->p_masking               = p->p_masking;
    this->dark_masking            = p->dark_masking;
    this->slice_count             = p->slice_count;
    this->prediction_method       = p->prediction_method;
    this->sample_aspect_ratio     = p->sample_aspect_ratio;
    this->me_cmp                  = p->me_cmp;
    this->me_sub_cmp              = p->me_sub_cmp;
    this->mb_cmp                  = p->mb_cmp;
    this->ildct_cmp               = p->ildct_cmp;
    this->dia_size                = p->dia_size;
    this->last_predictor_count    = p->last_predictor_count;
    this->pre_me                  = p->pre_me;
    this->me_pre_cmp              = p->me_pre_cmp;
    this->pre_dia_size            = p->pre_dia_size;
    this->me_subpel_quality       = p->me_subpel_quality;
    this->dtg_active_format       = p->dtg_active_format;
    this->me_range                = p->me_range;
    this->intra_quant_bias        = p->intra_quant_bias;
    this->inter_quant_bias        = p->inter_quant_bias;
    this->slice_flags             = p->slice_flags;
    this->xvmc_acceleration       = p->xvmc_acceleration;
    this->mb_decision             = p->mb_decision;
    this->scenechange_threshold   = p->scenechange_threshold;
    this->noise_reduction         = p->noise_reduction;
    this->me_threshold            = p->me_threshold;
    this->mb_threshold            = p->mb_threshold;
    this->intra_dc_precision      = p->intra_dc_precision;
    this->skip_top                = p->skip_top;
    this->skip_bottom             = p->skip_bottom;
    this->border_masking          = p->border_masking;
    this->mb_lmin                 = p->mb_lmin;
    this->mb_lmax                 = p->mb_lmax;
    this->me_penalty_compensation = p->me_penalty_compensation;
    this->bidir_refine            = p->bidir_refine;
    this->brd_scale               = p->brd_scale;
    this->keyint_min              = p->keyint_min;
    this->refs                    = p->refs;
    this->chromaoffset            = p->chromaoffset;
    this->scenechange_factor      = p->scenechange_factor;
    this->mv0_threshold           = p->mv0_threshold;
    this->b_sensitivity           = p->b_sensitivity;
    this->color_primaries         = p->color_primaries;
    this->color_trc               = p->color_trc;
    this->colorspace              = p->colorspace;
    this->color_range             = p->color_range;
    this->chroma_sample_location  = p->chroma_sample_location;
  }
  else if (libVer.avcodec.major == 57)
  {
    auto p                        = reinterpret_cast<AVCodecContext_57 *>(this->codec);
    this->codec_type              = p->codec_type;
    this->codec_name              = QString(p->codec_name);
    this->codec_id                = p->codec_id;
    this->codec_tag               = p->codec_tag;
    this->stream_codec_tag        = p->stream_codec_tag;
    this->bit_rate                = p->bit_rate;
    this->bit_rate_tolerance      = p->bit_rate_tolerance;
    this->global_quality          = p->global_quality;
    this->compression_level       = p->compression_level;
    this->flags                   = p->flags;
    this->flags2                  = p->flags2;
    this->extradata               = QByteArray((const char *)p->extradata, p->extradata_size);
    this->time_base               = p->time_base;
    this->ticks_per_frame         = p->ticks_per_frame;
    this->delay                   = p->delay;
    this->width                   = p->width;
    this->height                  = p->height;
    this->coded_width             = p->coded_width;
    this->coded_height            = p->coded_height;
    this->gop_size                = p->gop_size;
    this->pix_fmt                 = p->pix_fmt;
    this->me_method               = p->me_method;
    this->max_b_frames            = p->max_b_frames;
    this->b_quant_factor          = p->b_quant_factor;
    this->rc_strategy             = p->rc_strategy;
    this->b_frame_strategy        = p->b_frame_strategy;
    this->b_quant_offset          = p->b_quant_offset;
    this->has_b_frames            = p->has_b_frames;
    this->mpeg_quant              = p->mpeg_quant;
    this->i_quant_factor          = p->i_quant_factor;
    this->i_quant_offset          = p->i_quant_offset;
    this->lumi_masking            = p->lumi_masking;
    this->temporal_cplx_masking   = p->temporal_cplx_masking;
    this->spatial_cplx_masking    = p->spatial_cplx_masking;
    this->p_masking               = p->p_masking;
    this->dark_masking            = p->dark_masking;
    this->slice_count             = p->slice_count;
    this->prediction_method       = p->prediction_method;
    this->sample_aspect_ratio     = p->sample_aspect_ratio;
    this->me_cmp                  = p->me_cmp;
    this->me_sub_cmp              = p->me_sub_cmp;
    this->mb_cmp                  = p->mb_cmp;
    this->ildct_cmp               = p->ildct_cmp;
    this->dia_size                = p->dia_size;
    this->last_predictor_count    = p->last_predictor_count;
    this->pre_me                  = p->pre_me;
    this->me_pre_cmp              = p->me_pre_cmp;
    this->pre_dia_size            = p->pre_dia_size;
    this->me_subpel_quality       = p->me_subpel_quality;
    this->dtg_active_format       = p->dtg_active_format;
    this->me_range                = p->me_range;
    this->intra_quant_bias        = p->intra_quant_bias;
    this->inter_quant_bias        = p->inter_quant_bias;
    this->slice_flags             = p->slice_flags;
    this->xvmc_acceleration       = p->xvmc_acceleration;
    this->mb_decision             = p->mb_decision;
    this->scenechange_threshold   = p->scenechange_threshold;
    this->noise_reduction         = p->noise_reduction;
    this->me_threshold            = p->me_threshold;
    this->mb_threshold            = p->mb_threshold;
    this->intra_dc_precision      = p->intra_dc_precision;
    this->skip_top                = p->skip_top;
    this->skip_bottom             = p->skip_bottom;
    this->border_masking          = p->border_masking;
    this->mb_lmin                 = p->mb_lmin;
    this->mb_lmax                 = p->mb_lmax;
    this->me_penalty_compensation = p->me_penalty_compensation;
    this->bidir_refine            = p->bidir_refine;
    this->brd_scale               = p->brd_scale;
    this->keyint_min              = p->keyint_min;
    this->refs                    = p->refs;
    this->chromaoffset            = p->chromaoffset;
    this->scenechange_factor      = p->scenechange_factor;
    this->mv0_threshold           = p->mv0_threshold;
    this->b_sensitivity           = p->b_sensitivity;
    this->color_primaries         = p->color_primaries;
    this->color_trc               = p->color_trc;
    this->colorspace              = p->colorspace;
    this->color_range             = p->color_range;
    this->chroma_sample_location  = p->chroma_sample_location;
  }
  else if (libVer.avcodec.major == 58)
  {
    auto p                        = reinterpret_cast<AVCodecContext_58 *>(this->codec);
    this->codec_type              = p->codec_type;
    this->codec_name              = QString("Not supported in AVCodec >= 58");
    this->codec_id                = p->codec_id;
    this->codec_tag               = p->codec_tag;
    this->stream_codec_tag        = -1;
    this->bit_rate                = p->bit_rate;
    this->bit_rate_tolerance      = p->bit_rate_tolerance;
    this->global_quality          = p->global_quality;
    this->compression_level       = p->compression_level;
    this->flags                   = p->flags;
    this->flags2                  = p->flags2;
    this->extradata               = QByteArray((const char *)p->extradata, p->extradata_size);
    this->time_base               = p->time_base;
    this->ticks_per_frame         = p->ticks_per_frame;
    this->delay                   = p->delay;
    this->width                   = p->width;
    this->height                  = p->height;
    this->coded_width             = p->coded_width;
    this->coded_height            = p->coded_height;
    this->gop_size                = p->gop_size;
    this->pix_fmt                 = p->pix_fmt;
    this->me_method               = -1;
    this->max_b_frames            = p->max_b_frames;
    this->b_quant_factor          = p->b_quant_factor;
    this->rc_strategy             = -1;
    this->b_frame_strategy        = p->b_frame_strategy;
    this->b_quant_offset          = p->b_quant_offset;
    this->has_b_frames            = p->has_b_frames;
    this->mpeg_quant              = p->mpeg_quant;
    this->i_quant_factor          = p->i_quant_factor;
    this->i_quant_offset          = p->i_quant_offset;
    this->lumi_masking            = p->lumi_masking;
    this->temporal_cplx_masking   = p->temporal_cplx_masking;
    this->spatial_cplx_masking    = p->spatial_cplx_masking;
    this->p_masking               = p->p_masking;
    this->dark_masking            = p->dark_masking;
    this->slice_count             = p->slice_count;
    this->prediction_method       = p->prediction_method;
    this->sample_aspect_ratio     = p->sample_aspect_ratio;
    this->me_cmp                  = p->me_cmp;
    this->me_sub_cmp              = p->me_sub_cmp;
    this->mb_cmp                  = p->mb_cmp;
    this->ildct_cmp               = p->ildct_cmp;
    this->dia_size                = p->dia_size;
    this->last_predictor_count    = p->last_predictor_count;
    this->pre_me                  = p->pre_me;
    this->me_pre_cmp              = p->me_pre_cmp;
    this->pre_dia_size            = p->pre_dia_size;
    this->me_subpel_quality       = p->me_subpel_quality;
    this->dtg_active_format       = -1;
    this->me_range                = p->me_range;
    this->intra_quant_bias        = -1;
    this->inter_quant_bias        = -1;
    this->slice_flags             = p->slice_flags;
    this->xvmc_acceleration       = -1;
    this->mb_decision             = p->mb_decision;
    this->scenechange_threshold   = p->scenechange_threshold;
    this->noise_reduction         = p->noise_reduction;
    this->me_threshold            = -1;
    this->mb_threshold            = -1;
    this->intra_dc_precision      = p->intra_dc_precision;
    this->skip_top                = p->skip_top;
    this->skip_bottom             = p->skip_bottom;
    this->border_masking          = -1;
    this->mb_lmin                 = p->mb_lmin;
    this->mb_lmax                 = p->mb_lmax;
    this->me_penalty_compensation = p->me_penalty_compensation;
    this->bidir_refine            = p->bidir_refine;
    this->brd_scale               = p->brd_scale;
    this->keyint_min              = p->keyint_min;
    this->refs                    = p->refs;
    this->chromaoffset            = p->chromaoffset;
    this->scenechange_factor      = -1;
    this->mv0_threshold           = p->mv0_threshold;
    this->b_sensitivity           = p->b_sensitivity;
    this->color_primaries         = p->color_primaries;
    this->color_trc               = p->color_trc;
    this->colorspace              = p->colorspace;
    this->color_range             = p->color_range;
    this->chroma_sample_location  = p->chroma_sample_location;
  }
  else if (libVer.avcodec.major == 59 || libVer.avcodec.major == 60)
  {
    auto p                        = reinterpret_cast<AVCodecContext_59_60 *>(this->codec);
    this->codec_type              = p->codec_type;
    this->codec_name              = QString("Not supported in AVCodec >= 58");
    this->codec_id                = p->codec_id;
    this->codec_tag               = p->codec_tag;
    this->stream_codec_tag        = -1;
    this->bit_rate                = p->bit_rate;
    this->bit_rate_tolerance      = p->bit_rate_tolerance;
    this->global_quality          = p->global_quality;
    this->compression_level       = p->compression_level;
    this->flags                   = p->flags;
    this->flags2                  = p->flags2;
    this->extradata               = QByteArray((const char *)p->extradata, p->extradata_size);
    this->time_base               = p->time_base;
    this->ticks_per_frame         = p->ticks_per_frame;
    this->delay                   = p->delay;
    this->width                   = p->width;
    this->height                  = p->height;
    this->coded_width             = p->coded_width;
    this->coded_height            = p->coded_height;
    this->gop_size                = p->gop_size;
    this->pix_fmt                 = p->pix_fmt;
    this->me_method               = -1;
    this->max_b_frames            = p->max_b_frames;
    this->b_quant_factor          = p->b_quant_factor;
    this->rc_strategy             = -1;
    this->b_frame_strategy        = -1;
    this->b_quant_offset          = p->b_quant_offset;
    this->has_b_frames            = p->has_b_frames;
    this->mpeg_quant              = -1;
    this->i_quant_factor          = p->i_quant_factor;
    this->i_quant_offset          = p->i_quant_offset;
    this->lumi_masking            = p->lumi_masking;
    this->temporal_cplx_masking   = p->temporal_cplx_masking;
    this->spatial_cplx_masking    = p->spatial_cplx_masking;
    this->p_masking               = p->p_masking;
    this->dark_masking            = p->dark_masking;
    this->slice_count             = p->slice_count;
    this->prediction_method       = -1;
    this->sample_aspect_ratio     = p->sample_aspect_ratio;
    this->me_cmp                  = p->me_cmp;
    this->me_sub_cmp              = p->me_sub_cmp;
    this->mb_cmp                  = p->mb_cmp;
    this->ildct_cmp               = p->ildct_cmp;
    this->dia_size                = p->dia_size;
    this->last_predictor_count    = p->last_predictor_count;
    this->pre_me                  = -1;
    this->me_pre_cmp              = p->me_pre_cmp;
    this->pre_dia_size            = p->pre_dia_size;
    this->me_subpel_quality       = p->me_subpel_quality;
    this->dtg_active_format       = -1;
    this->me_range                = p->me_range;
    this->intra_quant_bias        = -1;
    this->inter_quant_bias        = -1;
    this->slice_flags             = p->slice_flags;
    this->xvmc_acceleration       = -1;
    this->mb_decision             = p->mb_decision;
    this->scenechange_threshold   = -1;
    this->noise_reduction         = -1;
    this->me_threshold            = -1;
    this->mb_threshold            = -1;
    this->intra_dc_precision      = p->intra_dc_precision;
    this->skip_top                = p->skip_top;
    this->skip_bottom             = p->skip_bottom;
    this->border_masking          = -1;
    this->mb_lmin                 = p->mb_lmin;
    this->mb_lmax                 = p->mb_lmax;
    this->me_penalty_compensation = -1;
    this->bidir_refine            = p->bidir_refine;
    this->brd_scale               = -1;
    this->keyint_min              = p->keyint_min;
    this->refs                    = p->refs;
    this->chromaoffset            = -1;
    this->scenechange_factor      = -1;
    this->mv0_threshold           = p->mv0_threshold;
    this->b_sensitivity           = -1;
    this->color_primaries         = p->color_primaries;
    this->color_trc               = p->color_trc;
    this->colorspace              = p->colorspace;
    this->color_range             = p->color_range;
    this->chroma_sample_location  = p->chroma_sample_location;
  }
  else
    throw std::runtime_error("Invalid library version");
}

} // namespace FFmpeg
