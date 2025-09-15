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
#include <video/PixelFormat.h>

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

constexpr EnumMapper<Channel, 4> ChannelMapper = {std::make_pair(Channel::Red, "Red"),
                                                  std::make_pair(Channel::Green, "Green"),
                                                  std::make_pair(Channel::Blue, "Blue"),
                                                  std::make_pair(Channel::Alpha, "Alpha")};

struct rgba_t
{
  int r{0}, g{0}, b{0}, a{0};

  int &operator[](const Channel channel)
  {
    if (channel == Channel::Red)
      return this->r;
    if (channel == Channel::Green)
      return this->g;
    if (channel == Channel::Blue)
      return this->b;
    if (channel == Channel::Alpha)
      return this->a;

    throw std::out_of_range("Unsupported channel for value access");
  }

  int at(const Channel channel) const
  {
    if (channel == Channel::Red)
      return this->r;
    if (channel == Channel::Green)
      return this->g;
    if (channel == Channel::Blue)
      return this->b;
    if (channel == Channel::Alpha)
      return this->a;

    throw std::out_of_range("Unsupported channel for value access");
  }

  bool operator==(const rgba_t &other) const
  {
    return this->r == other.r && this->g == other.g && this->b == other.b && this->a == other.a;
  };

  bool operator!=(const rgba_t &other) const
  {
    return this->r != other.r || this->g != other.g || this->b != other.b || this->a != other.a;
  };

  rgba_t operator-(const rgba_t &other) const
  {
    return {this->r - other.r, this->g - other.g, this->b - other.b, this->a - other.a};
  }
};

template <typename T> inline T convertBitness(T value, unsigned src_bitness, unsigned dst_bitness)
{
  if (src_bitness > dst_bitness)
    return value >> (src_bitness - dst_bitness);
  else
    return value << (dst_bitness - src_bitness);
}

inline rgba_t convertBitness(rgba_t value, unsigned src_bitness, unsigned dst_bitness)
{
  return rgba_t({convertBitness(value.r, src_bitness, dst_bitness),
                 convertBitness(value.g, src_bitness, dst_bitness),
                 convertBitness(value.b, src_bitness, dst_bitness),
                 convertBitness(value.a, src_bitness, dst_bitness)});
}

enum class PredefinedPixelFormat
{
  RGB565, // 16 bits packed as R:5, G:6, B:5
};

constexpr EnumMapper<PredefinedPixelFormat, 2> PredefinedPixelFormatMapper = {
  std::make_pair(PredefinedPixelFormat::RGB565, "RGB565")};

enum class ChannelOrder
{
  RGB,
  RBG,
  GRB,
  GBR,
  BRG,
  BGR
};

constexpr EnumMapper<ChannelOrder, 6> ChannelOrderMapper = {
  std::make_pair(ChannelOrder::RGB, "RGB"),
  std::make_pair(ChannelOrder::RBG, "RBG"),
  std::make_pair(ChannelOrder::GRB, "GRB"),
  std::make_pair(ChannelOrder::GBR, "GBR"),
  std::make_pair(ChannelOrder::BRG, "BRG"),
  std::make_pair(ChannelOrder::BGR, "BGR")};

enum class AlphaMode
{
  None,
  First,
  Last
};

constexpr EnumMapper<AlphaMode, 3> AlphaModeMapper = {std::make_pair(AlphaMode::None, "None"),
                                                      std::make_pair(AlphaMode::First, "First"),
                                                      std::make_pair(AlphaMode::Last, "Last")};

class PixelFormatRGB
{
public:
  // The default constructed Pixel format will be invalid
  PixelFormatRGB() = default;
  PixelFormatRGB(const std::string &name);
  PixelFormatRGB(const int          bitsPerComponent,
                 const DataLayout   dataLayout,
                 const ChannelOrder channelOrder,
                 const AlphaMode    alphaMode  = AlphaMode::None,
                 const Endianness   endianness = Endianness::Little);
  PixelFormatRGB(const PredefinedPixelFormat predefinedPixelFormat,
                 const Endianness            endianness = Endianness::Little);

  [[nodiscard]] bool        isValid() const;
  [[nodiscard]] bool        hasAlpha() const;
  [[nodiscard]] std::string getName() const;

  [[nodiscard]] int                                  getBitsPerComponent() const;
  [[nodiscard]] DataLayout                           getDataLayout() const;
  [[nodiscard]] ChannelOrder                         getChannelOrder() const;
  [[nodiscard]] AlphaMode                            getAlphaMode() const;
  [[nodiscard]] Endianness                           getEndianess() const;
  [[nodiscard]] std::optional<PredefinedPixelFormat> getPredefinedPixelFormat() const;

  [[nodiscard]] int           getNrChannels() const;
  [[nodiscard]] int           getBytesPerFrame(const Size frameSize) const;
  [[nodiscard]] int           getChannelPosition(const Channel channel) const;
  [[nodiscard]] Channel       getChannelAtPosition(const int position) const;
  [[nodiscard]] TextRendering getPixelValueTextRendering(rgba_t value) const;

  bool operator==(const PixelFormatRGB &a) const;
  bool operator!=(const PixelFormatRGB &a) const;
  bool operator==(const std::string &a) const;
  bool operator!=(const std::string &a) const;

private:
  // If this is set, the format is defined according to a specific standard and does not
  // conform to the definition below (using
  // dataLayout/bitsPerSample/ChannelOder/alphaMode/Endianess). If this is set, none of the values
  // below matter.
  std::optional<PredefinedPixelFormat> predefinedPixelFormat;

  int          bitsPerComponent{0};
  DataLayout   dataLayout{DataLayout::Packed};
  ChannelOrder channelOrder{ChannelOrder::RGB};
  AlphaMode    alphaMode{AlphaMode::None};
  Endianness   endianness{Endianness::Little};
};

void PrintTo(const PixelFormatRGB &point, std::ostream *os);

} // namespace video::rgb
