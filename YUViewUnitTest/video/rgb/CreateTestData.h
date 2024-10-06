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

#include <QByteArray>
#include <array>
#include <video/rgb/PixelFormatRGB.h>

namespace video::rgb::test
{

constexpr std::array<rgba_t, 16> TEST_VALUES_12BIT = {{{0, 0, 0, 0},
                                                       {156, 0, 0, 0},
                                                       {560, 0, 0, 98},
                                                       {1023, 0, 0, 700},
                                                       {0, 156, 0, 852},
                                                       {0, 760, 0, 0},
                                                       {0, 1023, 0, 230},
                                                       {0, 0, 156, 0},
                                                       {0, 0, 576, 0},
                                                       {0, 0, 1023, 0},
                                                       {213, 214, 265, 1023},
                                                       {1023, 78, 234, 1023},
                                                       {1023, 1023, 3, 0},
                                                       {16, 1023, 22, 0},
                                                       {1023, 1023, 1023, 0},
                                                       {1023, 1023, 1023, 1023}}};
constexpr Size                   TEST_FRAME_SIZE   = {4, 4};
constexpr auto TEST_FRAME_NR_VALUES                = TEST_FRAME_SIZE.width * TEST_FRAME_SIZE.height;

QByteArray createRawRGBData(const PixelFormatRGB &format);

} // namespace video::rgb::test
