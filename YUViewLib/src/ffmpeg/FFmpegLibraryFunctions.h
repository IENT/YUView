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
#include <QMutex>
#include <QMutexLocker>
#include <common/Typedef.h>

namespace FFmpeg
{

class FFmpegLibraryFunctions
{
public:
  FFmpegLibraryFunctions() = default;
  ~FFmpegLibraryFunctions();

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
    void (*av_register_all)();
    int(*avformat_open_input)(
        AVFormatContext **ps, const char *url, AVInputFormat *fmt, AVDictionary **options);
                                                                    ;
    void (*avformat_close_input)(AVFormatContext **s)                        ;
    int (*avformat_find_stream_info)(AVFormatContext *ic, AVDictionary **options) ;
    int (*av_read_frame)(AVFormatContext *s, AVPacket *pkt)           ;
    int (*av_seek_frame)(AVFormatContext *s, int stream_index, int64_t timestamp, int flags)
                              ;
    unsigned(*avformat_version)() ;
  };
  static AvFormatFunctions avformat;

  struct AvCodecFunctions
  {
    AVCodec *(*avcodec_find_decoder)(AVCodecID id)                ;
    AVCodecContext *(*avcodec_alloc_context3)(const AVCodec *codec) ;
    int (*avcodec_open2)(AVCodecContext *avctx, const AVCodec *codec, AVDictionary **options) ;
    void (*avcodec_free_context)(AVCodecContext **avctx) ;
    AVPacket *(*av_packet_alloc)()                 ;
    void (*av_packet_free)(AVPacket **pkt)         ;
    void (*av_init_packet)(AVPacket *pkt)          ;
    void (*av_packet_unref)(AVPacket *pkt)          ;
    void (*avcodec_flush_buffers)(AVCodecContext *avctx)  ;
    unsigned(*avcodec_version)()                   ;
    const char *(*avcodec_get_name)(AVCodecID id)   ;
    AVCodecParameters *(*avcodec_parameters_alloc)()        ;
    // The following functions are part of the new API.
    // We will check if it is available. If not, we will use the old decoding API.
    bool                                                             newParametersAPIAvailable{};
    int (*avcodec_send_packet)(AVCodecContext *avctx, const AVPacket *avpkt) ;
    int (*avcodec_receive_frame)(AVCodecContext *avctx, AVFrame *frame)        ;
    int (*avcodec_parameters_to_context)(AVCodecContext *codec, const AVCodecParameters *par)
                          ;
    void (*avcodec_decode_video2)() ;
  };
  static AvCodecFunctions avcodec;

  struct AvUtilFunctions
  {
    AVFrame *(*av_frame_alloc)()           ;
    void (*av_frame_free)(AVFrame **frame) ;
    void (*av_mallocz)(size_t size)     ;
    unsigned(*avutil_version)()            ;
    int (*av_dict_set)(AVDictionary **pm, const char *key, const char *value, int flags)
        ;
    AVDictionaryEntry*(*av_dict_get)(
        AVDictionary *m, const char *key, const AVDictionaryEntry *prev, int flags)
        ;
    AVFrameSideData *(*av_frame_get_side_data)(const AVFrame *frame, AVFrameSideDataType type)
                                                        ;
    AVDictionary *(*av_frame_get_metadata)(const AVFrame *frame) ;
    void (*av_log_set_callback)(void (*callback)(void *, int, const char *, va_list)) ;
    void (*av_log_set_level)(int level)                                            ;
    AVPixFmtDescriptor *(*av_pix_fmt_desc_get)(AVPixelFormat pix_fmt)                ;
    AVPixFmtDescriptor *(*av_pix_fmt_desc_next)(const AVPixFmtDescriptor *prev)       ;
    AVPixelFormat (*av_pix_fmt_desc_get_id)(const AVPixFmtDescriptor *desc) ;
  };
  static AvUtilFunctions avutil;

  struct SwResampleFunction
  {
    unsigned(*swresample_version)() ;
  };
  static SwResampleFunction swresample;

  void setLogList(QStringList *l) { logList = l; }

private:
  void addLibNamesToList(QString libName, QStringList &l, const QLibrary &lib) const;
  void unloadAllLibrariesLocked();

  QStringList *logList{};
  void         log(QString message);

};

} // namespace FFmpeg
