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

fileSource::fileSource()
{
  srcFile = NULL;
}

bool fileSource::openFile(QString yuvFilePath)
{
  // Check if the file exists
  fileInfo.setFile(yuvFilePath);
  if (!fileInfo.exists() || !fileInfo.isFile()) 
    return false;

  // open file for reading
  srcFile = new QFile(yuvFilePath);
  if (!srcFile->open(QIODevice::ReadOnly))
  {
    // Could not open file
    delete srcFile;
    srcFile = NULL;
    return false;
  }

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

void fileSource::formatFromFilename(int &width, int &height, int &frameRate, int &bitDepth, int &subFormat)
{
  // preset return values first
  width = -1;
  height = -1;
  frameRate = -1;
  bitDepth = -1;
  subFormat = -1;
  
  QString name = fileInfo.fileName();

  if (name.isEmpty())
    return;

  // parse filename and extract width, height and framerate
  // default format is: sequenceName_widthxheight_framerate.yuv
  QRegExp rxExtendedFormat("([0-9]+)x([0-9]+)_([0-9]+)_([0-9]+)_([0-9]+)");
  if (rxExtendedFormat.indexIn(name) > -1)
  {
    QString widthString = rxExtendedFormat.cap(1);
    width = widthString.toInt();

    QString heightString = rxExtendedFormat.cap(2);
    height = heightString.toInt();

    QString rateString = rxExtendedFormat.cap(3);
    frameRate = rateString.toDouble();

    QString bitDepthString = rxExtendedFormat.cap(4);
    bitDepth = bitDepthString.toInt();

    QString subSampling = rxExtendedFormat.cap(5);
    subFormat = subSampling.toInt();
    return;
  }
  
  QRegExp rxExtended("([0-9]+)x([0-9]+)_([0-9]+)_([0-9]+)");
  if (rxExtended.indexIn(name) > -1)
  {
    QString widthString = rxExtended.cap(1);
    width = widthString.toInt();

    QString heightString = rxExtended.cap(2);
    height = heightString.toInt();

    QString rateString = rxExtended.cap(3);
    frameRate = rateString.toDouble();

    QString bitDepthString = rxExtended.cap(4);
    bitDepth = bitDepthString.toInt();
    return;
  }

  QRegExp rxDefault("([0-9]+)x([0-9]+)_([0-9]+)");
  if (rxDefault.indexIn(name) > -1) {
    QString widthString = rxDefault.cap(1);
    width = widthString.toInt();

    QString heightString = rxDefault.cap(2);
    height = heightString.toInt();

    QString rateString = rxDefault.cap(3);
    frameRate = rateString.toDouble();

    bitDepth = 8; // assume 8 bit
    return;
  }

  QRegExp rxSizeOnly("([0-9]+)x([0-9]+)");
  if (rxSizeOnly.indexIn(name) > -1) {
    QString widthString = rxSizeOnly.cap(1);
    width = widthString.toInt();

    QString heightString = rxSizeOnly.cap(2);
    height = heightString.toInt();

    bitDepth = 8; // assume 8 bit
    return;
  }
  
  // try to find resolution indicators (e.g. 'cif', 'hd') in file name
  if (name.contains("_cif", Qt::CaseInsensitive))
  {
    width = 352;
    height = 288;
    return;
  }
  else if (name.contains("_qcif", Qt::CaseInsensitive))
  {
    width = 176;
    height = 144;
    return;
  }
  else if (name.contains("_4cif", Qt::CaseInsensitive))
  {
    width = 704;
    height = 576;
    return;
  }

  // try other resolution indicators with framerate: "1080p50", "720p24" ...
  QRegExp rx1080p("1080p([0-9]+)");
  if (rx1080p.indexIn(name) > -1) {
    width = 1920;
    height = 1080;
    
    QString frameRateString = rxSizeOnly.cap(1);
    frameRate = frameRateString.toInt();
    return;
  }

  QRegExp rx720p("720p([0-9]+)");
  if (rx720p.indexIn(name) > -1) {
    width = 1280;
    height = 720;
    
    QString frameRateString = rxSizeOnly.cap(1);
    frameRate = frameRateString.toInt();
    return;
  }

  // try without framerate indication: "1080p", "720p"
  if (name.contains("1080p", Qt::CaseSensitive)) {
    width = 1920;
    height = 1080;
    return;
  }
  if (name.contains("720p", Qt::CaseSensitive)) {
    width = 1280;
    height = 720;
    return;
  }

  // Maybe there is at least something about the frame rate in the filename
  // like: 24fps, 50Hz
  QRegExp rxFPS("([0-9]+)fps");
  if (rxFPS.indexIn(name) > -1) {
    QString frameRateString = rxSizeOnly.cap(1);
    frameRate = frameRateString.toInt();
    return;
  }

  QRegExp rxHZ("([0-9]+)HZ");
  if (rxFPS.indexIn(name) > -1) {
    QString frameRateString = rxSizeOnly.cap(1);
    frameRate = frameRateString.toInt();
    return;
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