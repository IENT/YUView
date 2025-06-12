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

constexpr auto UNKNOWN_FORMAT_NAME = "Unknown Pixel Format";

PixelFormatRGB::PixelFormatRGB(const int          bitsPerComponent,
                               const DataLayout   dataLayout,
                               const ChannelOrder channelOrder,
                               const AlphaMode    alphaMode,
                               const Endianness   endianness)
    : bitsPerComponent(bitsPerComponent), dataLayout(dataLayout), channelOrder(channelOrder),
      alphaMode(alphaMode), endianness(endianness)
{
}

PixelFormatRGB::PixelFormatRGB(const std::string &name)
{
  if (name == UNKNOWN_FORMAT_NAME)
    return;

  for (const auto predefinedFormat : PredefinedPixelFormatMapper)
    if (name == predefinedFormat.second)
    {
      this->predefinedPixelFormat = predefinedFormat.first;
      return;
    }

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
    this->bitsPerComponent = std::stoi(name.substr(bitIdx - 2, 2), nullptr);
  if (name.find("planar") != std::string::npos)
    this->dataLayout = DataLayout::Planar;
  if (this->bitsPerComponent > 8 && name.find("BE") != std::string::npos)
    this->endianness = Endianness::Big;
}

PixelFormatRGB::PixelFormatRGB(const PredefinedPixelFormat predefinedPixelFormat)
{
  this->predefinedPixelFormat = predefinedPixelFormat;
}

bool PixelFormatRGB::isValid() const
{
  if (this->predefinedPixelFormat)
    return true;

  if (this->bitsPerComponent == 8 && this->endianness == Endianness::Big)
    return false;

  return this->bitsPerComponent >= 8 && this->bitsPerComponent <= 32;
}

bool PixelFormatRGB::hasAlpha() const
{
  return this->alphaMode != AlphaMode::None;
}

std::string PixelFormatRGB::getName() const
{
  if (!this->isValid())
    return UNKNOWN_FORMAT_NAME;

  if (this->predefinedPixelFormat)
    return std::string(PredefinedPixelFormatMapper.getName(*this->predefinedPixelFormat));

  std::string name;
  if (this->alphaMode == AlphaMode::First)
    name += "A";
  name += ChannelOrderMapper.getName(this->channelOrder);
  if (this->alphaMode == AlphaMode::Last)
    name += "A";

  name += " " + std::to_string(this->bitsPerComponent) + "bit";
  if (this->dataLayout == DataLayout::Planar)
    name += " planar";
  if (this->bitsPerComponent > 8 && this->endianness == Endianness::Big)
    name += " BE";

  return name;
}

int PixelFormatRGB::getBitsPerComponent() const
{
  return this->bitsPerComponent;
}

DataLayout PixelFormatRGB::getDataLayout() const
{
  return this->dataLayout;
}

ChannelOrder PixelFormatRGB::getChannelOrder() const
{
  return this->channelOrder;
}

AlphaMode PixelFormatRGB::getAlphaMode() const
{
  return this->alphaMode;
}

Endianness PixelFormatRGB::getEndianess() const
{
  return this->endianness;
}

std::optional<PredefinedPixelFormat> PixelFormatRGB::getPredefinedPixelFormat() const
{
  return this->predefinedPixelFormat;
}

int PixelFormatRGB::getNrChannels() const
{
  if (this->predefinedPixelFormat == PredefinedPixelFormat::RGB565 ||
      this->predefinedPixelFormat == PredefinedPixelFormat::RGB565BE)
    return 3;

  return this->alphaMode != AlphaMode::None ? 4 : 3;
}

int PixelFormatRGB::getBytesPerFrame(const Size frameSize) const
{
  if (!this->isValid() || !frameSize.isValid())
    return 0;

  const auto numberSamples = std::size_t(frameSize.height) * std::size_t(frameSize.width);

  int numberBytesPerFrame;
  if (this->predefinedPixelFormat == PredefinedPixelFormat::RGB565 ||
      this->predefinedPixelFormat == PredefinedPixelFormat::RGB565BE)
  {
    numberBytesPerFrame = numberSamples * 2;
  }
  else
  {
    const auto numberBytesPerComponent = ((this->bitsPerComponent + 7) / 8);
    numberBytesPerFrame = numberSamples * numberBytesPerComponent * this->getNrChannels();
  }

  DEBUG_RGB_FORMAT(
    "PixelFormatRGB::bytesPerFrame numberSamples %d numberSamples %d numberBytesPerFrame %d",
    int(numberSamples),
    this->nrChannels(),
    numberBytesPerFrame);

  return numberBytesPerFrame;
}

int PixelFormatRGB::getChannelPosition(Channel channel) const
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

Channel PixelFormatRGB::getChannelAtPosition(int position) const
{
  if (this->hasAlpha())
  {
    if (position == 0 && this->alphaMode == AlphaMode::First)
      return Channel::Alpha;
    if (position == 3 && this->alphaMode == AlphaMode::Last)
      return Channel::Alpha;

    if (this->alphaMode == AlphaMode::First)
      position--;
  }

  if (position == 0)
  {
    if (this->channelOrder == ChannelOrder::RGB || this->channelOrder == ChannelOrder::RBG)
      return Channel::Red;
    if (this->channelOrder == ChannelOrder::GRB || this->channelOrder == ChannelOrder::GBR)
      return Channel::Green;
    if (this->channelOrder == ChannelOrder::BGR || this->channelOrder == ChannelOrder::BRG)
      return Channel::Blue;
  }
  else if (position == 1)
  {
    if (this->channelOrder == ChannelOrder::GRB || this->channelOrder == ChannelOrder::BRG)
      return Channel::Red;
    if (this->channelOrder == ChannelOrder::RGB || this->channelOrder == ChannelOrder::BGR)
      return Channel::Green;
    if (this->channelOrder == ChannelOrder::RBG || this->channelOrder == ChannelOrder::GBR)
      return Channel::Blue;
  }
  else if (position == 2)
  {
    if (this->channelOrder == ChannelOrder::GBR || this->channelOrder == ChannelOrder::BGR)
      return Channel::Red;
    if (this->channelOrder == ChannelOrder::RBG || this->channelOrder == ChannelOrder::BRG)
      return Channel::Green;
    if (this->channelOrder == ChannelOrder::RGB || this->channelOrder == ChannelOrder::GRB)
      return Channel::Blue;
  }

  throw std::invalid_argument("Invalid argument for channel position");
}

TextRendering PixelFormatRGB::getPixelValueTextRendering(rgba_t value) const
{
  // Shift the values to 8 bit
  if (this->predefinedPixelFormat)
  {
    value.R = (value.R << 3);
    value.G = (value.G << 2);
    value.B = (value.B << 3);
  }
  else if (this->bitsPerComponent > 8)
  {
    const auto shift = (this->bitsPerComponent - 8);
    value.R          = (value.R >> shift);
    value.G          = (value.G >> shift);
    value.B          = (value.B >> shift);
  }

  // Approximation of Y = 0.375 R + 0.5 G + 0.125 B to be closer to the percieved brightness.
  const auto luminance = (3 * value.R + 4 * value.G + value.B) >> 3;
  return luminance < 128 ? TextRendering::White : TextRendering::Black;
}

bool PixelFormatRGB::operator==(const PixelFormatRGB &a) const
{
  if (!this->isValid() || !a.isValid())
    return false;

  if (this->predefinedPixelFormat)
    return this->predefinedPixelFormat == a.predefinedPixelFormat;

  return this->bitsPerComponent == a.bitsPerComponent && this->dataLayout == a.dataLayout &&
         this->channelOrder == a.channelOrder && this->alphaMode == a.alphaMode &&
         this->endianness == a.endianness;
}

bool PixelFormatRGB::operator!=(const PixelFormatRGB &a) const
{
  return !(*this == a);
}

bool PixelFormatRGB::operator==(const std::string &a) const
{
  if (!this->isValid() || a == UNKNOWN_FORMAT_NAME)
    return false;

  return this->getName() == a;
}

bool PixelFormatRGB::operator!=(const std::string &a) const
{
  if (!this->isValid() || a == UNKNOWN_FORMAT_NAME)
    return true;

  return this->getName() != a;
}

void PrintTo(const PixelFormatRGB &pixelFormatRGB, std::ostream *os)
{
  *os << pixelFormatRGB.getName();
}

} // namespace video::rgb
