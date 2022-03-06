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

class AVFrameWrapper
{
public:
  AVFrameWrapper() = default;
  AVFrameWrapper(LibraryVersion libVersion, AVFrame *frame);
  ~AVFrameWrapper() = default;

  void clear();

  uint8_t *     getData(int component);
  int           getLineSize(int component);
  AVFrame *     getFrame() const;
  int           getWidth();
  int           getHeight();
  Size          getSize();
  int           getPTS();
  AVPictureType getPictType();
  int           getKeyFrame();

  explicit operator bool() const { return this->frame != nullptr; }

private:
  void update();

  // These are private. Use "update" to update them from the AVFormatContext
  uint8_t *     data[AV_NUM_DATA_POINTERS]{};
  int           linesize[AV_NUM_DATA_POINTERS]{};
  int           width{};
  int           height{};
  int           nb_samples{};
  int           format{};
  int           key_frame{};
  AVPictureType pict_type{};
  AVRational    sample_aspect_ratio{};
  int64_t       pts{};
  int64_t       pkt_pts{};
  int64_t       pkt_dts{};
  int           coded_picture_number{};
  int           display_picture_number{};
  int           quality{};

  AVFrame *      frame{};
  LibraryVersion libVer{};
};

} // namespace FFmpeg
