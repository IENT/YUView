/*  YUView - YUV player with advanced analytics toolset
*   Copyright (C) 2015  Institut für Nachrichtentechnik
*                       RWTH Aachen University, GERMANY
*
*   YUView is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   YUView is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with YUView.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "fileSource.h"

#include "typedef.h"

#include <QFileInfo>
#include <QDateTime>
#include <QRegExp>
#include <QDir>
#include <QDebug>
#include <QSettings>

fileSource::fileSource()
{
  srcFile = NULL;
  fileChanged = false;

  connect(&fileWatcher, SIGNAL(fileChanged(const QString)), this, SLOT(fileSystemWatcherFileChanged(const QString)));
}

bool fileSource::openFile(QString filePath)
{
  // Check if the file exists
  fileInfo.setFile(filePath);
  if (!fileInfo.exists() || !fileInfo.isFile()) 
    return false;

  if (srcFile != NULL)
  {
    // We already opened a file. Close it.
    srcFile->close();
    delete srcFile;
  }

  // open file for reading
  srcFile = new QFile(filePath);
  if (!srcFile->open(QIODevice::ReadOnly))
  {
    // Could not open file
    delete srcFile;
    srcFile = NULL;
    return false;
  }
  
  // Save the full file path
  fullFilePath = filePath;

  // Install a watcher for the file (if file watching is active)
  updateFileWatchSetting();

  fileChanged = false;

  return true;
}

fileSource::~fileSource()
{
  if (srcFile)
  {
    srcFile->close();
    delete srcFile; 
  }
}

#if SSE_CONVERSION
// Resize the target array if necessary and read the given number of bytes to the data array
void fileSource::readBytes(byteArrayAligned &targetBuffer, qint64 startPos, qint64 nrBytes)
{
  if(!isOk())
    return;

  if (targetBuffer.size() < nrBytes)
    targetBuffer.resize(nrBytes);

  srcFile->seek(startPos);
  srcFile->read(targetBuffer.data(), nrBytes);
}
#endif

// Resize the target array if necessary and read the given number of bytes to the data array
qint64 fileSource::readBytes(QByteArray &targetBuffer, qint64 startPos, qint64 nrBytes)
{
  if(!isOk())
    return 0;

  if (targetBuffer.size() < nrBytes)
    targetBuffer.resize(nrBytes);

  srcFile->seek(startPos);
  return srcFile->read(targetBuffer.data(), nrBytes);
}

QList<infoItem> fileSource::getFileInfoList()
{
  QList<infoItem> infoList;

  if (srcFile == NULL)
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

void fileSource::formatFromFilename(QSize &frameSize, int &frameRate, int &bitDepth)
{
  // preset return values first
  frameSize = QSize();
  frameRate = -1;
  bitDepth = -1;
  
  // We are going to check two strings (one after the other) for indicators on the frames size, fps and bit depth.
  // 1: The file name, 2: The folder name that the file is contained in.
  QStringList checkStrings;

  // The full name of the file
  QString fileName = fileInfo.fileName();
  if (fileName.isEmpty())
    return;
  checkStrings.append(fileName);

  // The name of the folder that the file is in
  QString dirName = fileInfo.absoluteDir().dirName();
  checkStrings.append(dirName);
  
  foreach(QString name, checkStrings)
  {
    // First, we will try to get a frame size from the name
    if (!frameSize.isValid())
    {
      QRegExp rxExtended("([0-9]+)x([0-9]+)_([0-9]+)_([0-9]+)[\\._]");  // Something_2160x1440_60_8_more.yuv or Something_2160x1440_60_8.yuv
      QRegExp rxDefault("([0-9]+)x([0-9]+)_([0-9]+)[\\._]");            // Something_2160x1440_60_more.yuv or Something_2160x1440_60.yuv
      QRegExp rxSizeOnly("([0-9]+)x([0-9]+)[\\._]");                    // Something_2160x1440_more.yuv or Something_2160x1440.yuv

      if (rxExtended.indexIn(name) > -1)
      {
        QString widthString = rxExtended.cap(1);
        QString heightString = rxExtended.cap(2);
        frameSize = QSize(widthString.toInt(), heightString.toInt());

        QString rateString = rxExtended.cap(3);
        frameRate = rateString.toDouble();

        QString bitDepthString = rxExtended.cap(4);
        bitDepth = bitDepthString.toInt();
      }
      else if (rxDefault.indexIn(name) > -1) 
      {
        QString widthString = rxDefault.cap(1);
        QString heightString = rxDefault.cap(2);
        frameSize = QSize(widthString.toInt(), heightString.toInt());

        QString rateString = rxDefault.cap(3);
        frameRate = rateString.toDouble();
      }
      else if (rxSizeOnly.indexIn(name) > -1) 
      {
        QString widthString = rxSizeOnly.cap(1);
        QString heightString = rxSizeOnly.cap(2);
        frameSize = QSize(widthString.toInt(), heightString.toInt());
      }
    }

    if (!frameSize.isValid())
    {
      // try resolution indicators with framerate: "1080p50", "720p24" ...
      QRegExp rx1080p("1080p([0-9]+)");
      QRegExp rx720p("720p([0-9]+)");

      if (rx1080p.indexIn(name) > -1)
      {
        frameSize = QSize(1920, 1080);
        QString frameRateString = rx1080p.cap(1);
        frameRate = frameRateString.toInt();
      }
      else if (rx720p.indexIn(name) > -1) 
      {
        frameSize = QSize(1280, 720);
        QString frameRateString = rx720p.cap(1);
        frameRate = frameRateString.toInt();
      }
    }

    if (!frameSize.isValid())
    {
      // try to find resolution indicators (e.g. 'cif', 'hd') in file name
      if (name.contains("_cif", Qt::CaseInsensitive))
        frameSize = QSize(352, 288);
      else if (name.contains("_qcif", Qt::CaseInsensitive))
        frameSize = QSize(176, 144);
      else if (name.contains("_4cif", Qt::CaseInsensitive))
        frameSize = QSize(704, 576);
      else if (name.contains("UHD", Qt::CaseSensitive))
        frameSize = QSize(3840, 2160);
      else if (name.contains("HD", Qt::CaseSensitive))
        frameSize = QSize(1920, 1080);
      else if (name.contains("1080p", Qt::CaseSensitive))
        frameSize = QSize(1920, 1080);
      else if (name.contains("720p", Qt::CaseSensitive)) 
        frameSize = QSize(1280, 720);
    }

    // Second, if we were able to get a frame size but no frame rate, we will try to get a frame rate.
    if (frameSize.isValid() && frameRate == -1)
    {
      // Look for: 24fps, 50fps, 24FPS, 50FPS
      QRegExp rxFPS("([0-9]+)fps", Qt::CaseInsensitive);
      if (rxFPS.indexIn(name) > -1) 
      {
        QString frameRateString = rxFPS.cap(1);
        frameRate = frameRateString.toInt();
      }
    }
    if (frameSize.isValid() && frameRate == -1)
    {
      // Look for: 24Hz, 50Hz, 24HZ, 50HZ
      QRegExp rxHZ("([0-9]+)HZ", Qt::CaseInsensitive);
      if (rxHZ.indexIn(name) > -1) 
      {
        QString frameRateString = rxHZ.cap(1);
        frameRate = frameRateString.toInt();
      }
    }

    // Third, if we were able to get a frame size but no bit depth, we try to get a bit depth.
    if (frameSize.isValid() && bitDepth == -1)
    {
      // Look for: 10bit, 10BIT, 10-bit, 10-BIT
      QList<int> bitDepths = QList<int>() << 8 << 9 << 10 << 12 << 16;
      foreach(int bd, bitDepths)
      {
        if (name.contains(QString("%1bit").arg(bd), Qt::CaseInsensitive) || name.contains(QString("%1-bit").arg(bd), Qt::CaseInsensitive))
        {
          // That looks like a bit depth indicator
          bitDepth = bd;
          break;
        }
      }
    }
  }
}

// If you are loading a playlist and you have an absolut path and a relative path, this function will return
// the absolute path (if a file with that absolute path exists) or convert the relative path to an absolute
// one and return that (if that file exists). If neither exists the empty string is returned.
QString fileSource::getAbsPathFromAbsAndRel(QString currentPath, QString absolutePath, QString relativePath)
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

  return "";
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
