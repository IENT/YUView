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

#pragma once

#include <QByteArray>
#include <QList>
#include <QPair>
#include <QString>

namespace update
{

using DownloadFile = QPair<QString, int>;

struct FileListEntry
{
  QString filePath; //< The file name and path
  QString version;  //< The version of the file
  QString hash;     //< A hash of the file (unused so far)
  int     fileSize; //< The size of the file in bytes
};

class UpdateFileHandler
{
public:
  UpdateFileHandler();
  UpdateFileHandler(const QString &fileName, const QString &updatePath);
  UpdateFileHandler(const QByteArray &byteArray);

  // Call this on the remote file list with a reference to the local file list to get a list
  // of files that require an update (that need to be downloaded).
  QList<DownloadFile> getFilesToUpdate(UpdateFileHandler &localFiles) const;

private:
  // Parse the local file list and add all files that exist locally to the list of files
  // which potentially might require an update.
  void readFromFile(const QString &fileName);

  // Parse the remote file from the given QByteArray. Remote files will not be checked for
  // existence on the remote side. We assume that all files listed in the remote file list
  // also exist on the remote side and can be downloaded.
  void readRemoteFromData(const QByteArray &arr);

  // For every entry in the "versioninfo.txt" file, these entries present:

  // For every entry, there is the file path/name and a version string
  QList<FileListEntry> updateFileList;
  QString              updatePath{};
  bool                 loaded{false};
};

} // namespace update
