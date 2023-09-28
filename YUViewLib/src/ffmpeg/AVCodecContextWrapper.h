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

#pragma once

#include "FFMpegLibrariesTypes.h"
#include <common/Typedef.h>

namespace FFmpeg
{

class AVCodecContextWrapper
{
public:
  AVCodecContextWrapper();
  AVCodecContextWrapper(AVCodecContext *c, const LibraryVersions &libraryVersions);

  explicit operator bool() const { return this->codec != nullptr; };

  AVMediaType     getCodecType();
  AVCodecID       getCodecID();
  AVCodecContext *getCodec();
  AVPixelFormat   getPixelFormat();
  Size            getSize();
  AVColorSpace    getColorspace();
  AVRational      getTimeBase();
  QByteArray      getExtradata();

private:
  void update();

  // These are private. Use "update" to update them from the AVCodecContext
  AVMediaType   codec_type{};
  QString       codec_name{};
  AVCodecID     codec_id{};
  unsigned int  codec_tag{};
  unsigned int  stream_codec_tag{};
  int           bit_rate{};
  int           bit_rate_tolerance{};
  int           global_quality{};
  int           compression_level{};
  int           flags{};
  int           flags2{};
  QByteArray    extradata{};
  AVRational    time_base{};
  int           ticks_per_frame{};
  int           delay{};
  int           width, height{};
  int           coded_width, coded_height{};
  int           gop_size{};
  AVPixelFormat pix_fmt{};
  int           me_method{};
  int           max_b_frames{};
  float         b_quant_factor{};
  int           rc_strategy{};
  int           b_frame_strategy{};
  float         b_quant_offset{};
  int           has_b_frames{};
  int           mpeg_quant{};
  float         i_quant_factor{};
  float         i_quant_offset{};
  float         lumi_masking{};
  float         temporal_cplx_masking{};
  float         spatial_cplx_masking{};
  float         p_masking{};
  float         dark_masking{};
  int           slice_count{};
  int           prediction_method{};
  // int *slice_offset;
  AVRational sample_aspect_ratio{};
  int        me_cmp{};
  int        me_sub_cmp{};
  int        mb_cmp{};
  int        ildct_cmp{};
  int        dia_size{};
  int        last_predictor_count{};
  int        pre_me{};
  int        me_pre_cmp{};
  int        pre_dia_size{};
  int        me_subpel_quality{};
  int        dtg_active_format{};
  int        me_range{};
  int        intra_quant_bias{};
  int        inter_quant_bias{};
  int        slice_flags{};
  int        xvmc_acceleration{};
  int        mb_decision{};
  // uint16_t *intra_matrix;
  // uint16_t *inter_matrix;
  int                           scenechange_threshold{};
  int                           noise_reduction{};
  int                           me_threshold{};
  int                           mb_threshold{};
  int                           intra_dc_precision{};
  int                           skip_top{};
  int                           skip_bottom{};
  float                         border_masking{};
  int                           mb_lmin{};
  int                           mb_lmax{};
  int                           me_penalty_compensation{};
  int                           bidir_refine{};
  int                           brd_scale{};
  int                           keyint_min{};
  int                           refs{};
  int                           chromaoffset{};
  int                           scenechange_factor{};
  int                           mv0_threshold{};
  int                           b_sensitivity{};
  AVColorPrimaries              color_primaries{};
  AVColorTransferCharacteristic color_trc{};
  AVColorSpace                  colorspace{};
  AVColorRange                  color_range{};
  AVChromaLocation              chroma_sample_location{};

  AVCodecContext *codec{};
  LibraryVersions libraryVersions{};
};

} // namespace FFmpeg
