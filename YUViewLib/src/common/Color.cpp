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

#include "Color.h"

#include <iomanip>
#include <sstream>

Color::Color(std::string name)
{
  if ((name.size() == 7 || name.size() == 9) && name[0] == '#')
    name.erase(0, 1);
  if (name.size() == 6 || name.size() == 8)
  {
    this->values[0] = std::stoi(name.substr(0, 2), nullptr, 16);
    this->values[1] = std::stoi(name.substr(2, 2), nullptr, 16);
    this->values[2] = std::stoi(name.substr(4, 2), nullptr, 16);
    if (name.size() == 8)
      this->values[3] = std::stoi(name.substr(6, 2), nullptr, 16);
  }
}

std::string Color::toHex() const
{
  std::stringstream stream;
  stream << "#" << std::hex << this->values[0] << std::hex << this->values[1] << std::hex
         << this->values[2];
  if (this->values[3] >= 0)
    stream << std::hex << this->values[3];
  return stream.str();
}

Color::Color(int R, int G, int B, int A = -1)
{
  this->values[0] = R;
  this->values[1] = G;
  this->values[2] = B;
  this->values[3] = A;
}
