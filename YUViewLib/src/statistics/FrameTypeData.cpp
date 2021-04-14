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

#include "FrameTypeData.h"

namespace stats
{

void FrameTypeData::addBlockValue(
    unsigned short x, unsigned short y, unsigned short w, unsigned short h, int val)
{
  statisticsItem_Value value;
  value.pos[0]  = x;
  value.pos[1]  = y;
  value.size[0] = w;
  value.size[1] = h;
  value.value   = val;

  // Always keep the biggest block size updated.
  unsigned int wh = w * h;
  if (wh > maxBlockSize)
    maxBlockSize = wh;

  valueData.push_back(value);
}

void FrameTypeData::addBlockVector(
    unsigned short x, unsigned short y, unsigned short w, unsigned short h, int vecX, int vecY)
{
  statisticsItem_Vector vec;
  vec.pos[0]   = x;
  vec.pos[1]   = y;
  vec.size[0]  = w;
  vec.size[1]  = h;
  vec.point[0] = IntPair({vecX, vecY});
  vec.isLine   = false;
  vectorData.push_back(vec);
}

void FrameTypeData::addBlockAffineTF(unsigned short x,
                                     unsigned short y,
                                     unsigned short w,
                                     unsigned short h,
                                     int            vecX0,
                                     int            vecY0,
                                     int            vecX1,
                                     int            vecY1,
                                     int            vecX2,
                                     int            vecY2)
{
  statisticsItem_AffineTF affineTF;
  affineTF.pos[0]   = x;
  affineTF.pos[1]   = y;
  affineTF.size[0]  = w;
  affineTF.size[1]  = h;
  affineTF.point[0] = IntPair({vecX0, vecY0});
  affineTF.point[1] = IntPair({vecX1, vecY1});
  affineTF.point[2] = IntPair({vecX2, vecY2});
  affineTFData.push_back(affineTF);
}

void FrameTypeData::addLine(unsigned short x,
                            unsigned short y,
                            unsigned short w,
                            unsigned short h,
                            int            x1,
                            int            y1,
                            int            x2,
                            int            y2)
{
  statisticsItem_Vector vec;
  vec.pos[0]   = x;
  vec.pos[1]   = y;
  vec.size[0]  = w;
  vec.size[1]  = h;
  vec.point[0] = IntPair({x1, y1});
  vec.point[1] = IntPair({x2, y2});
  vec.isLine   = true;
  vectorData.push_back(vec);
}

void FrameTypeData::addPolygonValue(const Polygon &points, int val)
{
  statisticsItemPolygon_Value value;
  value.corners = points;
  value.value   = val;

  // todo: how to do this nicely?
  //  // Always keep the biggest block size updated.
  //  unsigned int wh = w*h;
  //  if (wh > maxBlockSize)
  //    maxBlockSize = wh;

  polygonValueData.push_back(value);
}

void FrameTypeData::addPolygonVector(const Polygon &points, int vecX, int vecY)
{
  statisticsItemPolygon_Vector vec;
  vec.corners  = points;
  vec.point = IntPair({vecX, vecY});
  polygonVectorData.push_back(vec);
}

} // namespace stats
