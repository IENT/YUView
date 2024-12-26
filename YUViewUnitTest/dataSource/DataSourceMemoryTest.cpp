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

#include "DataSourceMemory.h"

namespace datasource::test
{

TEST(DataSourceMemoryTest, TestCreation)
{
  DataSourceMemory dataSource({'t', 'e', 's', 't', 'd', 'a', 't', 'a'});

  EXPECT_TRUE(dataSource.getInfoList().empty());
  EXPECT_FALSE(dataSource.atEnd());
  EXPECT_TRUE(dataSource.isOk());
  EXPECT_EQ(dataSource.getPosition(), 0);
  EXPECT_EQ(dataSource.getSize(), 8);
}

TEST(DataSourceMemoryTest, readingWithInvalidValue_ShouldThrow)
{
  DataSourceMemory dataSource({'t', 'e', 's', 't', 'd', 'a', 't', 'a'});
  ByteVector       buffer;
  EXPECT_THROW(dataSource.read(buffer, 0), std::invalid_argument);
  EXPECT_THROW(dataSource.read(buffer, -1), std::invalid_argument);
  EXPECT_THROW(dataSource.read(buffer, -999), std::invalid_argument);
}

TEST(DataSourceMemoryTest, seekWithInvalidValue_ShouldThrow)
{
  DataSourceMemory dataSource({'t', 'e', 's', 't', 'd', 'a', 't', 'a'});
  EXPECT_THROW(dataSource.seek(-1), std::invalid_argument);
  EXPECT_THROW(dataSource.seek(-999), std::invalid_argument);
}

TEST(DataSourceMemoryTest, readDataUntilEnd_shouldReturnCorrectData)
{
  DataSourceMemory dataSource({'t', 'e', 's', 't', 'd', 'a', 't', 'a'});
  ByteVector       buffer;
  EXPECT_EQ(dataSource.read(buffer, 3), 3);
  EXPECT_THAT(buffer, ElementsAre('t', 'e', 's'));
  EXPECT_FALSE(dataSource.atEnd());
  EXPECT_EQ(dataSource.getPosition(), 3);

  EXPECT_EQ(dataSource.read(buffer, 3), 3);
  EXPECT_THAT(buffer, ElementsAre('t', 'd', 'a'));
  EXPECT_FALSE(dataSource.atEnd());
  EXPECT_EQ(dataSource.getPosition(), 6);

  EXPECT_EQ(dataSource.read(buffer, 3), 2);
  EXPECT_THAT(buffer, ElementsAre('t', 'a'));
  EXPECT_TRUE(dataSource.atEnd());
  EXPECT_EQ(dataSource.getPosition(), 8);
}

TEST(DataSourceMemoryTest, readingPastEnd_shouldReturnNothing)
{
  DataSourceMemory dataSource({'t', 'e', 's', 't', 'd', 'a', 't', 'a'});
  ByteVector       buffer;
  EXPECT_EQ(dataSource.read(buffer, 8), 8);
  EXPECT_THAT(buffer, ElementsAre('t', 'e', 's', 't', 'd', 'a', 't', 'a'));
  EXPECT_TRUE(dataSource.atEnd());
  EXPECT_EQ(dataSource.getPosition(), 8);

  EXPECT_EQ(dataSource.read(buffer, 3), 0);
  EXPECT_TRUE(buffer.empty());
  EXPECT_TRUE(dataSource.atEnd());
  EXPECT_EQ(dataSource.getPosition(), 8);
}

TEST(DataSourceMemoryTest, seekFromEnd_ShouldReturnCorrectData)
{
  DataSourceMemory dataSource({'t', 'e', 's', 't', 'd', 'a', 't', 'a'});
  ByteVector       buffer;
  EXPECT_EQ(dataSource.read(buffer, 8), 8);
  EXPECT_THAT(buffer, ElementsAre('t', 'e', 's', 't', 'd', 'a', 't', 'a'));
  EXPECT_TRUE(dataSource.atEnd());
  EXPECT_EQ(dataSource.getPosition(), 8);

  EXPECT_TRUE(dataSource.seek(4));
  EXPECT_EQ(dataSource.getPosition(), 4);

  EXPECT_EQ(dataSource.read(buffer, 3), 3);
  EXPECT_THAT(buffer, ElementsAre('d', 'a', 't'));
  EXPECT_FALSE(dataSource.atEnd());
  EXPECT_EQ(dataSource.getPosition(), 7);
}

} // namespace datasource::test