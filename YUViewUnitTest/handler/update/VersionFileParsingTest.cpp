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

#include <handler/update/VersionFileParsing.h>

namespace update::test
{

const auto versionFileReference =
  QString("Last Commit: v2.16\n"
          "Last Commit Hash: b666a90f8619c1675ea3807fe14c1c7207fc8536\n"
          "Last Commit Hash Short: b666a90f\n"
          "testFile1.dll, 5.13, bca35d064ea1a1639fe3ec1683ffb8ab, 580096\n"
          "testFile2.jpg, 5.13, 6f9dec3ee7cc5ed48cfb0b803b4581c1, 9283072\n"
          "someFolder\\file3.dll, 5.13, e2e061e67fe7e6b8f74680b74d692eac, 35840\n"
          "folder\\subFolder\\file.tst, 5.13, hashStringMustNotBeHex, 229376\n");

TEST(VersionFileParsing, testIdenticalFiles_shouldReturnEmptyFilesDownloadList)
{
  EXPECT_TRUE(
    parseVersionFilesAndDetermineFilesToDownload(versionFileReference, versionFileReference)
      .empty());
}

TEST(VersionFileParsing, testDifferentCommitHash_shouldReturnEmptyFilesDownloadList)
{
  // The "Last Commit" lines are parsed but ignored.
  const auto versionFileWithDifferentAndMissingCommitHashes =
    QString("Last Commit: v3.26\n"
            "Last Commit Hash: 239495DifferentHash293884\n"
            "testFile1.dll, 5.13, bca35d064ea1a1639fe3ec1683ffb8ab, 580096\n"
            "testFile2.jpg, 5.13, 6f9dec3ee7cc5ed48cfb0b803b4581c1, 9283072\n"
            "someFolder\\file3.dll, 5.13, e2e061e67fe7e6b8f74680b74d692eac, 35840\n"
            "folder\\subFolder\\file.tst, 5.13, hashStringMustNotBeHex, 229376\n");

  EXPECT_TRUE(parseVersionFilesAndDetermineFilesToDownload(
                versionFileReference, versionFileWithDifferentAndMissingCommitHashes)
                .empty());
}

TEST(VersionFileParsing, testAdditionalComments_shouldReturnEmptyFilesDownloadList)
{
  // Comments (starting with %) should be ignored as well as non parsable lines
  const auto versionFileWithCommentsAndInvalidLines =
    QString("\%SomeComment\n"
            "Last Commit: v2.16\n"
            "Last Commit Hash: b666a90f8619c1675ea3807fe14c1c7207fc8536\n"
            "Last Commit Hash Short: b666a90f\n"
            "testFile1.dll, 5.13, bca35d064ea1a1639fe3ec1683ffb8ab, 580096\n"
            "\%SomeOtherComment\n"
            "SomeInvalidLine234\n"
            "testFile2.jpg, 5.13, 6f9dec3ee7cc5ed48cfb0b803b4581c1, 9283072\n"
            "someFolder\\file3.dll, 5.13, e2e061e67fe7e6b8f74680b74d692eac, 35840\n"
            "folder\\subFolder\\file.tst, 5.13, hashStringMustNotBeHex, 229376\n"
            "\%YetSomeOtherComment\n");

  EXPECT_TRUE(parseVersionFilesAndDetermineFilesToDownload(versionFileReference,
                                                           versionFileWithCommentsAndInvalidLines)
                .empty());
}

TEST(UpdateFileHandler, testDifferentFileVersions_shouldReturnFilesWithDifferentVersions)
{
  const auto versionFileWithDifferentAndMissingCommitHashes =
    QString("Last Commit: v3.26\n"
            "Last Commit Hash: 239495DifferentHash293884\n"
            "testFile1.dll, 5.13, bca35d064ea1a1639fe3ec1683ffb8ab, 580096\n"
            "testFile2.jpg, 6.99, 6f9dec3ee7cc5ed48cfb0b803b4581c1, 9283072\n"
            "someFolder\\file3.dll, 7.77, e2e061e67fe7e6b8f74680b74d692eac, 35840\n"
            "folder\\subFolder\\file.tst, 5.13, hashStringMustNotBeHex, 229376\n");

  const auto downloadFiles = parseVersionFilesAndDetermineFilesToDownload(
    versionFileReference, versionFileWithDifferentAndMissingCommitHashes);

  EXPECT_EQ(downloadFiles.size(), 3);
  EXPECT_EQ(downloadFiles.at(0), DownloadFile("testFile2.jpg", 9283072));
  EXPECT_EQ(downloadFiles.at(1), DownloadFile("someFolder\\file3.dll", 35840));
  EXPECT_EQ(downloadFiles.at(2), DownloadFile("versioninfo.txt", 10000));
}

TEST(VersionFileParsing, testDifferentFileHashes_shouldReturnFilesWithDifferentHash)
{
  const auto versionFileWithDifferentHash =
    QString("Last Commit: v2.16\n"
            "Last Commit Hash: b666a90f8619c1675ea3807fe14c1c7207fc8536\n"
            "Last Commit Hash Short: b666a90f\n"
            "testFile1.dll, 5.13, differentHash123, 580096\n"
            "testFile2.jpg, 5.13, alsoDifferent, 9283072\n"
            "someFolder\\file3.dll, 5.13, e2e061e67fe7e6b8f74680b74d692eac, 35840\n"
            "folder\\subFolder\\file.tst, 5.13, hashStringMustNotBeHex, 229376\n");

  const auto downloadFiles = parseVersionFilesAndDetermineFilesToDownload(
    versionFileReference, versionFileWithDifferentHash);

  EXPECT_EQ(downloadFiles.size(), 3);
  EXPECT_EQ(downloadFiles.at(0), DownloadFile("testFile1.dll", 580096));
  EXPECT_EQ(downloadFiles.at(1), DownloadFile("testFile2.jpg", 9283072));
  EXPECT_EQ(downloadFiles.at(2), DownloadFile("versioninfo.txt", 10000));
}

TEST(VersionFileParsing, testNewFileInRemote_shouldReturnFilesThatAreNew)
{
  const auto remoteVersionFileWithNewFiles =
    QString("Last Commit: v2.16\n"
            "Last Commit Hash: b666a90f8619c1675ea3807fe14c1c7207fc8536\n"
            "Last Commit Hash Short: b666a90f\n"
            "testFile1.dll, 5.13, bca35d064ea1a1639fe3ec1683ffb8ab, 580096\n"
            "testFile2.jpg, 5.13, 6f9dec3ee7cc5ed48cfb0b803b4581c1, 9283072\n"
            "someNewFile.jpg, 5.13, 17928ß028435abc87394, 283928\n"
            "someFolder\\file3.dll, 5.13, e2e061e67fe7e6b8f74680b74d692eac, 35840\n"
            "folder\\subFolder\\file.tst, 5.13, hashStringMustNotBeHex, 229376\n");

  const auto downloadFiles = parseVersionFilesAndDetermineFilesToDownload(
    versionFileReference, remoteVersionFileWithNewFiles);

  EXPECT_EQ(downloadFiles.size(), 2);
  EXPECT_EQ(downloadFiles.at(0), DownloadFile("someNewFile.jpg", 283928));
  EXPECT_EQ(downloadFiles.at(1), DownloadFile("versioninfo.txt", 10000));
}

TEST(VersionFileParsing, testFileOnlyLocal_shouldIgnoreOnlyLocalFiles)
{
  const auto remoteFileWithFewerFiles =
    QString("Last Commit: v2.16\n"
            "Last Commit Hash: b666a90f8619c1675ea3807fe14c1c7207fc8536\n"
            "Last Commit Hash Short: b666a90f\n"
            "folder\\subFolder\\file.tst, 5.13, hashStringMustNotBeHex, 229376\n");

  EXPECT_TRUE(
    parseVersionFilesAndDetermineFilesToDownload(versionFileReference, remoteFileWithFewerFiles)
      .empty());
}

TEST(VersionFileParsing, testFilesizeNotInt_shouldBeHandledAsNonParsable)
{
  const auto localFileWithSomeInvalidSizes =
    QString("Last Commit: v2.16\n"
            "Last Commit Hash: b666a90f8619c1675ea3807fe14c1c7207fc8536\n"
            "Last Commit Hash Short: b666a90f\n"
            "testFile1.dll, 5.13, bca35d064ea1a1639fe3ec1683ffb8ab, invalidSize\n"
            "testFile2.jpg, 5.13, 6f9dec3ee7cc5ed48cfb0b803b4581c1, 9283Invalid072\n"
            "someFolder\\file3.dll, 5.13, e2e061e67fe7e6b8f74680b74d692eac, 35840\n"
            "folder\\subFolder\\file.tst, 5.13, hashStringMustNotBeHex, 229376\n");

  const auto downloadFiles = parseVersionFilesAndDetermineFilesToDownload(
    localFileWithSomeInvalidSizes, versionFileReference);

  EXPECT_EQ(downloadFiles.size(), 3);
  EXPECT_EQ(downloadFiles.at(0), DownloadFile("testFile1.dll", 580096));
  EXPECT_EQ(downloadFiles.at(1), DownloadFile("testFile2.jpg", 9283072));
  EXPECT_EQ(downloadFiles.at(2), DownloadFile("versioninfo.txt", 10000));
}

} // namespace update::test
