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
#include <QList>

namespace FFmpeg
{

class AVCodecWrapper
{
public:
  AVCodecWrapper() {}
  AVCodecWrapper(AVCodec *codec, LibraryVersion libVer) : codec(codec), libVer(libVer) {}
  explicit  operator bool() const { return this->codec != nullptr; }
  AVCodec * getAVCodec() { return this->codec; }
  AVCodecID getCodecID()
  {
    update();
    return this->id;
  }
  QString getName()
  {
    update();
    return this->name;
  }
  QString getLongName()
  {
    update();
    return this->long_name;
  }

private:
  void update();

  QString               name{};
  QString               long_name{};
  AVMediaType           type;
  AVCodecID             id{AV_CODEC_ID_NONE};
  int                   capabilities{0};       ///< see AV_CODEC_CAP_
  QList<AVRational>     supported_framerates;  ///< terminated by {0,0}
  QList<AVPixelFormat>  pix_fmts;              ///< array is terminated by -1
  QList<int>            supported_samplerates; ///< array is terminated by 0
  QList<AVSampleFormat> sample_fmts;           ///< array is terminated by -1
  QList<uint64_t>       channel_layouts;       ///< array is terminated by 0
  uint8_t               max_lowres{0}; ///< maximum value for lowres supported by the decoder

  AVCodec *            codec{nullptr};
  LibraryVersion libVer;
};

}
