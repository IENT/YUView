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

#ifndef FILESOURCE_H
#define FILESOURCE_H

#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QMutex>
#include <QMutexLocker>
#include <QString>
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
  virtual QList<infoItem> getFileInfoList() const;

  QString absoluteFilePath() const { return isFileOpened ? fileInfo.absoluteFilePath() : QString(); }
  QFileInfo getFileInfo() const { return fileInfo; }

  // Return true if the file could be opened and is ready for use.
  bool isOk() const { return isFileOpened; }

  QFile *getQFile() { return &srcFile; }

  // Pass on to srcFile
  virtual bool atEnd() const { return !isFileOpened ? true : srcFile.atEnd(); }
  QByteArray readLine() { return !isFileOpened ? QByteArray() : srcFile.readLine(); }
  virtual bool seek(int64_t pos) { return !isFileOpened ? false : srcFile.seek(pos); }
  int64_t pos() { return !isFileOpened ? 0 : srcFile.pos(); }

  // Guess the format (width, height, frameTate...) from the file name.
  // Certain patterns are recognized. E.g: "something_352x288_24.yuv"
  void formatFromFilename(QSize &frameSize, int &frameRate, int &bitDepth) const;

  // Get the file size in bytes
  int64_t getFileSize() const { return !isFileOpened ? -1 : fileInfo.size(); }

  // Read the given number of bytes starting at startPos into the QByteArray out
  // Resize the QByteArray if necessary. Return how many bytes were read.
  int64_t readBytes(QByteArray &targetBuffer, int64_t startPos, int64_t nrBytes);
#if SSE_CONVERSION
  void readBytes(byteArrayAligned &data, int64_t startPos, int64_t nrBytes);
#endif

  QString getAbsoluteFilePath() const { return fileInfo.absoluteFilePath(); }

  // Get the absolute path to the file (from absolute or relative path)
  static QString getAbsPathFromAbsAndRel(const QString &currentPath, const QString &absolutePath, const QString &relativePath);

  // Was the file changed by some other application?
  bool isFileChanged() { bool b = fileChanged; fileChanged = false; return b; }
  // Check if we are supposed to watch the file for changes. If no, remove the file watcher. If yes, install one.
  void updateFileWatchSetting();

  // Clear the cache of the file in the system. Currently only windows supported.
  void clearFileCache();

private slots:
  void fileSystemWatcherFileChanged(const QString &path) { Q_UNUSED(path); fileChanged = true; }

protected:
  // Info on the source file.
  QString   fullFilePath;
  QFileInfo fileInfo;

  // This file might not be open if the opening has failed.
  QFile srcFile;
  bool isFileOpened;

private:
  // Watch the opened file for modifications
  QFileSystemWatcher fileWatcher;
  bool fileChanged;

  // protect the read function with a mutex
  QMutex readMutex;
};

#endif
