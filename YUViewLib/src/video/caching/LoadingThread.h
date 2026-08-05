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

#include <QThread>
#include <atomic>

#include "LoadingWorker.h"

namespace video
{

#define LOADINGTHREAD_DEBUG_LOADING 0
#if LOADINGTHREAD_DEBUG_LOADING && !NDEBUG
#else
#define DEBUG_THREAD(fmt, ...) ((void)0)
#endif

// Thread-safe loading thread with atomic quitting flag.
// This ensures proper synchronization when the user drags new files during 4K 10-bit buffer loading.
class LoadingThread : public QThread
{
  Q_OBJECT
public:
  LoadingThread(QObject *parent) : QThread(parent)
  {
    // Create a new worker and move it to this thread
    this->threadWorker.reset(new LoadingWorker(nullptr));
    this->threadWorker->moveToThread(this);
  }
  ~LoadingThread() {}

  void quitWhenDone()
  {
    m_quitting.store(true, std::memory_order_release);
    if (this->threadWorker->isWorking())
    {
      // We must wait until the worker is done.
      DEBUG_THREAD("loadingThread::quitWhenDone waiting for worker to finish...");
      connect(worker(),
              &LoadingWorker::loadingFinished,
              this,
              [=]
              {
                DEBUG_THREAD("loadingThread::quitWhenDone worker done -> quit");
                quit();
              },
              Qt::UniqueConnection);  // Prevent duplicate connections
    }
    else
    {
      DEBUG_THREAD("loadingThread::quitWhenDone quit now");
      quit();
    }
  }

  LoadingWorker *worker() { return this->threadWorker.get(); }
  bool isQuitting() const { return m_quitting.load(std::memory_order_acquire); }

private:
  std::unique_ptr<LoadingWorker> threadWorker{};
  std::atomic<bool> m_quitting{false}; // Atomic flag for thread-safe quit signaling
};

} // namespace video
