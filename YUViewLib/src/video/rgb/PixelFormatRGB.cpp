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

#include "PixelFormatRGB.h"

// Activate this if you want to know when which buffer is loaded/converted to image and so on.
#define RGBPIXELFORMAT_DEBUG 0
#if RGBPIXELFORMAT_DEBUG && !NDEBUG
#include <QDebug>
#define DEBUG_RGB_FORMAT qDebug
#else
#define DEBUG_RGB_FORMAT(fmt, ...) ((void)0)
#endif

namespace video::rgb
{

PixelFormatRGB::PixelFormatRGB(unsigned     bitsPerSample,
                               DataLayout   dataLayout,
                               ChannelOrder channelOrder,
                               AlphaMode    alphaMode,
                               Endianness   endianness)
    : bitsPerSample(bitsPerSample), dataLayout(dataLayout), channelOrder(channelOrder),
      alphaMode(alphaMode), endianness(endianness)
{
}

PixelFormatRGB::PixelFormatRGB(const std::string &name)
{
  if (name != "Unknown Pixel Format")
  {
    auto channelOrderString = name.substr(0, 3);
    if (name[0] == 'a' || name[0] == 'A')
    {
      this->alphaMode    = AlphaMode::First;
      channelOrderString = name.substr(1, 3);
    }
    else if (name[3] == 'a' || name[3] == 'A')
    {
      this->alphaMode    = AlphaMode::Last;
      channelOrderString = name.substr(0, 3);
    }
    auto order = ChannelOrderMapper.getValue(channelOrderString);
    if (order)
      this->channelOrder = *order;

    auto bitIdx = name.find("bit");
    if (bitIdx != std::string::npos)
      this->bitsPerSample = std::stoi(name.substr(bitIdx - 2, 2), nullptr);
    if (name.find("planar") != std::string::npos)
      this->dataLayout = DataLayout::Planar;
    if (this->bitsPerSample > 8 && name.find("BE") != std::string::npos)
      this->endianness = Endianness::Big;
  }
}

bool PixelFormatRGB::isValid() const
{
  return this->bitsPerSample >= 8 && this->bitsPerSample <= 16;
}

unsigned PixelFormatRGB::nrChannels() const
{
  return this->alphaMode != AlphaMode::None ? 4 : 3;
}

bool PixelFormatRGB::hasAlpha() const
{
  return this->alphaMode != AlphaMode::None;
}

std::string PixelFormatRGB::getName() const
{
  if (!this->isValid())
    return "Unknown Pixel Format";

  std::string name;
  if (this->alphaMode == AlphaMode::First)
    name += "A";
  name += ChannelOrderMapper.getName(this->channelOrder);
  if (this->alphaMode == AlphaMode::Last)
    name += "A";

  name += " " + std::to_string(this->bitsPerSample) + "bit";
  if (this->dataLayout == DataLayout::Planar)
    name += " planar";
  if (this->bitsPerSample > 8 && this->endianness == Endianness::Big)
    name += " BE";

  return name;
}

/* Get the number of bytes for a frame with this RGB format and the given size
 */
std::size_t PixelFormatRGB::bytesPerFrame(Size frameSize) const
{
  auto bpsValid = this->bitsPerSample >= 8 && this->bitsPerSample <= 16;
  if (!bpsValid || !frameSize.isValid())
    return 0;

  auto numSamples = std::size_t(frameSize.height) * std::size_t(frameSize.width);
  auto nrBytes    = numSamples * this->nrChannels() * ((this->bitsPerSample + 7) / 8);
  DEBUG_RGB_FORMAT("PixelFormatRGB::bytesPerFrame samples %d channels %d bytes %d",
                   int(numSamples),
                   this->nrChannels(),
                   nrBytes);
  return nrBytes;
}

int PixelFormatRGB::getComponentPosition(Channel channel) const
{
  if (channel == Channel::Alpha)
  {
    switch (this->alphaMode)
    {
    case AlphaMode::First:
      return 0;
    case AlphaMode::Last:
      return 3;
    default:
      return -1;
    }
  }

  auto rgbIdx = 0;
  if (channel == Channel::Red)
  {
    if (this->channelOrder == ChannelOrder::RGB || this->channelOrder == ChannelOrder::RBG)
      rgbIdx = 0;
    if (this->channelOrder == ChannelOrder::GRB || this->channelOrder == ChannelOrder::BRG)
      rgbIdx = 1;
    if (this->channelOrder == ChannelOrder::GBR || this->channelOrder == ChannelOrder::BGR)
      rgbIdx = 2;
  }
  else if (channel == Channel::Green)
  {
    if (this->channelOrder == ChannelOrder::GRB || this->channelOrder == ChannelOrder::GBR)
      rgbIdx = 0;
    if (this->channelOrder == ChannelOrder::RGB || this->channelOrder == ChannelOrder::BGR)
      rgbIdx = 1;
    if (this->channelOrder == ChannelOrder::RBG || this->channelOrder == ChannelOrder::BRG)
      rgbIdx = 2;
  }
  else if (channel == Channel::Blue)
  {
    if (this->channelOrder == ChannelOrder::BGR || this->channelOrder == ChannelOrder::BRG)
      rgbIdx = 0;
    if (this->channelOrder == ChannelOrder::RBG || this->channelOrder == ChannelOrder::GBR)
      rgbIdx = 1;
    if (this->channelOrder == ChannelOrder::RGB || this->channelOrder == ChannelOrder::GRB)
      rgbIdx = 2;
  }

  if (this->alphaMode == AlphaMode::First)
    return rgbIdx + 1;
  return rgbIdx;
}

} // namespace video::rgb