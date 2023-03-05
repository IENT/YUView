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

#include <video/yuv/Restrict.h>

namespace video::yuv
{

inline int getValueFromSource(const unsigned char *restrict src,
                              const int                     idx,
                              const int                     bitsPerSample,
                              const bool                    bigEndian)
{
  if (bitsPerSample > 8)
    // Read two bytes in the right order
    return (bigEndian) ? src[idx * 2] << 8 | src[idx * 2 + 1]
                       : src[idx * 2] | src[idx * 2 + 1] << 8;
  else
    // Just read one byte
    return src[idx];
}

inline void setValueInBuffer(
    unsigned char *restrict dst, const int val, const int idx, const int bps, const bool bigEndian)
{
  if (bps > 8)
  {
    // Write two bytes
    if (bigEndian)
    {
      dst[idx * 2]     = val >> 8;
      dst[idx * 2 + 1] = val & 0xff;
    }
    else
    {
      dst[idx * 2]     = val & 0xff;
      dst[idx * 2 + 1] = val >> 8;
    }
  }
  else
    // Write one byte
    dst[idx] = val;
}

int getChromaStrideInSamples(const PixelFormatYUV &pixelFormat, const Size frameSize)
{
  switch (pixelFormat.getChromaPacking())
  {
  case ChromaPacking::Planar:
    return (frameSize.width / pixelFormat.getSubsamplingHor());
  case ChromaPacking::PerLine:
  case ChromaPacking::PerValue:
    return frameSize.width;
  }
}

} // namespace video::yuv
