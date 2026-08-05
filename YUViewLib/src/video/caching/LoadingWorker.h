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

#include "playlistitem/playlistItem.h"

#include <QObject>
#include <QString>
#include <QMutex>
#include <QMutexLocker>
#include <atomic>

namespace video
{

// Thread-safe loading worker with proper synchronization for concurrent cache operations.
// This class ensures that the 4K 10-bit YUV buffer loading process is protected against
// race conditions when users drag new files or change color conversion settings.
class LoadingWorker : public QObject
{
  Q_OBJECT
public:
  LoadingWorker(QObject *parent);
  ~LoadingWorker() {}

  // Thread-safe getters - use mutex for compound operations
  playlistItem *getCacheItem() { 
    QMutexLocker lock(&m_stateMutex);
    return this->currentCacheItem; 
  }
  int getCacheFrame() { 
    QMutexLocker lock(&m_stateMutex);
    return this->currentFrame; 
  }
  
  // Thread-safe job setup
  void setJob(playlistItem *item, int frame, bool test = false);
  
  // Atomic working state for lock-free fast path checks
  void setWorking(bool state) { m_working.store(state, std::memory_order_release); }
  bool isWorking() const { return m_working.load(std::memory_order_acquire); }
  
  // Get current item safely for comparison (used in deletion checks)
  playlistItem* getCacheItemUnsafe() const { return currentCacheItem; }
  
  QString getStatus();

  // Process the job in the thread that this worker was moved to. This function can be directly
  // called from the main thread. It will still process the call in the separate thread.
  void processCacheJob();
  void processLoadingJob(bool playing, bool loadRawData);
  
signals:
  void loadingFinished();
  
private slots:
  void processCacheJobInternal();
  void processLoadingJobInternal(bool playing, bool loadRawData);

private:
  // Mutex for protecting state access during job setup and processing
  mutable QMutex m_stateMutex;
  
  playlistItem *currentCacheItem{};
  int           currentFrame{};
  std::atomic<bool> m_working{false};  // Atomic for fast lock-free checks
  bool          testMode{};
  int           id{}; // A static ID of the thread. Only used in getStatus().
};

} // namespace video
