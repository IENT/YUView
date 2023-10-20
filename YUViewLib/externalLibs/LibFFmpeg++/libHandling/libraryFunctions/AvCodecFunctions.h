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

#include <common/Expected.h>
#include <common/FFMpegLibrariesTypes.h>
#include <libHandling/SharedLibraryLoader.h>

namespace LibFFmpeg::functions
{

struct AvCodecFunctions
{
  std::function<AVCodec *(AVCodecID id)>                avcodec_find_decoder;
  std::function<AVCodecContext *(const AVCodec *codec)> avcodec_alloc_context3;
  std::function<int(AVCodecContext *avctx, const AVCodec *codec, AVDictionary **options)>
                                              avcodec_open2;
  std::function<void(AVCodecContext **avctx)> avcodec_free_context;
  std::function<AVPacket *()>                 av_packet_alloc;
  std::function<void(AVPacket **pkt)>         av_packet_free;
  std::function<void(AVPacket *pkt)>          av_init_packet;
  std::function<void(AVPacket *pkt)>          av_packet_unref;
  std::function<void(AVCodecContext *avctx)>  avcodec_flush_buffers;
  std::function<unsigned()>                   avcodec_version;
  std::function<const char *(AVCodecID id)>   avcodec_get_name;
  std::function<AVCodecParameters *()>        avcodec_parameters_alloc;
  // The following functions are part of the new API.
  // We will check if it is available. If not, we will use the old decoding API.
  bool                                                             newParametersAPIAvailable{};
  std::function<int(AVCodecContext *avctx, const AVPacket *avpkt)> avcodec_send_packet;
  std::function<int(AVCodecContext *avctx, AVFrame *frame)>        avcodec_receive_frame;
  std::function<int(AVCodecContext *codec, const AVCodecParameters *par)>
      avcodec_parameters_to_context;
  // This is the old API
  std::function<void()> avcodec_decode_video2;
};

std::optional<AvCodecFunctions> tryBindAVCodecFunctionsFromLibrary(SharedLibraryLoader &lib,
                                                                   Log &                log);

} // namespace LibFFmpeg::functions
