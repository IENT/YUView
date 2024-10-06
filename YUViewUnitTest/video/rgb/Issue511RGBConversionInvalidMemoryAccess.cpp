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

#include <common/Testing.h>

#include <video/rgb/ConversionRGB.h>

#include <QByteArray>

#include <random>

namespace video::rgb::test
{

namespace
{

constexpr Size TEST_FRAME_SIZE = {3840, 2160};

auto createRandomRawRGBData() -> QByteArray
{
  QByteArray data;

  std::random_device                                       randomDevice;
  std::mt19937                                             randomNumberGenerator(randomDevice());
  std::uniform_int_distribution<std::mt19937::result_type> distribution(0, 255);

  constexpr auto NR_RGB_BYTES = TEST_FRAME_SIZE.width * TEST_FRAME_SIZE.height * 3;
  for (unsigned i = 0; i < NR_RGB_BYTES; i++)
    data.push_back(distribution(randomNumberGenerator));

  data.squeeze();

  return data;
}

} // namespace

TEST(Issue511RGBConversionInvalidMemoryAccess, Test)
{
  PixelFormatRGB format(8, DataLayout::Planar, ChannelOrder::RGB);
  const auto     data = createRandomRawRGBData();

  constexpr auto NR_RGB_BYTES_OUTPUT = TEST_FRAME_SIZE.width * TEST_FRAME_SIZE.height * 4;

  std::vector<unsigned char> outputBuffer;
  outputBuffer.resize(NR_RGB_BYTES_OUTPUT);

  const std::array<bool, 4> componentInvert  = {false};
  const std::array<int, 4>  componentScale   = {1, 1, 1, 1};
  const auto                limitedRange     = false;
  const auto                convertAlpha     = false;
  const auto                premultiplyAlpha = false;

  convertInputRGBToARGB(data,
                        format,
                        outputBuffer.data(),
                        TEST_FRAME_SIZE,
                        componentInvert.data(),
                        componentScale.data(),
                        limitedRange,
                        convertAlpha,
                        premultiplyAlpha);
}

} // namespace video::rgb::test
