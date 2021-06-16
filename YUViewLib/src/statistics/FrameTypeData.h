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

#include "common/typedef.h"

namespace stats
{

struct Point
{
  int x;
  int y;
  Point(){};
  Point(int a, int b)
  {
    x = a;
    y = b;
  };

  bool operator==(const Point &other) const { return this->x == other.x && this->y == other.y; }
  bool operator!=(const Point &other) const { return this->x != other.x || this->y != other.y; }
};

struct Line
{
  Point p1;
  Point p2;

  Line(Point a, Point b)
  {
    p1 = a;
    p2 = b;
  };
};

using Polygon = std::vector<Point>;

struct statisticsItem_Value
{
  // The position and size of the item. (max 65535)
  unsigned short pos[2];
  unsigned short size[2];

  // The actual value
  int value;
};

struct statisticsItem_Vector
{
  // The position and size of the item. (max 65535)
  unsigned short pos[2];
  unsigned short size[2];

  bool    isLine; // the vector is specified by two points
  Point point[2];
};

struct statisticsItem_AffineTF
{
  // The position and size of the item. (max 65535)
  unsigned short pos[2];
  unsigned short size[2];

  // the vector is specified by two points
  Point point[3];
};

struct statisticsItemPolygon_Value
{
  // The position and size of the item.
  Polygon corners;

  // The actual value
  int value;
};

struct statisticsItemPolygon_Vector
{
  // The position and size of the item.
  Polygon corners;

  Point point;
};

// A collection of statistics data (value and vector) for a certain context (for example for a
// certain type and a certain POC).
class FrameTypeData
{
public:
  FrameTypeData() { maxBlockSize = 0; }
  void
  addBlockValue(unsigned short x, unsigned short y, unsigned short w, unsigned short h, int val);
  void addBlockVector(
      unsigned short x, unsigned short y, unsigned short w, unsigned short h, int vecX, int vecY);
  void addBlockAffineTF(unsigned short x,
                        unsigned short y,
                        unsigned short w,
                        unsigned short h,
                        int            vecX0,
                        int            vecY0,
                        int            vecX1,
                        int            vecY1,
                        int            vecX2,
                        int            vecY2);
  void addLine(unsigned short x,
               unsigned short y,
               unsigned short w,
               unsigned short h,
               int            x1,
               int            y1,
               int            x2,
               int            y2);
  void addPolygonVector(const Polygon &points, int vecX, int vecY);
  void addPolygonValue(const Polygon &points, int val);

  std::vector<statisticsItem_Value>         valueData;
  std::vector<statisticsItem_Vector>        vectorData;
  std::vector<statisticsItem_AffineTF>      affineTFData;
  std::vector<statisticsItemPolygon_Value>  polygonValueData;
  std::vector<statisticsItemPolygon_Vector> polygonVectorData;

  // What is the size (area) of the biggest block)? This is needed for scaling the blocks according
  // to their size.
  unsigned maxBlockSize;
};

} // namespace stats
