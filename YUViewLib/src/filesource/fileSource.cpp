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

#include "fileSource.h"

#include <QDateTime>
#include <QDir>
#include <QRegExp>
#include <QSettings>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "common/typedef.h"
 
#define FILESOURCE_DEBUG_SIMULATESLOWLOADING 0
#if FILESOURCE_DEBUG_SIMULATESLOWLOADING && !NDEBUG
#include <QThread>
#endif

fileSource::fileSource()
{
  fileChanged = false;
  isFileOpened = false;

  connect(&fileWatcher, &QFileSystemWatcher::fileChanged, this, &fileSource::fileSystemWatcherFileChanged);
}

bool fileSource::openFile(const QString &filePath)
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
void fileSource::readBytes(byteArrayAligned &targetBuffer, int64_t startPos, int64_t nrBytes)
{
  if(!isOk())
    return;

  if (targetBuffer.size() < nrBytes)
    targetBuffer.resize(nrBytes);

  srcFile.seek(startPos);
  srcFile.read(targetBuffer.data(), nrBytes);
}
#endif

// Resize the target array if necessary and read the given number of bytes to the data array
int64_t fileSource::readBytes(QByteArray &targetBuffer, int64_t startPos, int64_t nrBytes)
{
  if(!isOk())
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

QList<infoItem> fileSource::getFileInfoList() const
{
  QList<infoItem> infoList;

  if (!isFileOpened)
    return infoList;

  // The full file path
  infoList.append(infoItem("File Path", fileInfo.absoluteFilePath()));

  // The file creation time
  QString createdtime = fileInfo.created().toString("yyyy-MM-dd hh:mm:ss");
  infoList.append(infoItem("Time Created", createdtime));

  // The last modification time
  QString modifiedtime = fileInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss");
  infoList.append(infoItem("Time Modified", modifiedtime));

  // The file size in bytes
  QString fileSize = QString("%1").arg(fileInfo.size());
  infoList.append(infoItem("Nr Bytes", fileSize));

  return infoList;
}

fileSource::fileFormat_t fileSource::formatFromFilename(QFileInfo fileInfo)
{
  fileSource::fileFormat_t format;

  // We are going to check two strings (one after the other) for indicators on the frames size, fps and bit depth.
  // 1: The file name, 2: The folder name that the file is contained in.
  QStringList checkStrings;

  // The full name of the file
  QString fileName = fileInfo.fileName();
  if (fileName.isEmpty())
    return format;
  checkStrings.append(fileName);

  // The name of the folder that the file is in
  QString dirName = fileInfo.absoluteDir().dirName();
  checkStrings.append(dirName);

  for (auto const &name : checkStrings)
  {
    // First, we will try to get a frame size from the name
    if (!format.frameSize.isValid())
    {
      // The regular expressions to match. They are sorted from most detailed to least so that the most
      // detailed ones are tested first.
      QStringList regExprList;
      regExprList << "([0-9]+)(?:x|X|\\*)([0-9]+)_([0-9]+)(?:Hz)?_([0-9]+)b?[\\._]";    // Something_2160x1440_60_8_more.yuv or Something_2160x1440_60_8b.yuv or Something_2160x1440_60Hz_8_more.yuv
      regExprList << "([0-9]+)(?:x|X|\\*)([0-9]+)_([0-9]+)(?:Hz)?[\\._]";               // Something_2160x1440_60_more.yuv or Something_2160x1440_60.yuv
      regExprList << "([0-9]+)(?:x|X|\\*)([0-9]+)[\\._]";                               // Something_2160x1440_more.yuv or Something_2160x1440.yuv

      for (QString regExpStr : regExprList)
      {
        QRegExp exp(regExpStr);
        if (exp.indexIn(name) > -1)
        {
          QString widthString = exp.cap(1);
          QString heightString = exp.cap(2);
          format.frameSize = QSize(widthString.toInt(), heightString.toInt());

          QString rateString = exp.cap(3);
          if (!rateString.isEmpty())
            format.frameRate = rateString.toDouble();

          QString bitDepthString = exp.cap(4);
          if (!bitDepthString.isEmpty())
            format.bitDepth = bitDepthString.toInt();

          // Don't check the following expressions
          break;
        }
      }
    }

    if (!format.frameSize.isValid())
    {
      // try resolution indicators with framerate: "1080p50", "720p24" ...
      QRegExp rx1080p("1080p([0-9]+)");
      QRegExp rx720p("720p([0-9]+)");

      if (rx1080p.indexIn(name) > -1)
      {
        format.frameSize = QSize(1920, 1080);
        QString frameRateString = rx1080p.cap(1);
        format.frameRate = frameRateString.toInt();
      }
      else if (rx720p.indexIn(name) > -1)
      {
        format.frameSize = QSize(1280, 720);
        QString frameRateString = rx720p.cap(1);
        format.frameRate = frameRateString.toInt();
      }
    }

    if (!format.frameSize.isValid())
    {
      // try to find resolution indicators (e.g. 'cif', 'hd') in file name
      if (name.contains("_cif", Qt::CaseInsensitive))
        format.frameSize = QSize(352, 288);
      else if (name.contains("_qcif", Qt::CaseInsensitive))
        format.frameSize = QSize(176, 144);
      else if (name.contains("_4cif", Qt::CaseInsensitive))
        format.frameSize = QSize(704, 576);
      else if (name.contains("UHD", Qt::CaseSensitive))
        format.frameSize = QSize(3840, 2160);
      else if (name.contains("HD", Qt::CaseSensitive))
        format.frameSize = QSize(1920, 1080);
      else if (name.contains("1080p", Qt::CaseSensitive))
        format.frameSize = QSize(1920, 1080);
      else if (name.contains("720p", Qt::CaseSensitive))
        format.frameSize = QSize(1280, 720);
    }

    // Second, if we were able to get a frame size but no frame rate, we will try to get a frame rate.
    if (format.frameSize.isValid() && format.frameRate == -1)
    {
      // Look for: 24fps, 50fps, 24FPS, 50FPS
      QRegExp rxFPS("([0-9]+)fps", Qt::CaseInsensitive);
      if (rxFPS.indexIn(name) > -1)
      {
        QString frameRateString = rxFPS.cap(1);
        format.frameRate = frameRateString.toInt();
      }
    }
    if (format.frameSize.isValid() && format.frameRate == -1)
    {
      // Look for: 24Hz, 50Hz, 24HZ, 50HZ
      QRegExp rxHZ("([0-9]+)HZ", Qt::CaseInsensitive);
      if (rxHZ.indexIn(name) > -1)
      {
        QString frameRateString = rxHZ.cap(1);
        format.frameRate = frameRateString.toInt();
      }
    }

    // Third, if we were able to get a frame size but no bit depth, we try to get a bit depth.
    if (format.frameSize.isValid() && format.bitDepth == -1)
    {
      // Look for: 10bit, 10BIT, 10-bit, 10-BIT
      QList<int> bitDepths = QList<int>() << 8 << 9 << 10 << 12 << 16;
      for (int bd : bitDepths)
      {
        if (name.contains(QString("%1bit").arg(bd), Qt::CaseInsensitive) || name.contains(QString("%1-bit").arg(bd), Qt::CaseInsensitive))
        {
          // That looks like a bit depth indicator
          format.bitDepth = bd;
          break;
        }
      }
    }

    // If we were able to get a frame size, try to get an indicator for packed formats
    if (format.frameSize.isValid())
    {
      QRegExp exp("(?:_|\\.|-)packed(?:_|\\.|-)");
      if (exp.indexIn(name) > -1)
      {
        format.packed = true;
      }
    }
  }

  return format;
}

// If you are loading a playlist and you have an absolute path and a relative path, this function will return
// the absolute path (if a file with that absolute path exists) or convert the relative path to an absolute
// one and return that (if that file exists). If neither exists the empty string is returned.
QString fileSource::getAbsPathFromAbsAndRel(const QString &currentPath, const QString &absolutePath, const QString &relativePath)
{
  QFileInfo checkAbsoluteFile(absolutePath);
  if (checkAbsoluteFile.exists())
    return absolutePath;

  QFileInfo plFileInfo(currentPath);
  QString combinePath = QDir(plFileInfo.path()).filePath(relativePath);
  QFileInfo checkRelativeFile(combinePath);
  if (checkRelativeFile.exists() && checkRelativeFile.isFile())
  {
    return QDir::cleanPath(combinePath);
  }

  return QString();
}

void fileSource::updateFileWatchSetting()
{
  // Install a file watcher if file watching is active in the settings.
  // The addPath/removePath functions will do nothing if called twice for the same file.
  QSettings settings;
  if (settings.value("WatchFiles",true).toBool())
    fileWatcher.addPath(fullFilePath);
  else
    fileWatcher.removePath(fullFilePath);
}

void fileSource::clearFileCache()
{
  if (!isFileOpened)
    return;

#ifdef Q_OS_WIN
  // We will close the QFile, open it using the FILE_FLAG_NO_BUFFERING flags, close it and reopen the QFile.
  // Suggested: http://stackoverflow.com/questions/478340/clear-file-cache-to-repeat-performance-testing
  QMutexLocker locker(&readMutex);
  srcFile.close();

  LPCWSTR file = (const wchar_t*) fullFilePath.utf16();
  HANDLE hFile = CreateFile(file, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);
  CloseHandle(hFile);

  srcFile.setFileName(fullFilePath);
  srcFile.open(QIODevice::ReadOnly);
#endif
}
