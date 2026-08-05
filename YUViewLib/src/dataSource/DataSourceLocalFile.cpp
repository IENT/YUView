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

#include "DataSourceLocalFile.h"

#include <common/Functions.h>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace datasource
{

namespace
{

std::optional<std::filesystem::file_time_type>
getLastWriteTime(const std::filesystem::path &filePath) noexcept
{
  try
  {
    return {std::filesystem::last_write_time(filePath)};
  }
  catch (const std::exception &e)
  {
    return {};
  }
}

} // namespace

DataSourceLocalFile::DataSourceLocalFile(const std::filesystem::path &filePath)
{
  this->filePath = filePath;
  // Open via filesystem::path so Windows uses the wide API for Unicode paths.
  this->file.open(this->filePath, std::ios_base::in | std::ios_base::binary);
  if (this->isOk())
    this->lastWriteTime = getLastWriteTime(this->filePath);
}

DataSourceLocalFile::~DataSourceLocalFile()
{
  cleanupFileHandlePool();
}

std::vector<InfoItem> DataSourceLocalFile::getInfoList() const
{
  if (!this->isOk())
    return {};

  std::vector<InfoItem> infoList;
  infoList.push_back(InfoItem({"File Path",
                               functions::fsPathToUtf8String(this->filePath),
                               "The absolute path of the local file"}));
  if (const auto size = this->getFileSize())
    infoList.push_back(InfoItem({"File Size", std::to_string(*size)}));

  return infoList;
}

bool DataSourceLocalFile::atEnd() const
{
  return this->file.eof();
}

bool DataSourceLocalFile::isOk() const
{
  if (this->file.fail())
    return this->file.eof();
  return true;
}

std::int64_t DataSourceLocalFile::getPosition() const
{
  return this->filePosition;
}

void DataSourceLocalFile::clearFileCache()
{
  if (!this->isOk())
    return;

#ifdef Q_OS_WIN
  // Currently, we only support this on windows. But this is only used in performance testing.
  // We will close the QFile, open it using the FILE_FLAG_NO_BUFFERING flags, close it and reopen
  // the QFile. Suggested:
  // http://stackoverflow.com/questions/478340/clear-file-cache-to-repeat-performance-testing
  const std::lock_guard<std::mutex> readLock(this->readingMutex);
  this->file.close();

  const std::wstring widePath = this->filePath.wstring();
  HANDLE hFile =
      CreateFileW(widePath.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);
  CloseHandle(hFile);

  this->file.open(this->filePath, std::ios_base::in | std::ios_base::binary);
#endif
}

bool DataSourceLocalFile::wasSourceModified() const
{
  if (!this->isOk())
    return false;

  if (const auto newTime = getLastWriteTime(this->filePath))
    return *newTime > *this->lastWriteTime;

  return false;
}

void DataSourceLocalFile::reloadAndResetDataSource()
{
  this->file.close();
  this->file.open(this->filePath, std::ios_base::in | std::ios_base::binary);
  if (this->isOk())
    this->lastWriteTime = getLastWriteTime(this->filePath);
}

bool DataSourceLocalFile::seek(const std::int64_t pos)
{
  if (!this->isOk())
    return false;

  this->file.clear();
  this->file.seekg(static_cast<std::streampos>(pos));
  this->filePosition = this->file.tellg();

  return this->isOk();
}

std::int64_t DataSourceLocalFile::read(ByteVector &buffer, const std::int64_t nrBytes)
{
  if (!this->isOk())
    return 0;

  const std::lock_guard<std::mutex> readLock(this->readingMutex);

  const auto size = static_cast<size_t>(nrBytes);
  if (static_cast<std::int64_t>(buffer.size()) < nrBytes)
    buffer.resize(size);

  this->file.read(reinterpret_cast<char *>(buffer.data()), size);

  const auto bytesRead = this->file.gcount();
  buffer.resize(bytesRead);

  this->filePosition += bytesRead;
  return static_cast<std::int64_t>(bytesRead);
}

std::optional<std::int64_t> DataSourceLocalFile::getFileSize() const
{
  if (!this->isOk())
    return {};

  const auto size = std::filesystem::file_size(this->filePath);
  return static_cast<std::int64_t>(size);
}

[[nodiscard]] std::filesystem::path DataSourceLocalFile::getFilePath() const
{
  return this->filePath;
}

std::int64_t DataSourceLocalFile::readAt(ByteVector &buffer, 
                                         const std::int64_t position, 
                                         const std::int64_t nrBytes)
{
  // Get thread-local file handle for lock-free parallel reading
  std::ifstream* threadFile = getThreadLocalFileHandle();
  if (!threadFile || !threadFile->is_open())
  {
    // Fallback to serialized read if thread-local handle unavailable
    const std::lock_guard<std::mutex> readLock(this->readingMutex);
    if (!this->seek(position))
      return 0;
    return this->read(buffer, nrBytes);
  }

  // Seek to position (no mutex needed - each thread has its own handle)
  threadFile->clear();
  threadFile->seekg(static_cast<std::streampos>(position));
  if (threadFile->fail())
    return 0;

  // Resize buffer if needed
  const auto size = static_cast<size_t>(nrBytes);
  if (static_cast<std::int64_t>(buffer.size()) < nrBytes)
    buffer.resize(size);

  // Read data directly without mutex contention
  threadFile->read(reinterpret_cast<char *>(buffer.data()), size);
  const auto bytesRead = threadFile->gcount();
  buffer.resize(bytesRead);

  return static_cast<std::int64_t>(bytesRead);
}

std::ifstream* DataSourceLocalFile::getThreadLocalFileHandle()
{
  const auto threadId = std::this_thread::get_id();
  
  // Fast path: check if handle already exists (with minimal lock time)
  {
    std::lock_guard<std::mutex> lock(poolMutex);
    auto it = fileHandlePool.find(threadId);
    if (it != fileHandlePool.end())
      return it->second.get();
  }

  // Slow path: create new handle for this thread
  auto newHandle = std::make_unique<std::ifstream>();
  newHandle->open(this->filePath, std::ios_base::in | std::ios_base::binary);
  
  if (!newHandle->is_open())
    return nullptr;

  std::ifstream* rawPtr = newHandle.get();
  
  // Insert into pool
  {
    std::lock_guard<std::mutex> lock(poolMutex);
    fileHandlePool[threadId] = std::move(newHandle);
  }
  
  return rawPtr;
}

void DataSourceLocalFile::cleanupFileHandlePool()
{
  std::lock_guard<std::mutex> lock(poolMutex);
  for (auto& [threadId, handle] : fileHandlePool)
  {
    if (handle && handle->is_open())
      handle->close();
  }
  fileHandlePool.clear();
}

} // namespace datasource
