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

#include <common/Modified.h>

TEST(ModifiedTest, TestInitialization_ShouldBeUnmodified)
{
  modified<int> someValue = 22;
  EXPECT_EQ(someValue, 22);
  EXPECT_FALSE(someValue.wasModified());

  modified<int> somOtherValue{44};
  EXPECT_EQ(somOtherValue, 44);
  EXPECT_FALSE(somOtherValue.wasModified());
}

TEST(ModifiedTest, TestModificationWithSameValue_ShouldReportUnmodified)
{
  modified<int> someValue = 22;
  someValue               = 22;
  EXPECT_FALSE(someValue.wasModified());
}

TEST(ModifiedTest, TestModificationWithNewValue_ShouldReportModification)
{
  modified<int> someValue = 22;
  someValue               = 23;
  EXPECT_TRUE(someValue.wasModified());
}

TEST(ModifiedTest, TestModificationWithNewValueAndThenWithInitialValue_ShouldReportModification)
{
  modified<int> someValue = 22;
  someValue               = 23;
  EXPECT_TRUE(someValue.wasModified());

  someValue = 22;
  EXPECT_TRUE(someValue.wasModified());
}

TEST(ModifiedTest, TestValueRetrieval)
{
  modified<int> someValue = 22;
  EXPECT_EQ(*someValue, 22);
  EXPECT_EQ(someValue.value(), 22);

  someValue = 33;
  EXPECT_EQ(*someValue, 33);
  EXPECT_EQ(someValue.value(), 33);
}

TEST(ModifiedTest, TestComparisonOperator_ShouldCompareToTDirectly)
{
  modified<int> someValue = 22;
  EXPECT_TRUE(someValue == 22);
  EXPECT_EQ(someValue, 22);

  someValue = 33;
  EXPECT_TRUE(someValue == 33);
  EXPECT_EQ(someValue, 33);
}

TEST(ModifiedTest, TestComparisonOfModifiedAndNotModifiedValues_ShouldOnlyCompareValues)
{
  modified<int> unmodifiedValue = 22;
  modified<int> modifiedValue   = 77;
  modifiedValue                 = 22;

  EXPECT_FALSE(unmodifiedValue.wasModified());
  EXPECT_TRUE(modifiedValue.wasModified());
  EXPECT_EQ(unmodifiedValue, modifiedValue);
}

TEST(ModifiedTest, TestSetUnmodified)
{
  modified<int> someValue = 22;

  someValue = 33;
  EXPECT_TRUE(someValue.wasModified());

  someValue.setUnmodified();
  EXPECT_FALSE(someValue.wasModified());
}

TEST(ModiifedTest, TestImplicitConversionToT)
{
  modified<int> someValue = 22;
  const auto    result    = 44 + someValue;
  EXPECT_EQ(result, 66);

  modified<bool> someFlag{true};
  EXPECT_TRUE(someFlag);
  EXPECT_FALSE(!someFlag);
}
