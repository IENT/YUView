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
  const char *      name;
  const char *      long_name;
  enum AVMediaType  type;
  enum AVCodecID    id;
  int               capabilities;
  const AVRational *supported_framerates; ///< array of supported framerates, or NULL if any, array
                                          ///< is terminated by {0,0}
  const enum AVPixelFormat *
      pix_fmts; ///< array of supported pixel formats, or NULL if unknown, array is terminated by -1
  const int *supported_samplerates; ///< array of supported audio samplerates, or NULL if unknown,
                                    ///< array is terminated by 0
  const enum AVSampleFormat *sample_fmts; ///< array of supported sample formats, or NULL if
                                          ///< unknown, array is terminated by -1
  const uint64_t *channel_layouts; ///< array of support channel layouts, or NULL if unknown. array
                                   ///< is terminated by 0
  uint8_t        max_lowres;       ///< maximum value for lowres supported by the decoder
  const AVClass *priv_class;       ///< AVClass for the private context
  // const AVProfile *profiles;            ///< array of recognized profiles, or NULL if unknown,
  // array is terminated by {FF_PROFILE_UNKNOWN}

  // Actually, there is more here, but nothing more of the public API
} AVCodec_56_57_58;

} // namespace

void AVCodecWrapper::update()
{
  if (codec == nullptr)
    return;

  if (libVer.avcodec.major == 56 || libVer.avcodec.major == 57 || libVer.avcodec.major == 58)
  {
    auto p       = reinterpret_cast<AVCodec_56_57_58 *>(codec);
    name         = QString(p->name);
    long_name    = QString(p->long_name);
    type         = p->type;
    id           = p->id;
    capabilities = p->capabilities;
    if (p->supported_framerates)
    {
      int        i = 0;
      AVRational r = p->supported_framerates[i++];
      while (r.den != 0 && r.num != 0)
      {
        // Add and get the next one
        supported_framerates.append(r);
        r = p->supported_framerates[i++];
      }
    }
    if (p->pix_fmts)
    {
      int           i = 0;
      AVPixelFormat f = p->pix_fmts[i++];
      while (f != -1)
      {
        // Add and get the next one
        pix_fmts.append(f);
        f = p->pix_fmts[i++];
      }
    }
    if (p->supported_samplerates)
    {
      int i    = 0;
      int rate = p->supported_samplerates[i++];
      while (rate != 0)
      {
        supported_samplerates.append(rate);
        rate = p->supported_samplerates[i++];
      }
    }
    if (p->sample_fmts)
    {
      int            i = 0;
      AVSampleFormat f = p->sample_fmts[i++];
      while (f != -1)
      {
        sample_fmts.append(f);
        f = p->sample_fmts[i++];
      }
    }
    if (p->channel_layouts)
    {
      int      i = 0;
      uint64_t l = p->channel_layouts[i++];
      while (l != 0)
      {
        channel_layouts.append(l);
        l = p->channel_layouts[i++];
      }
    }
    max_lowres = p->max_lowres;
  }
  else
    throw std::runtime_error("Invalid library version");
}

} // namespace FFmpeg