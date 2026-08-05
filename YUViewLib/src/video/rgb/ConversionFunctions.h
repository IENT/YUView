/*  This file is part of YUView - The YUV player with advanced analytics toolset
 *   <https://github.com/IENT/YUView>
 *   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
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

#include <QByteArray>

#include "PixelFormatRGB.h"

#include <stdexcept>

namespace video::rgb
{

/**
 * @brief Pointers into the start of each RGB(A) plane/component in a raw frame buffer.
 *
 * @tparam T Sample storage type (uint8_t / uint16_t / uint32_t)
 */
template <typename T> struct DataPointers
{
  const T *r{};
  const T *g{};
  const T *b{};
  const T *a{};

  /**
   * @brief Advance all component pointers by the same sample offset.
   *
   * @param offset Number of samples to advance
   * @return Updated pointer set
   */
  DataPointers operator+=(const int offset)
  {
    this->r += offset;
    this->g += offset;
    this->b += offset;
    if (this->a)
      this->a += offset;
    return *this;
  }
};

/**
 * @brief Compute pointers to the first R/G/B(/A) sample for planar or packed RGB layouts.
 *
 * Adapted to the local PixelFormatRGB API (bytesPerFrame / getChannelPosition).
 *
 * @tparam T Sample storage type
 * @param rawFrameData Raw RGB frame bytes
 * @param frameSize Frame dimensions
 * @param pixelFormat Source RGB format
 * @return Pointers to the first R, G, B and optional A samples
 */
template <typename T>
DataPointers<T> calculatePointersToStartOfComponents(const QByteArray     &rawFrameData,
                                                     const Size           &frameSize,
                                                     const PixelFormatRGB &pixelFormat)
{
  if (!pixelFormat.isValid())
    throw std::invalid_argument("Pixel format must be valid");
  if (!frameSize)
    throw std::invalid_argument("Frame size must be valid");
  if (rawFrameData.size() < static_cast<int>(pixelFormat.bytesPerFrame(frameSize)))
    throw std::invalid_argument("Raw frame data too small");

  const auto posR = pixelFormat.getChannelPosition(Channel::Red);
  const auto posG = pixelFormat.getChannelPosition(Channel::Green);
  const auto posB = pixelFormat.getChannelPosition(Channel::Blue);
  const auto posA = pixelFormat.getChannelPosition(Channel::Alpha);

  const auto castDataPointer = reinterpret_cast<T const *>(rawFrameData.data());

  DataPointers<T> dataPointers;
  if (pixelFormat.getDataLayout() == DataLayout::Planar)
  {
    const auto offsetToNextPlane = frameSize.width * frameSize.height;

    dataPointers.r = castDataPointer + (posR * offsetToNextPlane);
    dataPointers.g = castDataPointer + (posG * offsetToNextPlane);
    dataPointers.b = castDataPointer + (posB * offsetToNextPlane);
    dataPointers.a =
        pixelFormat.hasAlpha() ? castDataPointer + (posA * offsetToNextPlane) : nullptr;
  }
  else
  {
    dataPointers.r = castDataPointer + posR;
    dataPointers.g = castDataPointer + posG;
    dataPointers.b = castDataPointer + posB;
    dataPointers.a = pixelFormat.hasAlpha() ? castDataPointer + posA : nullptr;
  }

  return dataPointers;
}

/**
 * @brief Swap byte order of an integer sample for big-endian RGB sources.
 *
 * @tparam bitDepth Nominal bit depth of the sample (8 / 16 / 32)
 * @tparam T Sample value type
 * @param val Input sample
 * @return Endianness-corrected sample
 */
template <int bitDepth, typename T> inline T swapBytesEndianness(const T &val)
{
  if (bitDepth <= 8)
    return val;
  if (bitDepth <= 16)
    return static_cast<T>(((val & 0xff) << 8) | ((val & 0xff00) >> 8));
  return static_cast<T>(((val & 0xff) << 24) | ((val & 0xff00) << 8) | ((val & 0xff0000) >> 8) |
                        ((val & 0xff000000) >> 24));
}

/**
 * @brief Decode one packed RGB565 pixel into an rgba_t value.
 *
 * RGB565 is not a first-class PixelFormatRGB on this branch; this helper is kept for
 * callers that already know the buffer layout is RGB565.
 *
 * @param data Pointer to two packed RGB565 bytes
 * @param endianness Byte order of the 16-bit word
 * @return Decoded RGBA sample (A fixed to 255)
 */
inline rgba_t extractRGB565Value(const unsigned char *data, const Endianness endianness)
{
  int byte1 = *data;
  int byte2 = *(data + 1);

  if (endianness == Endianness::Big)
    std::swap(byte1, byte2);

  const auto value = byte1 + (byte2 << 8);

  const unsigned r = static_cast<unsigned>((value & 0b11111000'00000000) >> 11);
  const unsigned g = static_cast<unsigned>((value & 0b00000111'11100000) >> 5);
  const unsigned b = static_cast<unsigned>(value & 0b00000000'00011111);

  return rgba_t{r, g, b, 255u};
}

} // namespace video::rgb
