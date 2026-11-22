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

#include "VersionFileParsing.h"

#define UPDATER_DEBUG_FILE 0
#if UPDATER_DEBUG_FILE && !NDEBUG
#include <QDebug>
#include <source_location>
#define DEBUG_UPDATE_FILE(f)                                                                       \
  do                                                                                               \
  {                                                                                                \
    qDebug() << std::source_location::current().function_name() << " " << f;                       \
  } while (0)
#else
#define DEBUG_UPDATE_FILE(f) ((void)0)
#endif

namespace update
{

namespace
{

struct FileListEntry
{
  QString filePath; //< The file name and path
  QString version;  //< The version of the file
  QString hash;     //< A hash of the file (unused so far)
  int     fileSize; //< The size of the file in bytes
};

std::optional<FileListEntry> parseOneLine(const QString &line)
{
  QStringList lineSplit = line.split(" ");
  if (line.startsWith("Last Commit"))
  {
    if (line.startsWith("Last Commit: "))
      DEBUG_UPDATE_FILE("Local file last commit: " << lineSplit[2]);
    return {};
  }
  // Ignore all lines that start with %, / or #
  if (line.startsWith("%") || line.startsWith("/") || line.startsWith("#"))
    return {};

  if (lineSplit.size() != 4)
    return {};

  FileListEntry entry;

  entry.filePath = lineSplit[0];
  if (entry.filePath.endsWith(","))
    // There is a comma at the end. Remove it.
    entry.filePath.chop(1);
  entry.version = lineSplit[1];
  if (entry.version.endsWith(","))
    entry.version.chop(1);
  entry.hash = lineSplit[2];
  if (entry.hash.endsWith(","))
    entry.hash.chop(1);
  QString sizeString = lineSplit[3];
  if (sizeString.endsWith(","))
    sizeString.chop(1);

  bool ok        = true;
  entry.fileSize = sizeString.toInt(&ok);

  return ok ? std::make_optional(entry) : std::nullopt;
}

QList<FileListEntry> parseFileData(const QString fileData)
{
  QList<FileListEntry> downloadFiles;
  const QStringList    lines = fileData.split("\n");
  for (auto line : lines)
    if (const auto updateFile = parseOneLine(line))
      downloadFiles.append(*updateFile);
  return downloadFiles;
}

const auto UpdateFileHandler_FILE_NAME = "versioninfo.txt";

QList<DownloadFile> calculateFilesToUpdate(const QList<FileListEntry>  localFiles,
                                           const QList<FileListEntry> &remoteFiles)
{
  QList<DownloadFile> updateList;
  for (const auto &remoteFile : remoteFiles)
  {
    bool fileFound    = false;
    bool updateNeeded = false;
    for (auto localFile : localFiles)
    {
      if (localFile.filePath.toLower() == remoteFile.filePath.toLower())
      {
        // File found. Do we need to update it?
        updateNeeded =
          (localFile.version != remoteFile.version) || (localFile.hash != remoteFile.hash);
        fileFound = true;
        break;
      }
    }
    if (!fileFound || updateNeeded)
      updateList.append(DownloadFile(remoteFile.filePath, remoteFile.fileSize));
  }

  if (!updateList.empty())
    updateList.append(DownloadFile(UpdateFileHandler_FILE_NAME, 10000));

  return updateList;
}

} // namespace

QList<DownloadFile> parseVersionFilesAndDetermineFilesToDownload(const QString &localFileContent,
                                                                 const QString &remoteFileContent)
{
  const auto localFileList  = parseFileData(localFileContent);
  const auto remoteFileList = parseFileData(remoteFileContent);
  return calculateFilesToUpdate(localFileList, remoteFileList);
}

} // namespace update
