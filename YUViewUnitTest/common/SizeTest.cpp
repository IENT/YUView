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

#include <common/Size.h>

namespace common::test
{

TEST(SizeTest, defaultConstruction)
{
  Size size();

  EXPECT_EQ(size.x, 0);
  EXPECT_EQ(size.y, 0);
}

TEST(SizeTest, valueConstruction)
{
  Size size(22, 43);

  EXPECT_EQ(size.x, 22);
  EXPECT_EQ(size.y, 43);
}

TEST(SizeTest, equalityTestForEqualSizes)
{
  Size size1(22, 43);
  Size size2(22, 43);
  EXPECT_TRUE(size1 == size2);
  EXPECT_FALSE(size1 != size2);

  Size size3(0, 0);
  Size size4;
  EXPECT_TRUE(size3 == size4);
  EXPECT_FALSE(size3 != size4);
}

TEST(SizeTest, equalityTestForUnequalSizes_widthDifferentWidth)
{
  Size size1(22, 44);
  Size size2(222, 44);
  EXPECT_TRUE(size1 != size2);
  EXPECT_FALSE(size1 == size2);
}

TEST(SizeTest, equalityTestForUnequalSizes_widthDifferentHeight)
{
  Size size1(22, 44);
  Size size2(22, 444);
  EXPECT_TRUE(size1 != size2);
  EXPECT_FALSE(size1 == size2);
}

TEST(SizeTest, equalityTestForUnequalSizes_widthDifferentWidthAndHeight)
{
  Size size1(22, 44);
  Size size2(222, 444);
  EXPECT_TRUE(size1 != size2);
  EXPECT_FALSE(size1 == size2);
}

} // namespace common::test
