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

#include "AVFormatContextWrapper.h"

#include "AVStreamWrapper.h"
#include <common/Functions.h>

namespace FFmpeg
{

namespace
{

// AVFormatContext is part of avformat.
// These functions give us version independent access to the structs.
typedef struct AVFormatContext_56
{
  const AVClass *        av_class;
  struct AVInputFormat * iformat;
  struct AVOutputFormat *oformat;
  void *                 priv_data;
  AVIOContext *          pb;
  int                    ctx_flags;
  unsigned int           nb_streams; //
  AVStream **            streams;    //
  char                   filename[1024];
  int64_t                start_time;
  int64_t                duration; //
  int                    bit_rate;
  unsigned int           packet_size;
  int                    max_delay;
  int                    flags;
  unsigned int           probesize;
  int                    max_analyze_duration;
  const uint8_t *        key;
  int                    keylen;
  unsigned int           nb_programs;
  AVProgram **           programs;
  enum AVCodecID         video_codec_id;
  enum AVCodecID         audio_codec_id;
  enum AVCodecID         subtitle_codec_id;
  unsigned int           max_index_size;
  unsigned int           max_picture_buffer;
  unsigned int           nb_chapters;
  AVChapter **           chapters;
  AVDictionary *         metadata;

  // Actually, there is more here, but the variables above are the only we need.
} AVFormatContext_56;

typedef struct AVFormatContext_57
{
  const AVClass *        av_class;
  struct AVInputFormat * iformat;
  struct AVOutputFormat *oformat;
  void *                 priv_data;
  AVIOContext *          pb;
  int                    ctx_flags;
  unsigned int           nb_streams;
  AVStream **            streams;
  char                   filename[1024];
  int64_t                start_time;
  int64_t                duration;
  int64_t                bit_rate;
  unsigned int           packet_size;
  int                    max_delay;
  int                    flags;
  unsigned int           probesize;
  int                    max_analyze_duration;
  const uint8_t *        key;
  int                    keylen;
  unsigned int           nb_programs;
  AVProgram **           programs;
  enum AVCodecID         video_codec_id;
  enum AVCodecID         audio_codec_id;
  enum AVCodecID         subtitle_codec_id;
  unsigned int           max_index_size;
  unsigned int           max_picture_buffer;
  unsigned int           nb_chapters;
  AVChapter **           chapters;
  AVDictionary *         metadata;

  // Actually, there is more here, but the variables above are the only we need.
} AVFormatContext_57;

typedef struct AVFormatContext_58
{
  const AVClass *        av_class;
  struct AVInputFormat * iformat;
  struct AVOutputFormat *oformat;
  void *                 priv_data;
  AVIOContext *          pb;
  int                    ctx_flags;
  unsigned int           nb_streams;
  AVStream **            streams;
  char                   filename[1024];
  char *                 url;
  int64_t                start_time;
  int64_t                duration;
  int64_t                bit_rate;
  unsigned int           packet_size;
  int                    max_delay;
  int                    flags;
  int64_t                probesize;
  int64_t                max_analyze_duration;
  const uint8_t *        key;
  int                    keylen;
  unsigned int           nb_programs;
  AVProgram **           programs;
  enum AVCodecID         video_codec_id;
  enum AVCodecID         audio_codec_id;
  enum AVCodecID         subtitle_codec_id;
  unsigned int           max_index_size;
  unsigned int           max_picture_buffer;
  unsigned int           nb_chapters;
  AVChapter **           chapters;
  AVDictionary *         metadata;

  // Actually, there is more here, but the variables above are the only we need.
} AVFormatContext_58;

typedef struct AVFormatContext_59
{
  const AVClass *        av_class;
  struct AVInputFormat * iformat;
  struct AVOutputFormat *oformat;
  void *                 priv_data;
  AVIOContext *          pb;
  int                    ctx_flags;
  unsigned int           nb_streams;
  AVStream **            streams;
  char *                 url;
  int64_t                start_time;
  int64_t                duration;
  int64_t                bit_rate;
  unsigned int           packet_size;
  int                    max_delay;
  int                    flags;
  int64_t                probesize;
  int64_t                max_analyze_duration;
  const uint8_t *        key;
  int                    keylen;
  unsigned int           nb_programs;
  AVProgram **           programs;
  enum AVCodecID         video_codec_id;
  enum AVCodecID         audio_codec_id;
  enum AVCodecID         subtitle_codec_id;
  unsigned int           max_index_size;
  unsigned int           max_picture_buffer;
  unsigned int           nb_chapters;
  AVChapter **           chapters;
  AVDictionary *         metadata;
} AVFormatContext_59;

} // namespace

AVFormatContextWrapper::AVFormatContextWrapper(AVFormatContext *c, LibraryVersion v)
{
  this->ctx    = c;
  this->libVer = v;
  this->update();
}

void AVFormatContextWrapper::updateFrom(AVFormatContext *c)
{
  assert(this->ctx == nullptr);
  this->ctx = c;
  this->update();
}

AVFormatContextWrapper::operator bool() const
{
  return this->ctx;
};

unsigned AVFormatContextWrapper::getNbStreams() const
{
  switch (this->libVer.avformat.major)
  {
  case 56:
    auto p = reinterpret_cast<AVFormatContext_56 *>(this->ctx);
    return p->nb_streams;
  case 57:
    auto p = reinterpret_cast<AVFormatContext_57 *>(this->ctx);
    return p->nb_streams;
  case 58:
    auto p = reinterpret_cast<AVFormatContext_58 *>(this->ctx);
    return p->nb_streams;
  case 59:
    auto p = reinterpret_cast<AVFormatContext_59 *>(this->ctx);
    return p->nb_streams;

  default:
    throw std::runtime_error("Invalid library version");
  }
}

AVStreamWrapper AVFormatContextWrapper::getStream(int idx) const
{
  if (idx >= this->getNbStreams())
    throw std::runtime_error("Invalid stream index");

  switch (this->libVer.avformat.major)
  {
  case 56:
    auto p = reinterpret_cast<AVFormatContext_56 *>(this->ctx);
    return AVStreamWrapper(p->streams[idx], this->libVer);
  case 57:
    auto p = reinterpret_cast<AVFormatContext_57 *>(this->ctx);
    return AVStreamWrapper(p->streams[idx], this->libVer);
  case 58:
    auto p = reinterpret_cast<AVFormatContext_58 *>(this->ctx);
    return AVStreamWrapper(p->streams[idx], this->libVer);
  case 59:
    auto p = reinterpret_cast<AVFormatContext_59 *>(this->ctx);
    return AVStreamWrapper(p->streams[idx], this->libVer);

  default:
    throw std::runtime_error("Invalid library version");
  }
}

AVInputFormatWrapper AVFormatContextWrapper::getInputFormat() const
{
  switch (this->libVer.avformat.major)
  {
  case 56:
    auto p = reinterpret_cast<AVFormatContext_56 *>(this->ctx);
    return AVInputFormatWrapper(p->iformat, libVer);
  case 57:
    auto p = reinterpret_cast<AVFormatContext_57 *>(this->ctx);
    return AVInputFormatWrapper(p->iformat, libVer);
  case 58:
    auto p = reinterpret_cast<AVFormatContext_58 *>(this->ctx);
    return AVInputFormatWrapper(p->iformat, libVer);
  case 59:
    auto p = reinterpret_cast<AVFormatContext_59 *>(this->ctx);
    return AVInputFormatWrapper(p->iformat, libVer);

  default:
    throw std::runtime_error("Invalid library version");
  }
}

int64_t AVFormatContextWrapper::getStartTime() const
{
  switch (this->libVer.avformat.major)
  {
  case 56:
    auto p = reinterpret_cast<AVFormatContext_56 *>(this->ctx);
    return p->start_time;
  case 57:
    auto p = reinterpret_cast<AVFormatContext_57 *>(this->ctx);
    return p->start_time;
  case 58:
    auto p = reinterpret_cast<AVFormatContext_58 *>(this->ctx);
    return p->start_time;
  case 59:
    auto p = reinterpret_cast<AVFormatContext_59 *>(this->ctx);
    return p->start_time;

  default:
    throw std::runtime_error("Invalid library version");
  }
}

int64_t AVFormatContextWrapper::getDuration() const
{
  switch (this->libVer.avformat.major)
  {
  case 56:
    auto p = reinterpret_cast<AVFormatContext_56 *>(this->ctx);
    return p->duration;
  case 57:
    auto p = reinterpret_cast<AVFormatContext_57 *>(this->ctx);
    return p->duration;
  case 58:
    auto p = reinterpret_cast<AVFormatContext_58 *>(this->ctx);
    return p->duration;
  case 59:
    auto p = reinterpret_cast<AVFormatContext_59 *>(this->ctx);
    return p->duration;

  default:
    throw std::runtime_error("Invalid library version");
  }
}

AVFormatContext *AVFormatContextWrapper::getFormatCtx() const
{
  return this->ctx;
}

AVDictionaryWrapper AVFormatContextWrapper::getMetadata() const
{
  switch (this->libVer.avformat.major)
  {
  case 56:
    auto p = reinterpret_cast<AVFormatContext_56 *>(this->ctx);
    return AVDictionaryWrapper(p->metadata);
  case 57:
    auto p = reinterpret_cast<AVFormatContext_57 *>(this->ctx);
    return AVDictionaryWrapper(p->metadata);
  case 58:
    auto p = reinterpret_cast<AVFormatContext_58 *>(this->ctx);
    return AVDictionaryWrapper(p->metadata);
  case 59:
    auto p = reinterpret_cast<AVFormatContext_59 *>(this->ctx);
    return AVDictionaryWrapper(p->metadata);

  default:
    throw std::runtime_error("Invalid library version");
  }
}

void AVFormatContextWrapper::update()
{
  if (this->ctx == nullptr)
    return;

  this->streams.clear();

  // Copy values from the source pointer
  if (this->libVer.avformat.major == 56)
  {
    auto p           = reinterpret_cast<AVFormatContext_56 *>(this->ctx);
    this->ctx_flags  = p->ctx_flags;
    this->nb_streams = p->nb_streams;
    for (unsigned i = 0; i < this->nb_streams; i++)
      this->streams.push_back(AVStreamWrapper(p->streams[i], this->libVer));
    this->filename             = std::string(p->filename);
    this->start_time           = p->start_time;
    this->duration             = p->duration;
    this->bit_rate             = p->bit_rate;
    this->packet_size          = p->packet_size;
    this->max_delay            = p->max_delay;
    this->flags                = p->flags;
    this->probesize            = p->probesize;
    this->max_analyze_duration = p->max_analyze_duration;
    this->key                  = std::string(reinterpret_cast<const char *>(p->key), p->keylen);
    this->nb_programs          = p->nb_programs;
    this->video_codec_id       = p->video_codec_id;
    this->audio_codec_id       = p->audio_codec_id;
    this->subtitle_codec_id    = p->subtitle_codec_id;
    this->max_index_size       = p->max_index_size;
    this->max_picture_buffer   = p->max_picture_buffer;
    this->nb_chapters          = p->nb_chapters;
    this->metadata             = AVDictionaryWrapper(p->metadata);

    this->iformat = AVInputFormatWrapper(p->iformat, libVer);
  }
  else if (this->libVer.avformat.major == 57)
  {
    auto p           = reinterpret_cast<AVFormatContext_57 *>(this->ctx);
    this->ctx_flags  = p->ctx_flags;
    this->nb_streams = p->nb_streams;
    for (unsigned i = 0; i < nb_streams; i++)
      this->streams.push_back(AVStreamWrapper(p->streams[i], this->libVer));
    this->filename             = std::string(p->filename);
    this->start_time           = p->start_time;
    this->duration             = p->duration;
    this->bit_rate             = p->bit_rate;
    this->packet_size          = p->packet_size;
    this->max_delay            = p->max_delay;
    this->flags                = p->flags;
    this->probesize            = p->probesize;
    this->max_analyze_duration = p->max_analyze_duration;
    this->key                  = std::string(reinterpret_cast<const char *>(p->key), p->keylen);
    this->nb_programs          = p->nb_programs;
    this->video_codec_id       = p->video_codec_id;
    this->audio_codec_id       = p->audio_codec_id;
    this->subtitle_codec_id    = p->subtitle_codec_id;
    this->max_index_size       = p->max_index_size;
    this->max_picture_buffer   = p->max_picture_buffer;
    this->nb_chapters          = p->nb_chapters;
    this->metadata             = AVDictionaryWrapper(p->metadata);

    this->iformat = AVInputFormatWrapper(p->iformat, libVer);
  }
  else if (this->libVer.avformat.major == 58)
  {
    auto p           = reinterpret_cast<AVFormatContext_58 *>(this->ctx);
    this->ctx_flags  = p->ctx_flags;
    this->nb_streams = p->nb_streams;
    for (unsigned i = 0; i < nb_streams; i++)
      this->streams.push_back(AVStreamWrapper(p->streams[i], this->libVer));
    this->filename             = std::string(p->filename);
    this->start_time           = p->start_time;
    this->duration             = p->duration;
    this->bit_rate             = p->bit_rate;
    this->packet_size          = p->packet_size;
    this->max_delay            = p->max_delay;
    this->flags                = p->flags;
    this->probesize            = p->probesize;
    this->max_analyze_duration = p->max_analyze_duration;
    this->key                  = std::string(reinterpret_cast<const char *>(p->key), p->keylen);
    this->nb_programs          = p->nb_programs;
    this->video_codec_id       = p->video_codec_id;
    this->audio_codec_id       = p->audio_codec_id;
    this->subtitle_codec_id    = p->subtitle_codec_id;
    this->max_index_size       = p->max_index_size;
    this->max_picture_buffer   = p->max_picture_buffer;
    this->nb_chapters          = p->nb_chapters;
    this->metadata             = AVDictionaryWrapper(p->metadata);

    this->iformat = AVInputFormatWrapper(p->iformat, this->libVer);
  }
  else if (this->libVer.avformat.major == 59)
  {
    auto p           = reinterpret_cast<AVFormatContext_59 *>(this->ctx);
    this->ctx_flags  = p->ctx_flags;
    this->nb_streams = p->nb_streams;
    for (unsigned i = 0; i < nb_streams; i++)
      this->streams.push_back(AVStreamWrapper(p->streams[i], this->libVer));
    this->filename             = std::string(p->url);
    this->start_time           = p->start_time;
    this->duration             = p->duration;
    this->bit_rate             = p->bit_rate;
    this->packet_size          = p->packet_size;
    this->max_delay            = p->max_delay;
    this->flags                = p->flags;
    this->probesize            = p->probesize;
    this->max_analyze_duration = p->max_analyze_duration;
    this->key                  = std::string(reinterpret_cast<const char *>(p->key), p->keylen);
    this->nb_programs          = p->nb_programs;
    this->video_codec_id       = p->video_codec_id;
    this->audio_codec_id       = p->audio_codec_id;
    this->subtitle_codec_id    = p->subtitle_codec_id;
    this->max_index_size       = p->max_index_size;
    this->max_picture_buffer   = p->max_picture_buffer;
    this->nb_chapters          = p->nb_chapters;
    this->metadata             = AVDictionaryWrapper(p->metadata);

    this->iformat = AVInputFormatWrapper(p->iformat, this->libVer);
  }
  else
    throw std::runtime_error("Invalid library version");
}

StringPairVec AVFormatContextWrapper::getInfoText() const
{
  if (this->ctx == nullptr)
    return {};

  AVFormatContextWrapper formatContext(this->ctx, this->libVer);
  StringPairVec          info;

  if (formatContext.ctx_flags != 0)
  {
    std::vector<std::string> flags;
    if (formatContext.ctx_flags & 1)
      flags.push_back("No-Header");
    if (formatContext.ctx_flags & 2)
      flags.push_back("Un-seekable");
    info.push_back({"Flags", functions::to_string(flags)});
  }

  AVRational time_base;
  time_base.num = 1;
  time_base.den = AV_TIME_BASE;

  info.push_back({"Number streams", std::to_string(formatContext.nb_streams)});
  info.push_back({"File name", this->filename});
  info.push_back({"Start time", formatWithReadableFormat(formatContext.start_time, time_base)});
  info.push_back({"Duration", formatWithReadableFormat(formatContext.duration, time_base)});
  if (bit_rate > 0)
    info.push_back({"Bitrate", std::to_string(formatContext.bit_rate)});
  info.push_back({"Packet size", std::to_string(formatContext.packet_size)});
  info.push_back({"Max delay", std::to_string(formatContext.max_delay)});
  info.push_back({"Number programs", std::to_string(formatContext.nb_programs)});
  info.push_back({"Number chapters", std::to_string(formatContext.nb_chapters)});

  return info;
}

} // namespace FFmpeg
