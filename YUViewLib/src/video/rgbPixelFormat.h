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

#include <QSize>
#include <QString>
#include <optional>

namespace RGB_Internals
{

// This class defines a specific RGB format with all properties like order of R/G/B, bitsPerValue, planarity...
class rgbPixelFormat
{
public:
  // The default constructor (will create an "Unknown Pixel Format")
  rgbPixelFormat() {}
  rgbPixelFormat(const QString &name);
  rgbPixelFormat(int bitsPerValue, bool planar, int posR=0, int posG=1, int posB=2, int posA=-1);
  bool operator==(const rgbPixelFormat &a) const { return getName() == a.getName(); } // Comparing names should be enough since you are not supposed to create your own rgbPixelFormat instances anyways.
  bool operator!=(const rgbPixelFormat &a) const { return getName()!= a.getName(); }
  bool operator==(const QString &a) const { return getName() == a; }
  bool operator!=(const QString &a) const { return getName() != a; }
  bool isValid() const;
  std::optional<size_t>  nrChannels() const { return posA == -1 ? 3 : 4; }
  bool hasAlphaChannel() const { return posA != -1; }
  // Get a name representation of this item (this will be unique for the set parameters)
  QString getName() const;
  // Get/Set the RGB format from string (accepted string are: "RGB", "BGR", ...)
  QString getRGBFormatString() const;
  void setRGBFormatFromString(const QString &sFormat);
  // Get the number of bytes for a frame with this rgbPixelFormat and the given size
  std::optional<size_t> bytesPerFrame(const QSize &frameSize) const;
  
  // The order of each component (E.g. for GBR this is posR=2,posG=0,posB=1)
  int posR {0};
  int posG {1};
  int posB {2};
  int posA {-1};  // The position of alpha can be -1 (no alpha channel)
  int bitsPerValue {-1};
  bool planar {false};
};

} // namespace RGB_Internals
