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

#include <common/Typedef.h>

namespace stats
{

using Value = int;

struct Point
{
  int x{};
  int y{};

  Point(){};
  Point(int x, int y)
  {
    this->x = x;
    this->y = y;
  };

  bool operator==(const Point &other) const { return this->x == other.x && this->y == other.y; }
  bool operator!=(const Point &other) const { return this->x != other.x || this->y != other.y; }
};

struct Line
{
  Point p1{};
  Point p2{};

  Line() = default;
  Line(Point a, Point b)
  {
    p1 = a;
    p2 = b;
  };
};

using Polygon = std::vector<Point>;

struct Block
{
  Point pos{};
  int   width{};
  int   height{};

  Block(int x, int y, int width, int height)
  {
    this->pos    = Point(x, y);
    this->width  = width;
    this->height = height;
  }

  bool contains(const Point &point) const
  {
    const auto isInHorizontal = (point.x >= this->pos.x) && (point.x < this->pos.x + this->width);
    const auto isInVertical   = (point.y >= this->pos.y) && (point.y < this->pos.y + this->height);
    return isInHorizontal && isInVertical;
  }
  int area() const { return this->width * this->height; }
};

struct Vector
{
  Vector() = default;
  Vector(int x, int y)
  {
    this->x = x;
    this->y = y;
  }
  int x{};
  int y{};
};

struct BlockWithValue : public Block
{
  BlockWithValue(const Block &block, int value) : Block(block) { this->value = value; }
  Value value{};
};

struct BlockWithVector : public Block
{
  BlockWithVector(const Block &block, const Vector &vector) : Block(block)
  {
    this->vector = vector;
  }
  Vector vector{};
};

struct BlockWithLine : public Block
{
  BlockWithLine(const Block &block, const Line &line) : Block(block) { this->line = line; }
  Line line{};
};

struct BlockWithAffineTF : public Block
{
  Point point[3]{};
};

struct PolygonWithValue : public Polygon
{
  Value value{};
};

struct PolygonWithVector : public Polygon
{
  Vector vector{};
};

// A collection of statistics data (value and vector) for a certain context (for example for a
// certain type and a certain POC).
struct DataPerType
{
  std::vector<BlockWithValue>    valueData{};
  std::vector<BlockWithVector>   vectorData{};
  std::vector<BlockWithLine>     lineData{};
  std::vector<BlockWithAffineTF> affineTFData{};
  std::vector<PolygonWithValue>  polygonValueData{};
  std::vector<PolygonWithVector> polygonVectorData{};
};

} // namespace stats
