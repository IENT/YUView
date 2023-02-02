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

#include "AVStreamWrapper.h"
#include "AVPacketWrapper.h"

#include <iomanip>
#include <sstream>

namespace FFmpeg
{

namespace
{

// AVStream is part of AVFormat
typedef struct AVStream_56
{
  int               index;
  int               id;
  AVCodecContext *  codec;
  void *            priv_data;
  struct AVFrac     pts;
  AVRational        time_base;
  int64_t           start_time;
  int64_t           duration;
  int64_t           nb_frames;
  int               disposition;
  enum AVDiscard    discard;
  AVRational        sample_aspect_ratio;
  AVDictionary *    metadata;
  AVRational        avg_frame_rate;
  AVPacket_56       attached_pic;
  AVPacketSideData *side_data;
  int               nb_side_data;
  int               event_flags;
} AVStream_56;

typedef struct AVProbeData_57
{
  const char *   filename;
  unsigned char *buf;
  int            buf_size;
  const char *   mime_type;
} AVProbeData_57;

typedef struct AVStream_57
{
  int               index;
  int               id;
  AVCodecContext *  codec; // Deprecated. Might be removed in the next major version.
  void *            priv_data;
  struct AVFrac     pts; // Deprecated. Might be removed in the next major version.
  AVRational        time_base;
  int64_t           start_time;
  int64_t           duration;
  int64_t           nb_frames;
  int               disposition;
  enum AVDiscard    discard;
  AVRational        sample_aspect_ratio;
  AVDictionary *    metadata;
  AVRational        avg_frame_rate;
  AVPacket_57_58    attached_pic;
  AVPacketSideData *side_data;
  int               nb_side_data;
  int               event_flags;
  // All field following this line are not part of the public API and may change/be removed.
  // However, we still need them here because further below some fields which are part of the public
  // API will follow. I really don't understand who thought up this idiotic scheme...
#define MAX_STD_TIMEBASES (30 * 12 + 30 + 3 + 6)
  struct
  {
    int64_t last_dts;
    int64_t duration_gcd;
    int     duration_count;
    int64_t rfps_duration_sum;
    double (*duration_error)[2][MAX_STD_TIMEBASES];
    int64_t codec_info_duration;
    int64_t codec_info_duration_fields;
    int     found_decoder;
    int64_t last_duration;
    int64_t fps_first_dts;
    int     fps_first_dts_idx;
    int64_t fps_last_dts;
    int     fps_last_dts_idx;
  } * info;
  int                          pts_wrap_bits;
  int64_t                      first_dts;
  int64_t                      cur_dts;
  int64_t                      last_IP_pts;
  int                          last_IP_duration;
  int                          probe_packets;
  int                          codec_info_nb_frames;
  enum AVStreamParseType       need_parsing;
  struct AVCodecParserContext *parser;
  struct AVPacketList *        last_in_packet_buffer;
  AVProbeData_57               probe_data;
#define MAX_REORDER_DELAY 16
  int64_t       pts_buffer[MAX_REORDER_DELAY + 1];
  AVIndexEntry *index_entries;
  int           nb_index_entries;
  unsigned int  index_entries_allocated_size;
  AVRational    r_frame_rate;
  int           stream_identifier;
  int64_t       interleaver_chunk_size;
  int64_t       interleaver_chunk_duration;
  int           request_probe;
  int           skip_to_keyframe;
  int           skip_samples;
  int64_t       start_skip_samples;
  int64_t       first_discard_sample;
  int64_t       last_discard_sample;
  int           nb_decoded_frames;
  int64_t       mux_ts_offset;
  int64_t       pts_wrap_reference;
  int           pts_wrap_behavior;
  int           update_initial_durations_done;
  int64_t       pts_reorder_error[MAX_REORDER_DELAY + 1];
  uint8_t       pts_reorder_error_count[MAX_REORDER_DELAY + 1];
  int64_t       last_dts_for_order_check;
  uint8_t       dts_ordered;
  uint8_t       dts_misordered;
  int           inject_global_side_data;
  // All fields above this line are not part of the public API.
  // All fields below are part of the public API and ABI again.
  char *             recommended_encoder_configuration;
  AVRational         display_aspect_ratio;
  struct FFFrac *    priv_pts;
  AVStreamInternal * internal;
  AVCodecParameters *codecpar;
} AVStream_57;

typedef struct AVStream_58
{
  int                index;
  int                id;
  AVCodecContext *   codec;
  void *             priv_data;
  AVRational         time_base;
  int64_t            start_time;
  int64_t            duration;
  int64_t            nb_frames;
  int                disposition;
  enum AVDiscard     discard;
  AVRational         sample_aspect_ratio;
  AVDictionary *     metadata;
  AVRational         avg_frame_rate;
  AVPacket_57_58     attached_pic;
  AVPacketSideData * side_data;
  int                nb_side_data;
  int                event_flags;
  AVRational         r_frame_rate;
  char *             recommended_encoder_configuration;
  AVCodecParameters *codecpar;

  // All field following this line are not part of the public API and may change/be removed.
} AVStream_58;

typedef struct AVStream_59
{
  int                index;
  int                id;
  void *             priv_data;
  AVRational         time_base;
  int64_t            start_time;
  int64_t            duration;
  int64_t            nb_frames;
  int                disposition;
  enum AVDiscard     discard;
  AVRational         sample_aspect_ratio;
  AVDictionary *     metadata;
  AVRational         avg_frame_rate;
  AVPacket_59        attached_pic;
  AVPacketSideData * side_data;
  int                nb_side_data;
  int                event_flags;
  AVRational         r_frame_rate;
  AVCodecParameters *codecpar;
  int                pts_wrap_bits;
} AVStream_59;

std::string formatFramerate(Rational frameRate)
{
  std::stringstream stream;
  auto              divFrameRate = 0.0;
  if (frameRate.den > 0)
    divFrameRate = static_cast<double>(frameRate.num) / static_cast<double>(frameRate.den);
  stream << frameRate.num << "/" << frameRate.den << " (" << std::setprecision(2) << divFrameRate
         << ")";
  return stream.str();
}

} // namespace

AVStreamWrapper::AVStreamWrapper(AVStream *src_str, LibraryVersion v)
{
  this->stream = src_str;
  this->libVer = v;
  this->update();
}

AVMediaType AVStreamWrapper::getCodecType()
{
  this->update();
  if (this->libVer.avformat.major <= 56 || !this->codecpar)
    return this->codec.getCodecType();
  return this->codecpar.getCodecType();
}

std::string AVStreamWrapper::getCodecTypeName()
{
  auto type = this->codecpar.getCodecType();
  if (type > AVMEDIA_TYPE_NB)
    return {};

  std::array names = {"Unknown", "Video", "Audio", "Data", "Subtitle", "Attachment", "NB"};
  return names[type + 1];
}

AVCodecID AVStreamWrapper::getCodecID()
{
  this->update();
  if (this->stream == nullptr)
    return AV_CODEC_ID_NONE;

  if (this->libVer.avformat.major <= 56 || !this->codecpar)
    return this->codec.getCodecID();
  else
    return this->codecpar.getCodecID();
}

AVCodecContextWrapper &AVStreamWrapper::getCodec()
{
  this->update();
  return this->codec;
};

Rational AVStreamWrapper::getAvgFrameRate()
{
  this->update();
  return this->avg_frame_rate;
}

Rational AVStreamWrapper::getTimeBase()
{
  this->update();
  if (this->time_base.den == 0 || this->time_base.num == 0)
    // The stream time_base seems not to be set. Try the time_base in the codec.
    return this->codec.getTimeBase();
  return this->time_base;
}

Size AVStreamWrapper::getFrameSize()
{
  this->update();
  if (this->libVer.avformat.major <= 56 || !this->codecpar)
    return this->codec.getSize();
  return this->codecpar.getSize();
}

AVColorSpace AVStreamWrapper::getColorspace()
{
  this->update();
  if (this->libVer.avformat.major <= 56 || !this->codecpar)
    return this->codec.getColorspace();
  return this->codecpar.getColorspace();
}

AVPixelFormat AVStreamWrapper::getPixelFormat()
{
  this->update();
  if (this->libVer.avformat.major <= 56 || !this->codecpar)
    return this->codec.getPixelFormat();
  return this->codecpar.getPixelFormat();
}

QByteArray AVStreamWrapper::getExtradata()
{
  this->update();
  if (this->libVer.avformat.major <= 56 || !this->codecpar)
    return this->codec.getExtradata();
  return this->codecpar.getExtradata();
}

int AVStreamWrapper::getIndex()
{
  this->update();
  return this->index;
}

AVCodecParametersWrapper AVStreamWrapper::getCodecpar()
{
  this->update();
  return this->codecpar;
}

void AVStreamWrapper::update()
{
  if (this->stream == nullptr)
    return;

  // Copy values from the source pointer
  if (libVer.avformat.major == 56)
  {
    auto p                        = reinterpret_cast<AVStream_56 *>(this->stream);
    this->index                   = p->index;
    this->id                      = p->id;
    this->codec                   = AVCodecContextWrapper(p->codec, libVer);
    this->time_base.num           = p->time_base.num;
    this->time_base.den           = p->time_base.den;
    this->start_time              = p->start_time;
    this->duration                = p->duration;
    this->nb_frames               = p->nb_frames;
    this->disposition             = p->nb_frames;
    this->discard                 = p->discard;
    this->sample_aspect_ratio.num = p->sample_aspect_ratio.num;
    this->sample_aspect_ratio.den = p->sample_aspect_ratio.den;
    this->avg_frame_rate.num      = p->avg_frame_rate.num;
    this->avg_frame_rate.den      = p->avg_frame_rate.den;
    this->nb_side_data            = p->nb_side_data;
    this->event_flags             = p->event_flags;
  }
  else if (libVer.avformat.major == 57)
  {
    auto p                        = reinterpret_cast<AVStream_57 *>(this->stream);
    this->index                   = p->index;
    this->id                      = p->id;
    this->codec                   = AVCodecContextWrapper(p->codec, libVer);
    this->time_base.num           = p->time_base.num;
    this->time_base.den           = p->time_base.den;
    this->start_time              = p->start_time;
    this->duration                = p->duration;
    this->nb_frames               = p->nb_frames;
    this->disposition             = p->nb_frames;
    this->discard                 = p->discard;
    this->sample_aspect_ratio.num = p->sample_aspect_ratio.num;
    this->sample_aspect_ratio.den = p->sample_aspect_ratio.den;
    this->avg_frame_rate.num      = p->avg_frame_rate.num;
    this->avg_frame_rate.den      = p->avg_frame_rate.den;
    this->nb_side_data            = p->nb_side_data;
    this->event_flags             = p->event_flags;
    this->codecpar                = AVCodecParametersWrapper(p->codecpar, libVer);
  }
  else if (libVer.avformat.major == 58)
  {
    auto p                        = reinterpret_cast<AVStream_58 *>(this->stream);
    this->index                   = p->index;
    this->id                      = p->id;
    this->codec                   = AVCodecContextWrapper(p->codec, libVer);
    this->time_base.num           = p->time_base.num;
    this->time_base.den           = p->time_base.den;
    this->start_time              = p->start_time;
    this->duration                = p->duration;
    this->nb_frames               = p->nb_frames;
    this->disposition             = p->nb_frames;
    this->discard                 = p->discard;
    this->sample_aspect_ratio.num = p->sample_aspect_ratio.num;
    this->sample_aspect_ratio.den = p->sample_aspect_ratio.den;
    this->avg_frame_rate.num      = p->avg_frame_rate.num;
    this->avg_frame_rate.den      = p->avg_frame_rate.den;
    this->nb_side_data            = p->nb_side_data;
    this->event_flags             = p->event_flags;
    this->codecpar                = AVCodecParametersWrapper(p->codecpar, libVer);
  }
  else if (libVer.avformat.major == 59)
  {
    auto p                        = reinterpret_cast<AVStream_59 *>(this->stream);
    this->index                   = p->index;
    this->id                      = p->id;
    this->time_base.num           = p->time_base.num;
    this->time_base.den           = p->time_base.den;
    this->start_time              = p->start_time;
    this->duration                = p->duration;
    this->nb_frames               = p->nb_frames;
    this->disposition             = p->nb_frames;
    this->discard                 = p->discard;
    this->sample_aspect_ratio.num = p->sample_aspect_ratio.num;
    this->sample_aspect_ratio.den = p->sample_aspect_ratio.den;
    this->avg_frame_rate.num      = p->avg_frame_rate.num;
    this->avg_frame_rate.den      = p->avg_frame_rate.den;
    this->nb_side_data            = p->nb_side_data;
    this->event_flags             = p->event_flags;
    this->codecpar                = AVCodecParametersWrapper(p->codecpar, libVer);
  }
  else
    throw std::runtime_error("Invalid library version");
}

StringPairVec AVStreamWrapper::getInfoText(const AVCodecIDWrapper &codecIdWrapper) const
{
  if (this->stream == nullptr)
    return {{"Error stream is null", ""}};

  AVStreamWrapper wrapper(this->stream, this->libVer);

  StringPairVec info;
  info.push_back({"Index", std::to_string(wrapper.index)});
  info.push_back({"ID", std::to_string(wrapper.id)});

  info.push_back({"Codec Type", wrapper.getCodecTypeName()});
  info.push_back({"Codec ID", std::to_string(wrapper.getCodecID())});
  info.push_back({"Codec Name", codecIdWrapper.getCodecName()});
  info.push_back(
      {"Time base",
       std::to_string(wrapper.time_base.num) + "/" + std::to_string(wrapper.time_base.den)});
  info.push_back({"Start Time",
                  std::to_string(wrapper.start_time) + " (" +
                      formatWithReadableFormat(wrapper.start_time, wrapper.time_base) + ")"});
  info.push_back({"Duration",
                  std::to_string(wrapper.duration) + " (" +
                      formatWithReadableFormat(wrapper.duration, wrapper.time_base) + ")"});
  info.push_back({"Number Frames", std::to_string(wrapper.nb_frames)});

  if (wrapper.disposition != 0)
  {
    std::string dispText;
    if (wrapper.disposition & 0x0001)
      dispText += "Default ";
    if (wrapper.disposition & 0x0002)
      dispText += "Dub ";
    if (wrapper.disposition & 0x0004)
      dispText += "Original ";
    if (wrapper.disposition & 0x0008)
      dispText += "Comment ";
    if (wrapper.disposition & 0x0010)
      dispText += "Lyrics ";
    if (wrapper.disposition & 0x0020)
      dispText += "Karaoke ";
    if (wrapper.disposition & 0x0040)
      dispText += "Forced ";
    if (wrapper.disposition & 0x0080)
      dispText += "Hearing_Imparied ";
    if (wrapper.disposition & 0x0100)
      dispText += "Visual_Impaired ";
    if (wrapper.disposition & 0x0200)
      dispText += "Clean_Effects ";
    if (wrapper.disposition & 0x0400)
      dispText += "Attached_Pic ";
    if (wrapper.disposition & 0x0800)
      dispText += "Timed_Thumbnails ";
    if (wrapper.disposition & 0x1000)
      dispText += "Captions ";
    if (wrapper.disposition & 0x2000)
      dispText += "Descriptions ";
    if (wrapper.disposition & 0x4000)
      dispText += "Metadata ";
    if (wrapper.disposition & 0x8000)
      dispText += "Dependent ";
    info.push_back({"Disposition", dispText});
  }

  info.push_back({"Sample Aspect Ratio",
                  std::to_string(wrapper.sample_aspect_ratio.num) + ":" +
                      std::to_string(wrapper.sample_aspect_ratio.den)});

  info.push_back({"Average Frame Rate", formatFramerate(wrapper.avg_frame_rate)});

  const auto codecparInfo = wrapper.codecpar.getInfoText();
  info.insert(info.end(), codecparInfo.begin(), codecparInfo.end());
  return info;
}

} // namespace FFmpeg
