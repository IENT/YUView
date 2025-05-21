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
class AVCodecParametersWrapper
{
public:
  AVCodecParametersWrapper() = default;
  AVCodecParametersWrapper(AVCodecParameters *p, LibraryVersion v);
  explicit        operator bool() const { return this->param != nullptr; }
  QStringPairList getInfoText();

  AVMediaType   getCodecType();
  AVCodecID     getCodecID();
  QByteArray    getExtradata();
  Size          getSize();
  AVColorSpace  getColorspace();
  AVPixelFormat getPixelFormat();
  Ratio         getSampleAspectRatio();

  // Set a default set of (unknown) values
  void setClearValues();

  void setAVMediaType(AVMediaType type);
  void setAVCodecID(AVCodecID id);
  void setExtradata(QByteArray extradata);
  void setSize(Size size);
  void setAVPixelFormat(AVPixelFormat f);
  void setProfileLevel(int profile, int level);
  void setSampleAspectRatio(int num, int den);

  AVCodecParameters *getCodecParameters() const { return this->param; }

private:
  // Update all private values from the AVCodecParameters
  void update();

  // These are private. Use "update" to update them from the AVCodecParameters
  AVMediaType                   codec_type{};
  AVCodecID                     codec_id{};
  uint32_t                      codec_tag{};
  QByteArray                    extradata{};
  int                           format{};
  int64_t                       bit_rate{};
  int                           bits_per_coded_sample{};
  int                           bits_per_raw_sample{};
  int                           profile{};
  int                           level{};
  int                           width{};
  int                           height{};
  AVRational                    sample_aspect_ratio{};
  AVFieldOrder                  field_order{};
  AVColorRange                  color_range{};
  AVColorPrimaries              color_primaries{};
  AVColorTransferCharacteristic color_trc{};
  AVColorSpace                  color_space{};
  AVChromaLocation              chroma_location{};
  int                           video_delay{};

  AVCodecParameters *param{};
  LibraryVersion     libVer{};
};

} // namespace FFmpeg
