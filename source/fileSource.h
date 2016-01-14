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

#ifndef FILESOURCE_H
#define FILESOURCE_H

#include <QString>
#include <QFile>
#include <QFileInfo>
#include "typedef.h"

class fileSource
{
public:
  fileSource(QString yuvFilePath);
  ~fileSource();

  // Return information on this file (like path, date created file Size ...)
  virtual QList<infoItem> getFileInfoList();

  // Return true if the file could be opened and is ready for use.
  bool isFileOk() { return srcFile != NULL; }

  // Guess the format (width, height, frameTate...) from the file name.
  // Certain patterns are recognized. E.g: "something_352x288_24.yuv"
  void formatFromFilename(int &width, int &height, int &frameRate, int &bitDepth, int &subFormat);

  // Get the file size in bytes
  qint64 getFileSize() { return (srcFile == NULL) ? -1 : fileInfo.size(); }

  // Read the given number of bytes starting at startPos into the QByteArray out
  // Resize the QByteArray if necessary
  void readBytes(QByteArray &targetBuffer, qint64 startPos, qint64 nrBytes);

  QString getAbsoluteFilePath() { return fileInfo.absoluteFilePath(); }

protected:

private:
  // Info on the source file. 
  QFileInfo fileInfo;

  // The pointer to the QFile to open. If opening failed, this will be NULL;
  QFile *srcFile;
};

#endif