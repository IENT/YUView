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

#include <video/rgb/PixelFormatRGB.h>

#include <QByteArray>
#include <QImage>

#include <ostream>

namespace video::rgb
{

struct InputFrameParameters
{
  const QByteArray &rawDataItem;
  const Size        frameSize{};
};

struct MSE
{
  double r{};
  double g{};
  double b{};
  double a{};

  bool operator==(const MSE &other) const
  {
    return std::tie(r, g, b, a) == std::tie(other.r, other.g, other.b, other.a);
  }
};

void PrintTo(const MSE &mse, std::ostream *os);

// Sum of Squared Errors
class SSE
{
public:
  void addSample(const rgba_t &delta)
  {
    this->r += delta.r * delta.r;
    this->g += delta.g * delta.g;
    this->b += delta.b * delta.b;
    this->a += delta.a * delta.a;
    ++this->nrSamples;
  }

  MSE getMSE(const bool hasAlpha) const
  {
    MSE mse;
    mse.r = static_cast<double>(this->r) / this->nrSamples;
    mse.g = static_cast<double>(this->g) / this->nrSamples;
    mse.b = static_cast<double>(this->b) / this->nrSamples;
    mse.a = hasAlpha ? static_cast<double>(this->a) / this->nrSamples : 0.0;
    return mse;
  }

private:
  int64_t r{};
  int64_t g{};
  int64_t b{};
  int64_t a{};
  int64_t nrSamples{};
};

std::pair<QImage, MSE> calculateDifferenceAndMSE(const InputFrameParameters &frame1,
                                                 const InputFrameParameters &frame2,
                                                 const PixelFormatRGB       &pixelFormat,
                                                 const int                   amplificationFactor,
                                                 const bool                  markDifference);

} // namespace video::rgb
