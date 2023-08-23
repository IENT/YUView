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

namespace FFmpeg
{

// AVPacket data can be in one of two formats:
// 1: The raw annexB format with start codes (0x00000001 or 0x000001)
// 2: ISO/IEC 14496-15 mp4 format: The first 4 bytes determine the size of the NAL unit followed by
// the payload
enum class PacketDataFormat
{
  Unknown,
  RawNAL,
  MP4,
  OBU
};

enum class PacketType
{
  VIDEO,
  AUDIO,
  SUBTITLE_DVB,
  SUBTITLE_608,
  OTHER
};

// AVPacket is part of avcodec. The definition is different for different major versions of avcodec.
// These are the version independent functions to retrive data from AVPacket.
// The size of this struct is part of the public API and must be correct
// since its size is used in other structures (e.g. AVStream).
typedef struct AVPacket_56
{
  AVBufferRef *     buf;
  int64_t           pts;
  int64_t           dts;
  uint8_t *         data;
  int               size;
  int               stream_index;
  int               flags;
  AVPacketSideData *side_data;
  int               side_data_elems;
  int               duration;
  void (*destruct)(struct AVPacket *);
  void *  priv;
  int64_t pos;
} AVPacket_56;

typedef struct AVPacket_57_58
{
  AVBufferRef *     buf;
  int64_t           pts;
  int64_t           dts;
  uint8_t *         data;
  int               size;
  int               stream_index;
  int               flags;
  AVPacketSideData *side_data;
  int               side_data_elems;
  int64_t           duration;
  int64_t           pos;
  int64_t           convergence_duration;
} AVPacket_57_58;

typedef struct AVPacket_59
{
  AVBufferRef *     buf;
  int64_t           pts;
  int64_t           dts;
  uint8_t *         data;
  int               size;
  int               stream_index;
  int               flags;
  AVPacketSideData *side_data;
  int               side_data_elems;
  int64_t           duration;
  int64_t           pos;
  void *            opaque;
  AVBufferRef *     opaque_ref;
  AVRational        time_base;
} AVPacket_59;

typedef struct AVPacket_60
{
  AVBufferRef *     buf;
  int64_t           pts;
  int64_t           dts;
  uint8_t *         data;
  int               size;
  int               stream_index;
  int               flags;
  AVPacketSideData *side_data;
  int               side_data_elems;
  int64_t           duration;
  int64_t           pos;
  void *            opaque;
  AVBufferRef *     opaque_ref;
  AVRational        time_base;
} AVPacket_60;

// A wrapper around the different versions of the AVPacket versions.
// It also adds some functions like automatic deletion when it goes out of scope.
class AVPacketWrapper
{
public:
  AVPacketWrapper() = default;
  AVPacketWrapper(LibraryVersion libVersion, AVPacket *packet);

  void clear();

  void setData(QByteArray &set_data);
  void setPTS(int64_t pts);
  void setDTS(int64_t dts);

  AVPacket *getPacket();
  int       getStreamIndex();
  int64_t   getPTS();
  int64_t   getDTS();
  int64_t   getDuration();
  int       getFlags();
  bool      getFlagKeyframe();
  bool      getFlagCorrupt();
  bool      getFlagDiscard();
  uint8_t * getData();
  int       getDataSize();

  // This info is set externally (in FileSourceFFmpegFile) based on the stream info
  PacketType getPacketType() const;
  void       setPacketType(PacketType packetType);

  // Guess the format. The actual guessing is only performed if the packetFormat is not set yet.
  PacketDataFormat guessDataFormatFromData();

  explicit operator bool() const { return this->pkt != nullptr; };

private:
  void update();

  // These are private. Use "update" to update them from the AVFormatContext
  AVBufferRef *     buf{};
  int64_t           pts{};
  int64_t           dts{};
  uint8_t *         data{};
  int               size{};
  int               stream_index{};
  int               flags{};
  AVPacketSideData *side_data{};
  int               side_data_elems{};
  int64_t           duration{};
  int64_t           pos{};

  PacketType packetType{};

  AVPacket *       pkt{};
  LibraryVersion   libVer{};
  PacketDataFormat packetFormat{PacketDataFormat::Unknown};
};

} // namespace FFmpeg