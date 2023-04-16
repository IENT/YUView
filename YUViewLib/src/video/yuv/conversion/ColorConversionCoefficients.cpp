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

#include "ColorConversionCoefficients.h"

namespace video::yuv::conversion
{

ColorConversionCoefficients getColorConversionCoefficients(ColorConversion colorConversion)
{
  // The values are [Y, cRV, cGU, cGV, cBU]
  switch (colorConversion)
  {
  case ColorConversion::BT709_LimitedRange:
    return {76309, 117489, -13975, -34925, 138438};
  case ColorConversion::BT709_FullRange:
    return {65536, 103206, -12276, -30679, 121608};
  case ColorConversion::BT601_LimitedRange:
    return {76309, 104597, -25675, -53279, 132201};
  case ColorConversion::BT601_FullRange:
    return {65536, 91881, -22553, -46802, 116129};
  case ColorConversion::BT2020_LimitedRange:
    return {76309, 110013, -12276, -42626, 140363};
  case ColorConversion::BT2020_FullRange:
    return {65536, 96638, -10783, -37444, 123299};

  default:
    return {};
  }
}

} // namespace video::yuv::conversion
