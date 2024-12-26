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

#include <TemporaryFile.h>
#include <dataSource/DataSourceLineReader.h>

using testing::Return;

namespace datasource::test
{

namespace
{

class DataSourceLineReaderSmallBuffer : public DataSourceLineReader
{
public:
  DataSourceLineReaderSmallBuffer(IDataSource *const dataSource) : DataSourceLineReader(dataSource)
  {
    this->bufferSize = 4;
  }
};

} // namespace

TEST(DataSourceLineReaderTest, CreateFromNullPointer_ShouldThrow)
{
  EXPECT_THROW(DataSourceLineReader(nullptr), std::runtime_error);
}

TEST(DataSourceLineReaderTest, ReadLinesFromFile_ShouldReadLinesCorrectly)
{
  std::string      multiLineText = "Line 1\nLine 2\nLine 3";
  DataSourceMemory dataSource(ByteVector(multiLineText.begin(), multiLineText.end()));

  DataSourceLineReader reader(&dataSource);
  EXPECT_EQ(reader.readLine(), "Line 1");
  EXPECT_EQ(reader.readLine(), "Line 2");
  EXPECT_EQ(reader.readLine(), "Line 3");
  EXPECT_FALSE(reader.readLine());
}

TEST(DataSourceLineReaderTest, ReadLinesFromFile_NewLineAtEnd_ShouldReadEmptyLineAtEnd)
{
  std::string      multiLineText = "Line 1\nLine 2\n";
  DataSourceMemory dataSource(ByteVector(multiLineText.begin(), multiLineText.end()));

  DataSourceLineReader reader(&dataSource);
  EXPECT_EQ(reader.readLine(), "Line 1");
  EXPECT_EQ(reader.readLine(), "Line 2");
  EXPECT_EQ(reader.readLine(), "");
  EXPECT_FALSE(reader.readLine());
}

TEST(DataSourceLineReaderTest, ReadLinesAcrossBufferBoundaries_ShouldReadLinesCorrectly)
{
  std::string      multiLineText = "Line 1\nLine 2\nLine 3";
  DataSourceMemory dataSource(ByteVector(multiLineText.begin(), multiLineText.end()));

  DataSourceLineReaderSmallBuffer reader(&dataSource);
  EXPECT_EQ(reader.readLine(), "Line 1");
  EXPECT_EQ(reader.readLine(), "Line 2");
  EXPECT_EQ(reader.readLine(), "Line 3");
  EXPECT_FALSE(reader.readLine());
}

} // namespace datasource::test
