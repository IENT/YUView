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

#include <QDateTime>
#include <QDir>
#include <QRegularExpression>
#include <QSettings>
#include <QtGlobal>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "common/typedef.h"

#define FILESOURCE_DEBUG_SIMULATESLOWLOADING 0
#if FILESOURCE_DEBUG_SIMULATESLOWLOADING && !NDEBUG
#include <QThread>
#endif

FileSource::FileSource()
{
  fileChanged  = false;
  isFileOpened = false;

  connect(&fileWatcher,
          &QFileSystemWatcher::fileChanged,
          this,
          &FileSource::fileSystemWatcherFileChanged);
}

bool FileSource::openFile(const QString &filePath)
{
  // Check if the file exists
  fileInfo.setFile(filePath);
  if (!fileInfo.exists() || !fileInfo.isFile())
    return false;

  if (isFileOpened && srcFile.isOpen())
    srcFile.close();

  // open file for reading
  srcFile.setFileName(filePath);
  isFileOpened = srcFile.open(QIODevice::ReadOnly);
  if (!isFileOpened)
    return false;

  // Save the full file path
  fullFilePath = filePath;

  // Install a watcher for the file (if file watching is active)
  updateFileWatchSetting();
  fileChanged = false;

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
  if (!isOk())
    return 0;

  if (targetBuffer.size() < nrBytes)
    targetBuffer.resize(nrBytes);

#if FILESOURCE_DEBUG_SIMULATESLOWLOADING && !NDEBUG
  QThread::msleep(50);
#endif

  // lock the seek and read function
  QMutexLocker locker(&readMutex);
  srcFile.seek(startPos);
  return srcFile.read(targetBuffer.data(), nrBytes);
}

QList<infoItem> FileSource::getFileInfoList() const
{
  QList<infoItem> infoList;

  if (!isFileOpened)
    return infoList;

  // The full file path
  infoList.append(infoItem("File Path", fileInfo.absoluteFilePath()));

  // The file creation time
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
  QString createdtime = fileInfo.created().toString("yyyy-MM-dd hh:mm:ss");
#else
  QString createdtime = fileInfo.birthTime().toString("yyyy-MM-dd hh:mm:ss");
#endif
  infoList.append(infoItem("Time Created", createdtime));

  // The last modification time
  QString modifiedtime = fileInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss");
  infoList.append(infoItem("Time Modified", modifiedtime));

  // The file size in bytes
  QString fileSize = QString("%1").arg(fileInfo.size());
  infoList.append(infoItem("Nr Bytes", fileSize));

  return infoList;
}

FileSource::fileFormat_t FileSource::formatFromFilename(QFileInfo fileInfo)
{
  FileSource::fileFormat_t format;

  // We are going to check two strings (one after the other) for indicators on the frames size, fps
  // and bit depth. 1: The file name, 2: The folder name that the file is contained in.

  auto fileName = fileInfo.fileName();
  if (fileName.isEmpty())
    return format;

  auto dirName = fileInfo.absoluteDir().dirName();

  for (auto const &name : {fileName, dirName})
  {
    // First, we will try to get a frame size from the name
    if (!format.frameSize.isValid())
    {
      // The regular expressions to match. They are sorted from most detailed to least so that the
      // most detailed ones are tested first.
      auto regExprList =
          QStringList()
          << "([0-9]+)(?:x|X|\\*)([0-9]+)_([0-9]+)(?:Hz)?_([0-9]+)b?[\\._]" // Something_2160x1440_60_8_more.yuv
                                                                            // or
                                                                            // Something_2160x1440_60_8b.yuv
                                                                            // or
                                                                            // Something_2160x1440_60Hz_8_more.yuv
          << "([0-9]+)(?:x|X|\\*)([0-9]+)_([0-9]+)(?:Hz)?[\\._]" // Something_2160x1440_60_more.yuv
                                                                 // or Something_2160x1440_60.yuv
          << "([0-9]+)(?:x|X|\\*)([0-9]+)[\\._]";                // Something_2160x1440_more.yuv or
                                                                 // Something_2160x1440.yuv

      for (QString regExpStr : regExprList)
      {
        QRegularExpression exp(regExpStr);
        auto               match = exp.match(name);
        if (match.hasMatch())
        {
          auto widthString  = match.captured(1);
          auto heightString = match.captured(2);
          format.frameSize  = Size(widthString.toInt(), heightString.toInt());

          auto rateString = match.captured(3);
          if (!rateString.isEmpty())
            format.frameRate = rateString.toDouble();

          auto bitDepthString = match.captured(4);
          if (!bitDepthString.isEmpty())
            format.bitDepth = bitDepthString.toUInt();

          break; // Don't check the following expressions
        }
      }
    }

    // try resolution indicators with framerate: "1080p50", "720p24" ...
    if (!format.frameSize.isValid())
    {
      QRegularExpression rx1080p("1080p([0-9]+)");
      auto               matchIt = rx1080p.globalMatch(name);
      if (matchIt.hasNext())
      {
        auto match           = matchIt.next();
        format.frameSize     = Size(1920, 1080);
        auto frameRateString = match.captured(1);
        format.frameRate     = frameRateString.toInt();
      }
    }
    if (!format.frameSize.isValid())
    {
      QRegularExpression rx720p("720p([0-9]+)");
      auto               matchIt = rx720p.globalMatch(name);
      if (matchIt.hasNext())
      {
        auto match           = matchIt.next();
        format.frameSize     = Size(1280, 720);
        auto frameRateString = match.captured(1);
        format.frameRate     = frameRateString.toInt();
      }
    }

    if (!format.frameSize.isValid())
    {
      // try to find resolution indicators (e.g. 'cif', 'hd') in file name
      if (name.contains("_cif", Qt::CaseInsensitive))
        format.frameSize = Size(352, 288);
      else if (name.contains("_qcif", Qt::CaseInsensitive))
        format.frameSize = Size(176, 144);
      else if (name.contains("_4cif", Qt::CaseInsensitive))
        format.frameSize = Size(704, 576);
      else if (name.contains("UHD", Qt::CaseSensitive))
        format.frameSize = Size(3840, 2160);
      else if (name.contains("HD", Qt::CaseSensitive))
        format.frameSize = Size(1920, 1080);
      else if (name.contains("1080p", Qt::CaseSensitive))
        format.frameSize = Size(1920, 1080);
      else if (name.contains("720p", Qt::CaseSensitive))
        format.frameSize = Size(1280, 720);
    }

    // Second, if we were able to get a frame size but no frame rate, we will try to get a frame
    // rate.
    if (format.frameSize.isValid() && format.frameRate == -1)
    {
      // Look for: 24fps, 50fps, 24FPS, 50FPS
      QRegularExpression rxFPS("([0-9]+)fps", QRegularExpression::CaseInsensitiveOption);
      auto               match = rxFPS.match(name);
      if (match.hasMatch())
      {
        auto frameRateString = match.captured(1);
        format.frameRate     = frameRateString.toInt();
      }
    }
    if (format.frameSize.isValid() && format.frameRate == -1)
    {
      // Look for: 24Hz, 50Hz, 24HZ, 50HZ
      QRegularExpression rxHZ("([0-9]+)HZ", QRegularExpression::CaseInsensitiveOption);
      auto               match = rxHZ.match(name);
      if (match.hasMatch())
      {
        QString frameRateString = match.captured(1);
        format.frameRate        = frameRateString.toInt();
      }
    }

    // Third, if we were able to get a frame size but no bit depth, we try to get a bit depth.
    if (format.frameSize.isValid() && format.bitDepth == 0)
    {
      for (unsigned bd : {8, 9, 10, 12, 16})
      {
        // Look for: 10bit, 10BIT, 10-bit, 10-BIT
        if (name.contains(QString("%1bit").arg(bd), Qt::CaseInsensitive) ||
            name.contains(QString("%1-bit").arg(bd), Qt::CaseInsensitive))
        {
          format.bitDepth = bd;
          break;
        }
        // Look for bit depths like: _16b_ .8b. -12b-
        QRegularExpression exp(QString("(?:_|\\.|-)%1b(?:_|\\.|-)").arg(bd));
        auto               match = exp.match(name);
        if (match.hasMatch())
        {
          format.bitDepth = bd;
          break;
        }
      }
    }

    // If we were able to get a frame size, try to get an indicator for packed formats
    if (format.frameSize.isValid())
    {
      QRegularExpression exp("(?:_|\\.|-)packed(?:_|\\.|-)");
      auto               match = exp.match(name);
      if (match.hasMatch())
        format.packed = true;
    }
  }

  return format;
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

void FileSource::updateFileWatchSetting()
{
  // Install a file watcher if file watching is active in the settings.
  // The addPath/removePath functions will do nothing if called twice for the same file.
  QSettings settings;
  if (settings.value("WatchFiles", true).toBool())
    fileWatcher.addPath(fullFilePath);
  else
    fileWatcher.removePath(fullFilePath);
}

void FileSource::clearFileCache()
{
  if (!isFileOpened)
    return;

#ifdef Q_OS_WIN
  // We will close the QFile, open it using the FILE_FLAG_NO_BUFFERING flags, close it and reopen
  // the QFile. Suggested:
  // http://stackoverflow.com/questions/478340/clear-file-cache-to-repeat-performance-testing
  QMutexLocker locker(&readMutex);
  srcFile.close();

  LPCWSTR file = (const wchar_t *)fullFilePath.utf16();
  HANDLE  hFile =
      CreateFile(file, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);
  CloseHandle(hFile);

  srcFile.setFileName(fullFilePath);
  srcFile.open(QIODevice::ReadOnly);
#endif
}
