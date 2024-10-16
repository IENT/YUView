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

#include <TemporaryFile.h>
#include <filesource/DataSourceLocalFile.h>

namespace
{

using namespace std::literals;

const ByteVector DUMMY_DATA = {'t', 'e', 's', 't', 'd', 'a', 't', 'a'};

TEST(DataSourceLocalFileTest, OpenFileThatDoesNotExist)
{
  filesource::DataSourceLocalFile file("/path/to/file/that/does/not/exist");
  EXPECT_FALSE(file.isOk());
  EXPECT_FALSE(file);
  EXPECT_TRUE(file.filePath().empty());
  EXPECT_EQ(file.getInfoList().size(), 0u);
  EXPECT_FALSE(file.atEnd());
  EXPECT_EQ(file.position(), 0);
  EXPECT_FALSE(file.fileSize().has_value());
  EXPECT_FALSE(file.seek(252));

  ByteVector dummyVector;
  EXPECT_EQ(file.read(dummyVector, 378), 0);
  EXPECT_EQ(dummyVector.size(), 0);
}

TEST(DataSourceLocalFileTest, OpenFileThatExists_TestRetrievalOfFileInfo)
{
  yuviewTest::TemporaryFile tempFile(DUMMY_DATA);

  filesource::DataSourceLocalFile file(tempFile.getFilePath());
  EXPECT_TRUE(file);

  EXPECT_EQ(file.fileSize().value(), 8);

  const auto debugTest = file.getInfoList();
  EXPECT_THAT(file.getInfoList(),
              ElementsAre(InfoItem("File Path",
                                   tempFile.getFilePath().string(),
                                   "The absolute path of the local file"),
                          InfoItem("File Size"sv, "8"sv)));
}

TEST(DataSourceLocalFileTest, OpenFileThatExists_TestReadingOfData)
{
  yuviewTest::TemporaryFile tempFile(DUMMY_DATA);

  filesource::DataSourceLocalFile file(tempFile.getFilePath());
  EXPECT_TRUE(file.isOk());
  EXPECT_TRUE(file);

  EXPECT_EQ(file.filePath(), tempFile.getFilePath());
  EXPECT_FALSE(file.atEnd());
  EXPECT_EQ(file.position(), 0);

  ByteVector buffer;
  EXPECT_EQ(file.read(buffer, 5), 5);
  EXPECT_TRUE(file);
  EXPECT_THAT(buffer, ElementsAre('t', 'e', 's', 't', 'd'));
  EXPECT_EQ(file.position(), 5);

  EXPECT_EQ(file.read(buffer, 36282), 3);
  EXPECT_TRUE(file);
  EXPECT_THAT(buffer, ElementsAre('a', 't', 'a'));
  EXPECT_EQ(file.position(), 8);
  EXPECT_TRUE(file.atEnd());
}

TEST(DataSourceLocalFileTest, OpenFileThatExists_TestSeekingBeforeReading)
{
  yuviewTest::TemporaryFile tempFile(DUMMY_DATA);

  filesource::DataSourceLocalFile file(tempFile.getFilePath());
  EXPECT_TRUE(file);

  EXPECT_TRUE(file.seek(4));
  EXPECT_TRUE(file);
  EXPECT_EQ(file.position(), 4);
  EXPECT_FALSE(file.atEnd());

  ByteVector buffer;
  EXPECT_EQ(file.read(buffer, 100), 4);
  EXPECT_TRUE(file);
  EXPECT_THAT(buffer, ElementsAre('d', 'a', 't', 'a'));
  EXPECT_EQ(file.position(), 8);
  EXPECT_TRUE(file.atEnd());
}

TEST(DataSourceLocalFileTest, OpenFileThatExists_TestSeekingFromEOF)
{
  yuviewTest::TemporaryFile tempFile(DUMMY_DATA);
  ByteVector                buffer;

  filesource::DataSourceLocalFile file(tempFile.getFilePath());

  EXPECT_TRUE(file);
  EXPECT_EQ(file.read(buffer, 100), 8);
  EXPECT_THAT(buffer, ElementsAre('t', 'e', 's', 't', 'd', 'a', 't', 'a'));
  EXPECT_TRUE(file.atEnd());
  EXPECT_EQ(file.position(), 8);
  EXPECT_TRUE(file);

  EXPECT_TRUE(file.seek(2));
  EXPECT_TRUE(file);
  EXPECT_EQ(file.position(), 2);
  EXPECT_FALSE(file.atEnd());

  EXPECT_EQ(file.read(buffer, 3), 3);
  EXPECT_TRUE(file);
  EXPECT_THAT(buffer, ElementsAre('s', 't', 'd'));
  EXPECT_EQ(file.position(), 5);
  EXPECT_FALSE(file.atEnd());
}

} // namespace
