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

#include <common/EnumMapper.h>
#include <common/Typedef.h>

#include "pixelFormat.h"

#include <string>

namespace video::rgb
{

enum class Channel
{
  Red,
  Green,
  Blue,
  Alpha
};

enum class ChannelOrder
{
  RGB,
  RBG,
  GRB,
  GBR,
  BRG,
  BGR
};

enum class AlphaMode
{
  None,
  First,
  Last
};

const auto ChannelOrderMapper = EnumMapper<ChannelOrder>({{ChannelOrder::RGB, "RGB"},
                                                          {ChannelOrder::RBG, "RBG"},
                                                          {ChannelOrder::GRB, "GRB"},
                                                          {ChannelOrder::GBR, "GBR"},
                                                          {ChannelOrder::BRG, "BRG"},
                                                          {ChannelOrder::BGR, "BGR"}});

// This class defines a specific RGB format with all properties like order of R/G/B, bitsPerValue,
// planarity...
class rgbPixelFormat
{
public:
  // The default constructor (will create an "Unknown Pixel Format")
  rgbPixelFormat() = default;
  rgbPixelFormat(const std::string &name);
  rgbPixelFormat(unsigned     bitsPerSample,
                 DataLayout   dataLayout,
                 ChannelOrder channelOrder,
                 AlphaMode    alphaMode  = AlphaMode::None,
                 Endianness   endianness = Endianness::Little);

  bool        isValid() const;
  unsigned    nrChannels() const;
  bool        hasAlpha() const;
  std::string getName() const;

  unsigned     getBitsPerSample() const { return this->bitsPerSample; }
  DataLayout   getDataLayout() const { return this->dataLayout; }
  ChannelOrder getChannelOrder() const { return this->channelOrder; }

  void setBitsPerSample(unsigned bitsPerSample) { this->bitsPerSample = bitsPerSample; }
  void setDataLayout(DataLayout dataLayout) { this->dataLayout = dataLayout; }

  std::size_t bytesPerFrame(Size frameSize) const;
  int         getComponentPosition(Channel channel) const;

  bool operator==(const rgbPixelFormat &a) const { return getName() == a.getName(); }
  bool operator!=(const rgbPixelFormat &a) const { return getName() != a.getName(); }
  bool operator==(const std::string &a) const { return getName() == a; }
  bool operator!=(const std::string &a) const { return getName() != a; }

private:
  unsigned     bitsPerSample{0};
  DataLayout   dataLayout{DataLayout::Packed};
  ChannelOrder channelOrder{ChannelOrder::RGB};
  AlphaMode    alphaMode{AlphaMode::None};
  Endianness   endianness{Endianness::Little};
};

} // namespace video::rgb
