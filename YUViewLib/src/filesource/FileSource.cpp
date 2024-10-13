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

#include "FileSource.h"

#include <common/Formatting.h>
#include <common/Typedef.h>

#include <QDateTime>
#include <QDir>
#include <QSettings>
#include <QtGlobal>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

#define FILESOURCE_DEBUG_SIMULATESLOWLOADING 0
#if FILESOURCE_DEBUG_SIMULATESLOWLOADING && !NDEBUG
#include <QThread>
#endif

FileSource::FileSource()
{
  connect(&fileWatcher,
          &QFileSystemWatcher::fileChanged,
          this,
          &FileSource::fileSystemWatcherFileChanged);
}

bool FileSource::openFile(const std::filesystem::path &filePath)
{
  if (!std::filesystem::is_regular_file(filePath))
    return false;

  if (this->isFileOpened && this->srcFile.isOpen())
    this->srcFile.close();

  this->srcFile.setFileName(QString::fromStdString(filePath));
  this->isFileOpened = this->srcFile.open(QIODevice::ReadOnly);
  if (!this->isFileOpened)
    return false;

  this->fullFilePath = filePath;

  this->updateFileWatchSetting();
  this->fileChanged = false;

  return true;
}

#if SSE_CONVERSION
// Resize the target array if necessary and read the given number of bytes to the data array
void FileSource::readBytes(byteArrayAligned &targetBuffer, int64_t startPos, int64_t nrBytes)
{
  if (!isOk())
    return;

  if (targetBuffer.size() < nrBytes)
    targetBuffer.resize(nrBytes);

  srcFile.seek(startPos);
  srcFile.read(targetBuffer.data(), nrBytes);
}
#endif

// Resize the target array if necessary and read the given number of bytes to the data array
int64_t FileSource::readBytes(QByteArray &targetBuffer, int64_t startPos, int64_t nrBytes)
{
  if (!this->isOk())
    return 0;

  if (targetBuffer.size() < nrBytes)
    targetBuffer.resize(nrBytes);

#if FILESOURCE_DEBUG_SIMULATESLOWLOADING && !NDEBUG
  QThread::msleep(50);
#endif

  // lock the seek and read function
  QMutexLocker locker(&this->readMutex);
  this->srcFile.seek(startPos);
  return this->srcFile.read(targetBuffer.data(), nrBytes);
}

std::vector<InfoItem> FileSource::getFileInfoList() const
{
  if (!this->isFileOpened)
    return {};

  // For now we still use the QFileInfo. There is no easy cross platform formatting
  // for the std::filesystem::file_time_type. This is added in C++ 20.
  QFileInfo fileInfo(QString::fromStdString(this->fullFilePath.string()));

  std::vector<InfoItem> infoList;

  infoList.emplace_back("File Path", this->fullFilePath);
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
  const auto createdtime = this->fileInfo.created().toString("yyyy-MM-dd hh:mm:ss");
#else
  const auto createdtime = fileInfo.birthTime().toString("yyyy-MM-dd hh:mm:ss");
#endif
  infoList.emplace_back("Time Created", createdtime.toStdString());
  infoList.emplace_back("Time Modified",
                        fileInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss").toStdString());

  if (const auto size = this->getFileSize())
    infoList.emplace_back("Nr Bytes", to_string(this->getFileSize()));

  return infoList;
}

std::optional<int64_t> FileSource::getFileSize() const
{
  if (!this->isFileOpened)
    return {};

  try
  {
    const auto size = std::filesystem::file_size(this->fullFilePath);
    return static_cast<int64_t>(size);
  }
  catch (const std::filesystem::filesystem_error &e)
  {
    return {};
  }
}

std::string FileSource::getAbsoluteFilePath() const
{
  return this->isFileOpened ? this->fullFilePath.string() : "";
}

// If you are loading a playlist and you have an absolute path and a relative path, this function
// will return the absolute path (if a file with that absolute path exists) or convert the relative
// path to an absolute one and return that (if that file exists). If neither exists the empty string
// is returned.
QString FileSource::getAbsPathFromAbsAndRel(const QString &currentPath,
                                            const QString &absolutePath,
                                            const QString &relativePath)
{
  QFileInfo checkAbsoluteFile(absolutePath);
  if (checkAbsoluteFile.exists())
    return absolutePath;

  QFileInfo plFileInfo(currentPath);
  auto      combinePath = QDir(plFileInfo.path()).filePath(relativePath);
  QFileInfo checkRelativeFile(combinePath);
  if (checkRelativeFile.exists() && checkRelativeFile.isFile())
  {
    return QDir::cleanPath(combinePath);
  }

  return {};
}

bool FileSource::getAndResetFileChangedFlag()
{
  bool b            = this->fileChanged;
  this->fileChanged = false;
  return b;
}

void FileSource::updateFileWatchSetting()
{
  // Install a file watcher if file watching is active in the settings.
  // The addPath/removePath functions will do nothing if called twice for the same file.
  QSettings settings;
  if (settings.value("WatchFiles", true).toBool())
    fileWatcher.addPath(QString::fromStdString(this->fullFilePath.string()));
  else
    fileWatcher.removePath(QString::fromStdString(this->fullFilePath.string()));
}

void FileSource::clearFileCache()
{
  if (!this->isFileOpened)
    return;

#ifdef Q_OS_WIN
  // Currently, we only support this on windows. But this is only used in performance testing.
  // We will close the QFile, open it using the FILE_FLAG_NO_BUFFERING flags, close it and reopen
  // the QFile. Suggested:
  // http://stackoverflow.com/questions/478340/clear-file-cache-to-repeat-performance-testing
  QMutexLocker locker(&this->readMutex);
  this->srcFile.close();

  LPCWSTR file = this->fullFilePath.wstring().c_str();
  HANDLE  hFile =
      CreateFile(file, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);
  CloseHandle(hFile);

  this->srcFile.setFileName(this->fullFilePath);
  this->srcFile.open(QIODevice::ReadOnly);
#endif
}
