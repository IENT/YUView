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

#include "updateHandlerFile.h"

#include <QFileInfo>
#include <QTextStream>

#define UPDATER_DEBUG_FILE 0
#if UPDATER_DEBUG_FILE && !NDEBUG
#include <QDebug>
#define DEBUG_UPDATE_FILE(msg) qDebug() << msg
#else
#define DEBUG_UPDATE_FILE(msg) ((void)0)
#endif

const auto UPDATEFILEHANDLER_FILE_NAME = "versioninfo.txt";

updateFileHandler::updateFileHandler()
{}

updateFileHandler::updateFileHandler(QString fileName, QString updatePath) : 
  updatePath(updatePath) 
{ 
  this->readFromFile(fileName); 
}

updateFileHandler::updateFileHandler(QByteArray &byteArray)
{ 
  this->readRemoteFromData(byteArray); 
}

void updateFileHandler::readFromFile(QString fileName)
{
  DEBUG_UPDATE_FILE("updateFileHandler::readFromFile Current working dir " << this->updatePath);

  // Open the file and get all files and their current version (int) from the file.
  QFileInfo updateFileInfo(fileName);
  if (!updateFileInfo.exists() || !updateFileInfo.isFile())
  {
    DEBUG_UPDATE_FILE("updateFileHandler::readFromFile local update file " << fileName << " not found");
    return;
  }

  // Read all lines from the file
  QFile updateFile(fileName);
  if (updateFile.open(QIODevice::ReadOnly))
  {
    QTextStream in(&updateFile);
    while (!in.atEnd())
    {
      // Convert the line into a file/version pair (they should be separated by a space)
      QString line = in.readLine();
      this->parseOneLine(line, true);
    }
    updateFile.close();
  }
  this->loaded = true;
}
  
void updateFileHandler::readRemoteFromData(QByteArray &arr)
{
  const QString reply = QString(arr);
  const QStringList lines = reply.split("\n");
  for (auto line : lines)
    this->parseOneLine(line);
}

void updateFileHandler::parseOneLine(QString &line, bool checkExistence)
{
  QStringList lineSplit = line.split(" ");
  if (line.startsWith("Last Commit"))
  {
    if (line.startsWith("Last Commit: "))
      DEBUG_UPDATE_FILE("updateFileHandler::parseOneLine Local file last commit: " << lineSplit[2]);
    return;
  }
  // Ignore all lines that start with %, / or #
  if (line.startsWith("%") || line.startsWith("/") || line.startsWith("#"))
    return;
  if (lineSplit.count() == 4)
  {
    auto entry = createFileEntry(lineSplit);
    
    if (checkExistence)
    {
      // Check if the file exists locally
      QFileInfo fInfo(this->updatePath + entry.filePath);
      if (fInfo.exists() && fInfo.isFile())
        this->updateFileList.append(entry);
      else
        // The file does not exist locally. That is strange since it is in the update info file.
        // Files that do not exist locally should always be downloaded so we don't put them into the list.
        DEBUG_UPDATE_FILE("updateFileHandler::parseOneLine The local file " << fInfo.absoluteFilePath() << " could not be found.");
    }
    else
      // Do not check if the file exists
      this->updateFileList.append(entry);
  }
}
  
QList<downloadFile> updateFileHandler::getFilesToUpdate(updateFileHandler &localFiles) const
{
  QList<downloadFile> updateList;
  for (auto remoteFile : this->updateFileList)
  {
    bool fileFound = false;
    bool updateNeeded = false;
    for(auto localFile : localFiles.updateFileList)
    {
      if (localFile.filePath.toLower() == remoteFile.filePath.toLower())
      {
        // File found. Do we need to update it?
        updateNeeded = (localFile.version != remoteFile.version) || (localFile.hash != remoteFile.hash);
        fileFound = true;
        break;
      }
    }
    if (!fileFound || updateNeeded)
      updateList.append(downloadFile(remoteFile.filePath, remoteFile.fileSize));
  }
  // No matter what, we will update the "versioninfo.txt" file (assume it to be 10kbyte)
  updateList.append(downloadFile(UPDATEFILEHANDLER_FILE_NAME, 10000));
  return updateList;
}

QString updateFileHandler::getInfo() const
{
  QString s;
  for (auto f : this->updateFileList)
  {
    s += " - " + f.filePath;
  }
  return s;
}

updateFileHandler::fileListEntry updateFileHandler::createFileEntry(QStringList &lineSplit) const
{
  fileListEntry entry;
  
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
  entry.fileSize = sizeString.toInt();
  
  return entry;
}
