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

#include <common/EnumMapper.h>

namespace
{

enum class TestEnum
{
  ValueOne,
  ValueTwo,
  ValueThree
};

constexpr EnumMapper<TestEnum, 3> TestEnumMapper(std::make_pair(TestEnum::ValueOne, "ValueOne"sv),
                                                 std::make_pair(TestEnum::ValueTwo, "ValueTwo"sv),
                                                 std::make_pair(TestEnum::ValueThree,
                                                                "ValueThree"sv));

const std::vector<TestEnum> expectedValues = {
    TestEnum::ValueOne, TestEnum::ValueTwo, TestEnum::ValueThree};
const std::vector<std::string> expectedNames = {"ValueOne", "ValueTwo", "ValueThree"};

TEST(EnumMapperTest, testSize)
{
  ASSERT_EQ(TestEnumMapper.size(), 3u);
}

TEST(EnumMapperTest, testGetName)
{
  EXPECT_EQ(TestEnumMapper.getName(TestEnum::ValueOne), "ValueOne");
  EXPECT_EQ(TestEnumMapper.getName(TestEnum::ValueTwo), "ValueTwo");
  EXPECT_EQ(TestEnumMapper.getName(TestEnum::ValueThree), "ValueThree");
}

TEST(EnumMapperTest, testGetValue)
{
  EXPECT_EQ(TestEnumMapper.getValue("ValueOne"), TestEnum::ValueOne);
  EXPECT_EQ(TestEnumMapper.getValue("ValueTwo"), TestEnum::ValueTwo);
  EXPECT_EQ(TestEnumMapper.getValue("ValueThree"), TestEnum::ValueThree);

  EXPECT_FALSE(TestEnumMapper.getValue("valueOne"));
  EXPECT_FALSE(TestEnumMapper.getValue("NonExistent"));
}

TEST(EnumMapperTest, testGetValueCaseInsensitive)
{
  EXPECT_EQ(TestEnumMapper.getValueCaseInsensitive("ValueOne"), TestEnum::ValueOne);
  EXPECT_EQ(TestEnumMapper.getValueCaseInsensitive("valueOne"), TestEnum::ValueOne);
  EXPECT_EQ(TestEnumMapper.getValueCaseInsensitive("valueone"), TestEnum::ValueOne);
  EXPECT_EQ(TestEnumMapper.getValueCaseInsensitive("vAlUeOnE"), TestEnum::ValueOne);
  EXPECT_EQ(TestEnumMapper.getValueCaseInsensitive("ValUeTwO"), TestEnum::ValueTwo);
  EXPECT_EQ(TestEnumMapper.getValueCaseInsensitive("VALUETHREE"), TestEnum::ValueThree);

  EXPECT_FALSE(TestEnumMapper.getValueCaseInsensitive("NonExistent"));
}

TEST(EnumMapperTest, getValueFromNameOrIndex)
{
  EXPECT_EQ(TestEnumMapper.getValueFromNameOrIndex("ValueOne"), TestEnum::ValueOne);
  EXPECT_EQ(TestEnumMapper.getValueFromNameOrIndex("ValueTwo"), TestEnum::ValueTwo);
  EXPECT_EQ(TestEnumMapper.getValueFromNameOrIndex("ValueThree"), TestEnum::ValueThree);
  EXPECT_EQ(TestEnumMapper.getValueFromNameOrIndex("0"), TestEnum::ValueOne);
  EXPECT_EQ(TestEnumMapper.getValueFromNameOrIndex("1"), TestEnum::ValueTwo);
  EXPECT_EQ(TestEnumMapper.getValueFromNameOrIndex("2"), TestEnum::ValueThree);

  EXPECT_FALSE(TestEnumMapper.getValueFromNameOrIndex("valueOne"));
  EXPECT_FALSE(TestEnumMapper.getValueFromNameOrIndex("VaLuEOnE"));
  EXPECT_FALSE(TestEnumMapper.getValueFromNameOrIndex("3"));
  EXPECT_FALSE(TestEnumMapper.getValueFromNameOrIndex("129"));
  EXPECT_FALSE(TestEnumMapper.getValueFromNameOrIndex("1Something"));
  EXPECT_FALSE(TestEnumMapper.getValueFromNameOrIndex("Something1"));
  EXPECT_FALSE(TestEnumMapper.getValueFromNameOrIndex("1_"));
  EXPECT_FALSE(TestEnumMapper.getValueFromNameOrIndex(" 1"));
}

TEST(EnumMapperTest, testIndexOf)
{
  EXPECT_EQ(TestEnumMapper.indexOf(TestEnum::ValueOne), 0u);
  EXPECT_EQ(TestEnumMapper.indexOf(TestEnum::ValueTwo), 1u);
  EXPECT_EQ(TestEnumMapper.indexOf(TestEnum::ValueThree), 2u);
}

TEST(EnumMapperTest, testAt)
{
  EXPECT_EQ(TestEnumMapper.at(0), TestEnum::ValueOne);
  EXPECT_EQ(TestEnumMapper.at(1), TestEnum::ValueTwo);
  EXPECT_EQ(TestEnumMapper.at(2), TestEnum::ValueThree);

  EXPECT_FALSE(TestEnumMapper.at(3));
  EXPECT_FALSE(TestEnumMapper.at(4));
  EXPECT_FALSE(TestEnumMapper.at(8902));
}

TEST(EnumMapperTest, testGetItems)
{
  ASSERT_THAT(TestEnumMapper.getItems(),
              ElementsAre(TestEnum::ValueOne, TestEnum::ValueTwo, TestEnum::ValueThree));
}

TEST(EnumMapperTest, testGetNames)
{
  ASSERT_THAT(TestEnumMapper.getNames(), ElementsAre("ValueOne", "ValueTwo", "ValueThree"));
}

TEST(EnumMapperTest, testIterator)
{
  int index = 0;

  for (const auto &it : TestEnumMapper)
  {
    ASSERT_LT(index, 3);
    EXPECT_EQ(it.first, expectedValues.at(index));
    EXPECT_EQ(it.second, expectedNames.at(index));
    ++index;
  }
  ASSERT_EQ(index, 3);
}

TEST(EnumMapperTest, testIteratorUnpacking)
{
  int index = 0;
  for (const auto &[value, name] : TestEnumMapper)
  {
    ASSERT_LT(index, 3);
    EXPECT_EQ(value, expectedValues.at(index));
    EXPECT_EQ(name, expectedNames.at(index));
    ++index;
  }
}

} // namespace
