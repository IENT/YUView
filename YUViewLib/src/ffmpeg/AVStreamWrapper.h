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

#include "AVCodecContextWrapper.h"
#include "AVCodecIDWrapper.h"
#include "AVCodecParametersWrapper.h"
#include "FFMpegLibrariesTypes.h"
#include <common/Typedef.h>

namespace FFmpeg
{

class AVStreamWrapper
{
public:
  AVStreamWrapper() {}
  AVStreamWrapper(AVStream *src_str, LibraryVersion v);

  explicit        operator bool() const { return this->stream != nullptr; };
  QStringPairList getInfoText(AVCodecIDWrapper &codecIdWrapper);

  AVMediaType              getCodecType();
  QString                  getCodecTypeName();
  AVCodecID                getCodecID();
  AVCodecContextWrapper &  getCodec();
  AVRational               getAvgFrameRate();
  AVRational               getTimeBase();
  Size                     getFrameSize();
  AVColorSpace             getColorspace();
  AVPixelFormat            getPixelFormat();
  QByteArray               getExtradata();
  int                      getIndex();
  AVCodecParametersWrapper getCodecpar();

private:
  void update();

  // These are private. Use "update" to update them from the AVFormatContext
  int                   index{};
  int                   id{};
  AVCodecContextWrapper codec{};
  AVRational            time_base{};
  int64_t               start_time{};
  int64_t               duration{};
  int64_t               nb_frames{};
  int                   disposition{};
  enum AVDiscard        discard
  {
  };
  AVRational sample_aspect_ratio{};
  AVRational avg_frame_rate{};
  int        nb_side_data{};
  int        event_flags{};

  // The AVCodecParameters are present from avformat major version 57 and up.
  AVCodecParametersWrapper codecpar{};

  AVStream *     stream{};
  LibraryVersion libVer{};
};

} // namespace FFmpeg