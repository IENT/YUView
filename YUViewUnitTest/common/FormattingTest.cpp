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

#include <common/Formatting.h>

namespace
{

template <typename T> void runFormatTest(const T &value, const std::string_view expectedString)
{
  std::ostringstream stream;
  stream << value;
  EXPECT_EQ(stream.str(), expectedString);
  EXPECT_EQ(to_string(value), expectedString);
}

TEST(FormattingTest, FormattingOfPair)
{
  runFormatTest(std::make_pair(11, 472), "(11, 472)");
  runFormatTest(std::make_pair(11.0, 472.23), "(11, 472.23)");
  runFormatTest(std::make_pair("abcde", "ghij"), "(abcde, ghij)");
}

TEST(FormattingTest, FormattingOfVector)
{
  runFormatTest(std::vector<int>({8, 22, 99, 0}), "[8, 22, 99, 0]");
  runFormatTest(std::vector<double>({8.22, 22.12, 99.0, 0.01}), "[8.22, 22.12, 99, 0.01]");
  runFormatTest(std::vector<std::string>({"abc", "def", "g", "hi"}), "[abc, def, g, hi]");
}

TEST(FormattingTest, FormattingOfSize)
{
  runFormatTest(Size(124, 156), "124x156");
  runFormatTest(Size(999, 125682), "999x125682");
}

TEST(FormattingTest, FormattingOfBool)
{
  EXPECT_EQ(to_string(false), "False");
  EXPECT_EQ(to_string(true), "True");
}

TEST(FormattingTest, FormattingOfOptional)
{
  runFormatTest(std::optional<int>(), "NA");
  runFormatTest(std::make_optional<int>(22), "22");
  runFormatTest(std::make_optional<double>(22.556), "22.556");
  runFormatTest(std::make_optional<std::string>("Test234"), "Test234");
}

TEST(FormattingTest, StringReplaceAll)
{
    EXPECT_EQ(stringReplaceAll("abcdefghi", 'c', 'g'), "abgdefghi");
    EXPECT_EQ(stringReplaceAll("abcdefghijklmn", {'c', 'h', 'm'}, 'g'), "abgdefggijklgn");
}

} // namespace
