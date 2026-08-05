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
#include <common/Functions.h>
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

  // QFile accepts filesystem::path (wide on Windows). Never use path.string()
  // which is ACP-encoded and breaks Chinese / Unicode paths.
  this->srcFile.setFileName(filePath);
  this->isFileOpened = this->srcFile.open(QIODevice::ReadOnly);
  if (!this->isFileOpened)
    return false;

  this->fullFilePath = filePath;

  this->updateFileWatchSetting();
  this->fileChanged = false;

  return true;
}

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
  QFileInfo fileInfo(functions::fsPathToQString(this->fullFilePath));

  std::vector<InfoItem> infoList;

  infoList.emplace_back("File Path", functions::fsPathToUtf8String(this->fullFilePath));
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

std::filesystem::path FileSource::getAbsoluteFilePath() const
{
  return this->isFileOpened ? this->fullFilePath : std::filesystem::path{};
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
  const QString pathString = functions::fsPathToQString(this->fullFilePath);
  QSettings settings;
  if (settings.value("WatchFiles", true).toBool())
    fileWatcher.addPath(pathString);
  else
    fileWatcher.removePath(pathString);
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

  // Keep the wide string alive for the CreateFile call (wstring() returns a temporary).
  const std::wstring widePath = this->fullFilePath.wstring();
  HANDLE hFile =
      CreateFileW(widePath.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);
  CloseHandle(hFile);

  this->srcFile.setFileName(this->fullFilePath);
  this->srcFile.open(QIODevice::ReadOnly);
#endif
}

FileSource::~FileSource()
{
  cleanupFileHandlePool();
}

int64_t FileSource::readBytesParallel(QByteArray &targetBuffer, int64_t startPos, int64_t nrBytes)
{
  // Get thread-local file handle for lock-free parallel reading
  QFile* threadFile = getThreadLocalFileHandle();
  if (!threadFile || !threadFile->isOpen())
  {
    // Fallback to serialized read if thread-local handle unavailable
    return this->readBytes(targetBuffer, startPos, nrBytes);
  }

  // Resize buffer if needed
  if (targetBuffer.size() < nrBytes)
    targetBuffer.resize(nrBytes);

#if FILESOURCE_DEBUG_SIMULATESLOWLOADING && !NDEBUG
  QThread::msleep(50);
#endif

  // Seek and read without mutex contention - each thread has its own handle
  if (!threadFile->seek(startPos))
    return 0;

  return threadFile->read(targetBuffer.data(), nrBytes);
}

QFile* FileSource::getThreadLocalFileHandle()
{
  // Use Qt's thread handle as the key
  const Qt::HANDLE threadId = QThread::currentThreadId();

  // Fast path: check if handle already exists (with minimal lock time)
  {
    QMutexLocker lock(&poolMutex);
    auto it = fileHandlePool.find(threadId);
    if (it != fileHandlePool.end())
      return it->second.get();
  }

  // Slow path: create new handle for this thread
  auto newHandle = std::make_unique<QFile>();
  newHandle->setFileName(this->fullFilePath);
  
  if (!newHandle->open(QIODevice::ReadOnly))
    return nullptr;

  QFile* rawPtr = newHandle.get();

  // Insert into pool
  {
    QMutexLocker lock(&poolMutex);
    fileHandlePool[threadId] = std::move(newHandle);
  }

  return rawPtr;
}

void FileSource::cleanupFileHandlePool()
{
  QMutexLocker lock(&poolMutex);
  for (auto& [threadId, handle] : fileHandlePool)
  {
    if (handle && handle->isOpen())
      handle->close();
  }
  fileHandlePool.clear();
}
