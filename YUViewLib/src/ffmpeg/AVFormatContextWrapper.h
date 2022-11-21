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

#include "AVDictionaryWrapper.h"
#include "AVInputFormatWrapper.h"
#include "AVPacketWrapper.h"
#include "AVStreamWrapper.h"
#include "FFMpegLibrariesTypes.h"
#include <common/Typedef.h>

namespace FFmpeg
{

class AVFormatContextWrapper
{
public:
  AVFormatContextWrapper() = default;
  AVFormatContextWrapper(AVFormatContext *c, LibraryVersion v);

  void                        updateFrom(AVFormatContext *c);
  explicit                    operator bool() const;
  [[nodiscard]] StringPairVec getInfoText() const;

  [[nodiscard]] unsigned int         getNbStreams() const;
  [[nodiscard]] AVStreamWrapper      getStream(int idx) const;
  [[nodiscard]] AVInputFormatWrapper getInputFormat() const;
  [[nodiscard]] int64_t              getStartTime() const;
  [[nodiscard]] int64_t              getDuration() const;
  [[nodiscard]] AVFormatContext *    getFormatCtx() const;
  [[nodiscard]] AVDictionaryWrapper  getMetadata() const;

private:
  // Update all private values from the AVFormatContext
  void update();

  AVInputFormatWrapper iformat{};

  // These are private. Use "update" to update them from the AVFormatContext
  int                          ctx_flags{0};
  unsigned int                 nb_streams{0};
  std::vector<AVStreamWrapper> streams;
  std::string                  filename{};
  int64_t                      start_time{-1};
  int64_t                      duration{-1};
  int                          bit_rate{0};
  unsigned int                 packet_size{0};
  int                          max_delay{0};
  int                          flags{0};

  unsigned int        probesize{0};
  int                 max_analyze_duration{0};
  std::string         key{};
  unsigned int        nb_programs{0};
  AVCodecID           video_codec_id{AV_CODEC_ID_NONE};
  AVCodecID           audio_codec_id{AV_CODEC_ID_NONE};
  AVCodecID           subtitle_codec_id{AV_CODEC_ID_NONE};
  unsigned int        max_index_size{0};
  unsigned int        max_picture_buffer{0};
  unsigned int        nb_chapters{0};
  AVDictionaryWrapper metadata;

  AVFormatContext *ctx{nullptr};
  LibraryVersion   libVer{};
};

} // namespace FFmpeg
