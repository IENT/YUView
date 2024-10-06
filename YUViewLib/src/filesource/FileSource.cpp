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

#include <common/Typedef.h>

#include <QFileInfo>
#include <QMutexLocker>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#define FILESOURCE_DEBUG_SIMULATESLOWLOADING 0
#if FILESOURCE_DEBUG_SIMULATESLOWLOADING && !NDEBUG
#include <QThread>
#endif

FileSource::FileSource() : FileSourceWithLocalFile()
{
}

bool FileSource::openFile(const QString &filePath)
{
  QFileInfo fileInfo(filePath);
  if (!fileInfo.exists() || !fileInfo.isFile())
    return false;

  if (this->srcFile.isOpen())
    this->srcFile.close();

  this->srcFile.setFileName(filePath);
  this->isFileOpened = this->srcFile.open(QIODevice::ReadOnly);
  if (!this->isFileOpened)
    return false;

  this->fullFilePath = filePath;

  this->updateFileWatchSetting();

  return true;
}

bool FileSource::atEnd() const
{
  return !this->isFileOpened ? true : this->srcFile.atEnd();
}

QByteArray FileSource::readLine()
{
  return !this->isFileOpened ? QByteArray() : this->srcFile.readLine();
}

bool FileSource::seek(int64_t pos)
{
  return !this->isFileOpened ? false : this->srcFile.seek(pos);
}

int64_t FileSource::pos()
{
  return !this->isFileOpened ? 0 : this->srcFile.pos();
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

int64_t FileSource::readBytes(QByteArray &targetBuffer, int64_t startPos, int64_t nrBytes)
{
  if (!this->isOpened())
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

  LPCWSTR file = (const wchar_t *)this->fullFilePath.utf16();
  HANDLE  hFile =
      CreateFile(file, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);
  CloseHandle(hFile);

  this->srcFile.setFileName(this->fullFilePath);
  this->srcFile.open(QIODevice::ReadOnly);
#endif
}
