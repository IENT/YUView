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

#include <common/Functions.h>

namespace functions::test
{

TEST(FunctionsTest, toUnsigned)
{
  EXPECT_EQ(toUnsigned("0"), 0);
  EXPECT_EQ(toUnsigned("256"), 256);
  EXPECT_EQ(toUnsigned("4294967295"), 4294967295);

  EXPECT_FALSE(toUnsigned(""));
  EXPECT_FALSE(toUnsigned(" "));
  EXPECT_FALSE(toUnsigned("4294967296"));
  EXPECT_FALSE(toUnsigned("-1"));
  EXPECT_FALSE(toUnsigned("-256"));
  EXPECT_FALSE(toUnsigned("24A"));
  EXPECT_FALSE(toUnsigned("A24"));
  EXPECT_FALSE(toUnsigned("NotANumber"));
}

TEST(FunctionsTest, toInt)
{
  EXPECT_EQ(toInt("0"), 0);
  EXPECT_EQ(toInt("256"), 256);
  EXPECT_EQ(toInt("2147483647"), 2147483647);
  EXPECT_EQ(toInt("-1"), -1);
  EXPECT_EQ(toInt("-256"), -256);
  EXPECT_EQ(toInt("-2147483648"), -2147483648);

  EXPECT_FALSE(toInt(""));
  EXPECT_FALSE(toInt(" "));
  EXPECT_FALSE(toInt("2147483648"));
  EXPECT_FALSE(toInt("-2147483649"));
  EXPECT_FALSE(toInt("24A"));
  EXPECT_FALSE(toInt("A24"));
  EXPECT_FALSE(toInt(" 24"));
  EXPECT_FALSE(toInt("NotANumber"));
}

TEST(FunctionsTest, stringToLower)
{
  EXPECT_EQ(toLower(""), "");
  EXPECT_EQ(toLower("Hello"), "hello");
  EXPECT_EQ(toLower("WORLD"), "world");
  EXPECT_EQ(toLower("C++"), "c++");
  EXPECT_EQ(toLower("AaBbCcDd"), "aabbccdd");
}

TEST(FunctionsTest, splitString)
{
  EXPECT_THAT(splitString("Hello,World,Test", ','), ElementsAre("Hello", "World", "Test"));
  EXPECT_THAT(splitString("a,b,c", ','), ElementsAre("a", "b", "c"));
  EXPECT_THAT(splitString("Test1;Test2;Test3", ';'), ElementsAre("Test1", "Test2", "Test3"));

  EXPECT_THAT(splitString("", ','), ElementsAre());
  EXPECT_THAT(splitString(",,", ','), ElementsAre("", ""));
  EXPECT_THAT(splitString("Hello,", ','), ElementsAre("Hello"));
  EXPECT_THAT(splitString("Hello;World;", ';'), ElementsAre("Hello", "World"));
}

TEST(FunctionsTest, stripWhitespace)
{
  EXPECT_EQ(stripWhitespace(" ABC"), "ABC");
  EXPECT_EQ(stripWhitespace("ABC "), "ABC");
  EXPECT_EQ(stripWhitespace(" ABC "), "ABC");
  EXPECT_EQ(stripWhitespace("  ABC  "), "ABC");
  EXPECT_EQ(stripWhitespace("ABC D E "), "ABC D E");
  EXPECT_EQ(stripWhitespace(" ABC D E"), "ABC D E");
  EXPECT_EQ(stripWhitespace(" ABC D E "), "ABC D E");
  EXPECT_EQ(stripWhitespace("   ABC D E   "), "ABC D E");
  EXPECT_EQ(stripWhitespace("     ABC      "), "ABC");
  EXPECT_EQ(stripWhitespace(" "), "");
  EXPECT_EQ(stripWhitespace(""), "");
  EXPECT_EQ(stripWhitespace("           "), "");
}

} // namespace functions::test
