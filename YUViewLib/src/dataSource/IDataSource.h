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

#include <common/InfoItemAndData.h>
#include <common/Typedef.h>

namespace datasource
{

/* The data source interface defines a something that can provide data.
 * The source of the data could be a local file, a remote file or maybe
 * something that generates data. It does not matter
 */
class IDataSource
{
public:
  [[nodiscard]] virtual std::vector<InfoItem> getInfoList() const = 0;
  [[nodiscard]] virtual bool                  atEnd() const       = 0;
  [[nodiscard]] virtual bool                  isOk() const        = 0;
  [[nodiscard]] virtual std::int64_t          getPosition() const = 0;

  virtual void               clearFileCache()           = 0;
  [[nodiscard]] virtual bool wasSourceModified() const  = 0;
  virtual void               reloadAndResetDataSource() = 0;

  explicit operator bool() const { return this->isOk(); }

  [[nodiscard]] virtual bool         seek(const std::int64_t pos)                         = 0;
  [[nodiscard]] virtual std::int64_t read(ByteVector &buffer, const std::int64_t nrBytes) = 0;

  /**
   * @brief Read data at specific position without mutex serialization
   * 
   * This method supports parallel reading from multiple threads by using
   * separate file handles per thread. Unlike seek()+read() which are serialized
   * by a mutex, this method can be called concurrently from multiple threads.
   * 
   * Performance improvement: For large video files (>10GB), this enables true
   * parallel I/O when caching frames, significantly reducing buffer loading time.
   * 
   * Default implementation falls back to seek()+read() for compatibility.
   * 
   * @param buffer Output buffer for read data
   * @param position File position to start reading from
   * @param nrBytes Number of bytes to read
   * @return Number of bytes actually read
   */
  [[nodiscard]] virtual std::int64_t readAt(ByteVector &buffer, 
                                            const std::int64_t position, 
                                            const std::int64_t nrBytes)
  {
    // Default implementation: fallback to serialized seek+read
    if (!this->seek(position))
      return 0;
    return this->read(buffer, nrBytes);
  }

  /**
   * @brief Check if parallel reading is supported
   * 
   * @return true if readAt() uses true parallel I/O (no global mutex)
   */
  [[nodiscard]] virtual bool supportsParallelRead() const { return false; }
};

} // namespace datasource
