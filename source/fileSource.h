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

#include <QObject>
#include <QString>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QFileSystemWatcher>

#include "typedef.h"
#include "fileInfoWidget.h"

/* The fileSource class provides functions for accessing files. Besides the reading of
 * certain blocks of the file, it also directly provides information on the file for the
 * fileInfoWidget. It also adds functions for guessing the format from the filename.
 */
class fileSource : public QObject
{
  Q_OBJECT

public:
  fileSource();

  // Try to open the given file and install a watcher for the file.
  virtual bool openFile(const QString &filePath);

  // Return information on this file (like path, date created file Size ...)
  virtual QList<infoItem> getFileInfoList();

  QString absoluteFilePath() { return srcFile.isOpen() ? fileInfo.absoluteFilePath() : QString(); }
  QFileInfo getFileInfo() { return fileInfo; }

  // Return true if the file could be opened and is ready for use.
  bool isOk() { return srcFile.isOpen(); }

  QFile *getQFile() { return &srcFile; }

  // Pass on to srcFile
  virtual bool atEnd() { return !srcFile.isOpen() ? true : srcFile.atEnd(); }
  QByteArray readLine() { return !srcFile.isOpen() ? QByteArray() : srcFile.readLine(); }
  bool seek(qint64 pos) { return !srcFile.isOpen() ? false : srcFile.seek(pos); }
  qint64 pos() { return !srcFile.isOpen() ? 0 : srcFile.pos(); }

  // Guess the format (width, height, frameTate...) from the file name.
  // Certain patterns are recognized. E.g: "something_352x288_24.yuv"
  void formatFromFilename(QSize &frameSize, int &frameRate, int &bitDepth);

  // Get the file size in bytes
  qint64 getFileSize() { return !srcFile.isOpen() ? -1 : fileInfo.size(); }

  // Read the given number of bytes starting at startPos into the QByteArray out
  // Resize the QByteArray if necessary. Return how many bytes were read.
  qint64 readBytes(QByteArray &targetBuffer, qint64 startPos, qint64 nrBytes);
#if SSE_CONVERSION
  void readBytes(byteArrayAligned &data, qint64 startPos, qint64 nrBytes);
#endif

  QString getAbsoluteFilePath() { return fileInfo.absoluteFilePath(); }

  // Get the absolut path to the file (from absolute or relative path)
  static QString getAbsPathFromAbsAndRel(const QString &currentPath, const QString &absolutePath, const QString &relativePath);

  // Was the file changed by some other application?
  bool isFileChanged() { bool b = fileChanged; fileChanged = false; return b; }
  // Check if we are supposed to watch the file for changes. If no, remove the file watcher. If yes, install one.
  void updateFileWatchSetting();

private slots:
  void fileSystemWatcherFileChanged(const QString &path) { Q_UNUSED(path); fileChanged = true; }

protected:
  // Info on the source file. 
  QString   fullFilePath;
  QFileInfo fileInfo;

  // This file might not be open if the opening has failed.
  QFile srcFile;

private:
  // Watch the opened file for modifications
  QFileSystemWatcher fileWatcher;
  bool fileChanged;
};

#endif
