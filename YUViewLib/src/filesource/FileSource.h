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

#pragma once

#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QMutex>
#include <QMutexLocker>
#include <QString>

#include <common/EnumMapper.h>
#include <common/FileInfo.h>
#include <common/Typedef.h>

enum class InputFormat
{
  Invalid = -1,
  AnnexBHEVC, // Raw HEVC annex B file
  AnnexBAVC,  // Raw AVC annex B file
  AnnexBVVC,  // Raw VVC annex B file
  OBUAV1,     // Raw OBU file with AV1
  Libav       // This is some sort of container file which we will read using libavformat
};

const auto InputFormatMapper = EnumMapper<InputFormat>({{InputFormat::Invalid, "Invalid"},
                                                        {InputFormat::AnnexBHEVC, "AnnexBHEVC"},
                                                        {InputFormat::AnnexBAVC, "AnnexBAVC"},
                                                        {InputFormat::AnnexBVVC, "AnnexBVVC"},
                                                        {InputFormat::OBUAV1, "OBUAV1"},
                                                        {InputFormat::Libav, "Libav"}});

struct DataAndStartEndPos
{
  QByteArray      data{};
  FileStartEndPos startEnd{};
};

/* The FileSource class provides functions for accessing files. Besides the reading of
 * certain blocks of the file, it also directly provides information on the file for the
 * fileInfoWidget. It also adds functions for guessing the format from the filename.
 */
class FileSource : public QObject
{
  Q_OBJECT

public:
  FileSource();

  virtual bool openFile(const QString &filePath);

  [[nodiscard]] virtual QList<InfoItem> getFileInfoList() const;
  [[nodiscard]] int64_t                 getFileSize() const;
  [[nodiscard]] QString                 getAbsoluteFilePath() const;
  [[nodiscard]] QFileInfo               getFileInfo() const { return this->fileInfo; }
  [[nodiscard]] QFile *                 getQFile() { return &this->srcFile; }
  [[nodiscard]] bool                    getAndResetFileChangedFlag();

  // Return true if the file could be opened and is ready for use.
  [[nodiscard]] bool         isOpen() const { return this->srcFile.isOpen(); }
  [[nodiscard]] virtual bool atEnd() const;
  [[nodiscard]] int64_t      pos() const { return this->srcFile.pos(); }

  QByteArray   readLine() { return this->srcFile.readLine(); }
  virtual bool seek(int64_t pos) { return this->srcFile.seek(pos); }

  struct FileFormat
  {
    Size     frameSize;
    int      frameRate{-1};
    unsigned bitDepth{};
    bool     packed{false};
  };
  FileFormat guessFormatFromFilename() const;

  // Get the file size in bytes

  // Read the given number of bytes starting at startPos into the QByteArray out
  // Resize the QByteArray if necessary. Return how many bytes were read.
  int64_t readBytes(QByteArray &targetBuffer, int64_t startPos, int64_t nrBytes);
#if SSE_CONVERSION
  void readBytes(byteArrayAligned &data, int64_t startPos, int64_t nrBytes);
#endif

  static QString getAbsPathFromAbsAndRel(const QString &currentPath,
                                         const QString &absolutePath,
                                         const QString &relativePath);

  void updateFileWatchSetting();
  void clearFileCache();

private slots:
  void fileSystemWatcherFileChanged(const QString &) { fileChanged = true; }

protected:
  QString   fullFilePath{};
  QFileInfo fileInfo;
  QFile     srcFile;

private:
  QFileSystemWatcher fileWatcher{};
  bool               fileChanged{};

  QMutex readMutex;
};
