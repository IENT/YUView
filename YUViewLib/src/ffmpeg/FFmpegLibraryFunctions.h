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
#include <QLibrary>
#include <common/Typedef.h>

namespace FFmpeg
{

class FFmpegLibraryFunctions
{
public:
  // Load the FFmpeg libraries from the given path.
  bool loadFFmpegLibraryInPath(QString path, LibraryVersion &libraryVersion);
  // Try to load the 4 given specific libraries
  bool loadFFMpegLibrarySpecific(QString avFormatLib,
                                 QString avCodecLib,
                                 QString avUtilLib,
                                 QString swResampleLib);

  QStringList getLibPaths() const;

  struct AvFormatFunctions
  {
    std::function<void()> av_register_all;
    std::function<int(
        AVFormatContext **ps, const char *url, AVInputFormat *fmt, AVDictionary **options)>
                                                                    avformat_open_input;
    std::function<void(AVFormatContext **s)>                        avformat_close_input;
    std::function<int(AVFormatContext *ic, AVDictionary **options)> avformat_find_stream_info;
    std::function<int(AVFormatContext *s, AVPacket *pkt)>           av_read_frame;
    std::function<int(AVFormatContext *s, int stream_index, int64_t timestamp, int flags)>
                              av_seek_frame;
    std::function<unsigned()> avformat_version;
  };
  AvFormatFunctions avformat{};

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
    std::function<void()> avcodec_decode_video2;
    std::function<int(
        AVCodecContext *avctx, AVFrame *picture, int *got_picture_ptr, const AVPacket *avpkt)>
        av_register_all;
  };
  AvCodecFunctions avcodec{};

  struct AvUtilFunctions
  {
    std::function<AVFrame *()>           av_frame_alloc;
    std::function<void(AVFrame **frame)> av_frame_free;
    std::function<void(size_t size)>     av_mallocz;
    std::function<unsigned()>            avutil_version;
    std::function<int(AVDictionary **pm, const char *key, const char *value, int flags)>
        av_dict_set;
    std::function<AVDictionaryEntry*(
        AVDictionary *m, const char *key, const AVDictionaryEntry *prev, int flags)>
        av_dict_get;
    std::function<AVFrameSideData *(const AVFrame *frame, AVFrameSideDataType type)>
                                                        av_frame_get_side_data;
    std::function<AVDictionary *(const AVFrame *frame)> av_frame_get_metadata;
    std::function<void(void (*callback)(void *, int, const char *, va_list))> av_log_set_callback;
    std::function<void(int level)>                                            av_log_set_level;
    std::function<AVPixFmtDescriptor *(AVPixelFormat pix_fmt)>                av_pix_fmt_desc_get;
    std::function<AVPixFmtDescriptor *(const AVPixFmtDescriptor *prev)>       av_pix_fmt_desc_next;
    std::function<AVPixelFormat(const AVPixFmtDescriptor *desc)> av_pix_fmt_desc_get_id;
  };
  AvUtilFunctions avutil{};

  struct SwResampleFunction
  {
    std::function<unsigned()> swresample_version;
  };
  SwResampleFunction swresample{};

  void setLogList(QStringList *l) { logList = l; }

private:
  // bind all functions from the loaded QLibraries.
  bool bindFunctionsFromLibraries();

  void addLibNamesToList(QString libName, QStringList &l, const QLibrary &lib) const;

  QStringList *logList{};
  void         log(QString message);

  QLibrary libAvutil;
  QLibrary libSwresample;
  QLibrary libAvcodec;
  QLibrary libAvformat;
};

} // namespace FFmpeg
