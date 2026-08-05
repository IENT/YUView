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

#include "IDataSource.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace datasource
{

/**
 * @brief Local file data source with parallel reading support
 * 
 * This class provides file I/O for local files with optimized parallel reading
 * capability for multi-threaded caching scenarios.
 * 
 * PERFORMANCE OPTIMIZATION:
 * For large video files (>10GB), traditional serialized I/O causes severe
 * bottlenecks when multiple caching threads try to read simultaneously.
 * This implementation uses a pool of file handles (one per thread) to enable
 * true parallel I/O operations, significantly improving cache loading speed.
 * 
 * Key features:
 * - Thread-local file handles for lock-free parallel reads
 * - Automatic handle pooling and cleanup
 * - Backward compatible with single-threaded sequential reads
 */
class DataSourceLocalFile : public IDataSource
{
public:
  DataSourceLocalFile() = delete;
  DataSourceLocalFile(const std::filesystem::path &filePath);
  ~DataSourceLocalFile();

  [[nodiscard]] std::vector<InfoItem> getInfoList() const override;
  [[nodiscard]] bool                  atEnd() const override;
  [[nodiscard]] bool                  isOk() const override;
  [[nodiscard]] std::int64_t          getPosition() const override;

  void               clearFileCache() override;
  [[nodiscard]] bool wasSourceModified() const override;
  void               reloadAndResetDataSource() override;

  [[nodiscard]] bool         seek(const std::int64_t pos) override;
  [[nodiscard]] std::int64_t read(ByteVector &buffer, const std::int64_t nrBytes) override;

  /**
   * @brief Read data at specific position with parallel I/O support
   * 
   * Uses thread-local file handles to enable concurrent reads from multiple
   * caching threads without mutex contention. Each thread gets its own file
   * handle from the pool, eliminating serialization overhead.
   * 
   * @param buffer Output buffer (will be resized if needed)
   * @param position Byte offset to start reading from
   * @param nrBytes Number of bytes to read
   * @return Number of bytes actually read
   */
  [[nodiscard]] std::int64_t readAt(ByteVector &buffer, 
                                    const std::int64_t position, 
                                    const std::int64_t nrBytes) override;

  /**
   * @brief Check if parallel reading is supported
   * @return Always true for local files
   */
  [[nodiscard]] bool supportsParallelRead() const override { return true; }

  [[nodiscard]] std::optional<std::int64_t> getFileSize() const;
  [[nodiscard]] std::filesystem::path       getFilePath() const;

protected:
  std::filesystem::path                          filePath{};
  std::optional<std::filesystem::file_time_type> lastWriteTime{};
  bool                                           isFileOpened{};

  // Primary file handle for sequential reads (seek+read pattern)
  std::ifstream file{};
  std::int64_t  filePosition{};
  std::mutex readingMutex;

private:
  /**
   * @brief Get or create a file handle for the current thread
   * 
   * File handles are cached per-thread to avoid repeated open/close overhead.
   * Each thread gets its own independent file handle for lock-free I/O.
   * 
   * @return Pointer to thread-local file stream, or nullptr on error
   */
  std::ifstream* getThreadLocalFileHandle();

  /**
   * @brief Clean up all pooled file handles
   */
  void cleanupFileHandlePool();

  // Thread-local file handle pool for parallel reads
  // Key: thread ID, Value: unique_ptr to ifstream
  std::unordered_map<std::thread::id, std::unique_ptr<std::ifstream>> fileHandlePool;
  std::mutex poolMutex;  // Only protects pool access, not reads
};

} // namespace datasource
