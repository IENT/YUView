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

#include "AVCodecWrapper.h"
#include <stdexcept>

namespace FFmpeg
{

namespace
{

typedef struct AVCodec_56_57_58
{
  const char *               name;
  const char *               long_name;
  enum AVMediaType           type;
  enum AVCodecID             id;
  int                        capabilities;
  const AVRational *         supported_framerates;
  const enum AVPixelFormat * pix_fmts;
  const int *                supported_samplerates;
  const enum AVSampleFormat *sample_fmts;
  const uint64_t *           channel_layouts;
  uint8_t                    max_lowres;
  const AVClass *            priv_class;

  // Actually, there is more here, but nothing more of the public API
} AVCodec_56_57_58;

typedef struct AVCodec_59
{
  const char *               name;
  const char *               long_name;
  enum AVMediaType           type;
  enum AVCodecID             id;
  int                        capabilities;
  uint8_t                    max_lowres;
  const AVRational *         supported_framerates;
  const enum AVPixelFormat * pix_fmts;
  const int *                supported_samplerates;
  const enum AVSampleFormat *sample_fmts;
  const uint64_t *           channel_layouts;
  const AVClass *            priv_class;

  // Actually, there is more here, but nothing more of the public API
} AVCodec_59;

template <typename T> std::vector<T> convertRawListToVec(const T *rawValues, T terminationValue)
{
  std::vector<T> values;
  int            i   = 0;
  auto           val = rawValues[i++];
  while (val != terminationValue)
  {
    values.push_back(val);
    val = rawValues[i++];
  }
  return values;
}

} // namespace

void AVCodecWrapper::update()
{
  if (codec == nullptr)
    return;

  if (libVer.avcodec.major == 56 || libVer.avcodec.major == 57 || libVer.avcodec.major == 58)
  {
    auto p                      = reinterpret_cast<AVCodec_56_57_58 *>(codec);
    this->name                  = QString(p->name);
    this->long_name             = QString(p->long_name);
    this->type                  = p->type;
    this->id                    = p->id;
    this->capabilities          = p->capabilities;
    this->supported_framerates  = convertRawListToVec(p->supported_framerates, AVRational({0, 0}));
    this->pix_fmts              = convertRawListToVec(p->pix_fmts, AVPixelFormat(-1));
    this->supported_samplerates = convertRawListToVec(p->supported_samplerates, 0);
    this->sample_fmts           = convertRawListToVec(p->sample_fmts, AVSampleFormat(-1));
    this->channel_layouts       = convertRawListToVec(p->channel_layouts, uint64_t(0));
    this->max_lowres            = p->max_lowres;
  }
  else if (libVer.avcodec.major == 59)
  {
    auto p                      = reinterpret_cast<AVCodec_59 *>(codec);
    this->name                  = QString(p->name);
    this->long_name             = QString(p->long_name);
    this->type                  = p->type;
    this->id                    = p->id;
    this->capabilities          = p->capabilities;
    this->supported_framerates  = convertRawListToVec(p->supported_framerates, AVRational({0, 0}));
    this->pix_fmts              = convertRawListToVec(p->pix_fmts, AVPixelFormat(-1));
    this->supported_samplerates = convertRawListToVec(p->supported_samplerates, 0);
    this->sample_fmts           = convertRawListToVec(p->sample_fmts, AVSampleFormat(-1));
    this->channel_layouts       = convertRawListToVec(p->channel_layouts, uint64_t(0));
    this->max_lowres            = p->max_lowres;
  }
  else
    throw std::runtime_error("Invalid library version");
}

} // namespace FFmpeg