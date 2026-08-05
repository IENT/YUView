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
#include <QThread>

#include <common/EnumMapper.h>
#include <common/InfoItemAndData.h>
#include <common/Typedef.h>

#include <filesystem>
#include <memory>
#include <unordered_map>

enum class InputFormat
{
  Invalid = -1,
  AnnexBHEVC, // Raw HEVC annex B file
  AnnexBAVC,  // Raw AVC annex B file
  AnnexBVVC,  // Raw VVC annex B file
  Libav       // This is some sort of container file which we will read using libavformat
};

constexpr EnumMapper<InputFormat, 5> InputFormatMapper = {
    std::make_pair(InputFormat::Invalid, "Invalid"),
    std::make_pair(InputFormat::AnnexBHEVC, "AnnexBHEVC"),
    std::make_pair(InputFormat::AnnexBAVC, "AnnexBAVC"),
    std::make_pair(InputFormat::AnnexBVVC, "AnnexBVVC"),
    std::make_pair(InputFormat::Libav, "Libav")};

/* The FileSource class provides functions for accessing files. Besides the reading of
 * certain blocks of the file, it also directly provides information on the file for the
 * fileInfoWidget. It also adds functions for guessing the format from the filename.
 */
class FileSource : public QObject
{
  Q_OBJECT

public:
  FileSource();

  virtual bool openFile(const std::filesystem::path &filePath);

  virtual std::vector<InfoItem> getFileInfoList() const;
  std::optional<int64_t>        getFileSize() const;
  /**
   * @brief Return the absolute path of the opened file as a native filesystem path.
   * @return Empty path when no file is open.
   */
  std::filesystem::path getAbsoluteFilePath() const;
  QFile                        *getQFile() { return &this->srcFile; }
  bool                          getAndResetFileChangedFlag();

  // Return true if the file could be opened and is ready for use.
  bool isOk() const { return this->isFileOpened; }

  virtual bool atEnd() const { return !this->isFileOpened ? true : this->srcFile.atEnd(); }
  QByteArray   readLine() { return !this->isFileOpened ? QByteArray() : this->srcFile.readLine(); }
  virtual bool seek(int64_t pos) { return !this->isFileOpened ? false : this->srcFile.seek(pos); }
  int64_t      pos() { return !this->isFileOpened ? 0 : this->srcFile.pos(); }

  // Get the file size in bytes

  // Read the given number of bytes starting at startPos into the QByteArray out
  // Resize the QByteArray if necessary. Return how many bytes were read.
  int64_t readBytes(QByteArray &targetBuffer, int64_t startPos, int64_t nrBytes);

  /**
   * @brief Read bytes at position using parallel I/O
   * 
   * This method enables true parallel file I/O by using thread-local file handles.
   * For large video files (>10GB), this significantly improves cache loading
   * performance when multiple threads read simultaneously.
   * 
   * PERFORMANCE IMPROVEMENT:
   * - Traditional readBytes: All reads serialized by single mutex
   * - readBytesParallel: Each thread gets own file handle, no mutex contention
   * 
   * @param targetBuffer Output buffer (will be resized if needed)
   * @param startPos Byte offset to start reading from
   * @param nrBytes Number of bytes to read
   * @return Number of bytes actually read
   */
  int64_t readBytesParallel(QByteArray &targetBuffer, int64_t startPos, int64_t nrBytes);

  /**
   * @brief Check if parallel reading is supported
   * @return true (always supported for local files)
   */
  bool supportsParallelRead() const { return true; }

  void updateFileWatchSetting();
  void clearFileCache();

  // Cleanup parallel file handles on destruction
  virtual ~FileSource();

private slots:
  void fileSystemWatcherFileChanged(const QString &) { fileChanged = true; }

protected:
  std::filesystem::path fullFilePath{};
  QFile                 srcFile;
  bool                  isFileOpened{};

private:
  /**
   * @brief Get or create thread-local file handle for parallel reads
   * 
   * Each calling thread gets its own QFile handle, enabling lock-free I/O.
   * Handles are cached per thread-ID and reused.
   * 
   * @return Pointer to thread-local QFile, or nullptr on error
   */
  QFile* getThreadLocalFileHandle();

  /**
   * @brief Clean up all pooled file handles
   */
  void cleanupFileHandlePool();

  QFileSystemWatcher fileWatcher{};
  bool               fileChanged{};

  QMutex readMutex;

  // Thread-local file handle pool for parallel reading
  // Maps thread ID to unique QFile handle
  std::unordered_map<Qt::HANDLE, std::unique_ptr<QFile>> fileHandlePool;
  QMutex poolMutex;  // Only protects pool access, not individual reads
};
