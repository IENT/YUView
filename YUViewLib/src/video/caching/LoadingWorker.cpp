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

#include "LoadingWorker.h"

namespace video
{

#define LOADINGWORKER_DEBUG_LOADING 0
#if LOADINGWORKER_DEBUG_LOADING && !NDEBUG
#else
#define DEBUG_WORKER(fmt, ...) ((void)0)
#endif

LoadingWorker::LoadingWorker(QObject *parent) : QObject(parent)
{
  static int idCounter = 0;
  id                   = idCounter++;
}

QString LoadingWorker::getStatus()
{
  QMutexLocker lock(&m_stateMutex);
  return QString("T%1: %2").arg(id).arg(isWorking() ? QString::number(this->currentFrame)
                                                    : QString("-"));
}

void LoadingWorker::setJob(playlistItem *item, int frame, bool test)
{
  Q_ASSERT_X(item != nullptr, Q_FUNC_INFO, "Given item is nullptr");
  Q_ASSERT_X(
    frame >= 0 || !item->properties().isIndexedByFrame(), Q_FUNC_INFO, "Given frame index invalid");
  
  // Thread-safe job setup - protect against concurrent modifications
  QMutexLocker lock(&m_stateMutex);
  this->currentCacheItem = item;
  this->currentFrame     = frame;
  this->testMode         = test;
}

void LoadingWorker::processCacheJob()
{
  DEBUG_WORKER("LoadingWorker::processCacheJob invoke processCacheJobInternal");
  QMetaObject::invokeMethod(this, "processCacheJobInternal");
}

void LoadingWorker::processLoadingJob(bool playing, bool loadRawData)
{
  DEBUG_WORKER("LoadingWorker::processLoadingJob invoke processLoadingJobInternal");
  QMetaObject::invokeMethod(
    this, "processLoadingJobInternal", Q_ARG(bool, playing), Q_ARG(bool, loadRawData));
}

void LoadingWorker::processCacheJobInternal()
{
  Q_ASSERT_X(this->currentCacheItem != nullptr, Q_FUNC_INFO, "Invalid Job - Item is nullptr");
  Q_ASSERT_X(this->currentFrame >= 0 || !this->currentCacheItem->properties().isIndexedByFrame(),
             Q_FUNC_INFO,
             "Given frame index invalid");
  DEBUG_WORKER("LoadingWorker::processCacheJobInternal");

  // Just cache the frame that was given to us.
  // This is performed in the thread that this worker is currently placed in.
  this->currentCacheItem->cacheFrame(currentFrame, testMode);

  this->currentCacheItem = nullptr;
  DEBUG_WORKER("LoadingWorker::processCacheJobInternal emit loadingFinished");
  emit loadingFinished();
}

void LoadingWorker::processLoadingJobInternal(bool playing, bool loadRawData)
{
  Q_ASSERT_X(this->currentCacheItem != nullptr, Q_FUNC_INFO, "The set job is nullptr");
  Q_ASSERT_X((!this->currentCacheItem->properties().isIndexedByFrame() || currentFrame >= 0),
             Q_FUNC_INFO,
             "The set frame index is invalid");
  Q_ASSERT_X(!this->currentCacheItem->taggedForDeletion(),
             Q_FUNC_INFO,
             "The set job was tagged for deletion");
  DEBUG_WORKER(Q_FUNC_INFO);

  // Load the frame of the item that was given to us.
  // This is performed in the thread (the loading thread with higher priority.
  this->currentCacheItem->loadFrame(currentFrame, playing, loadRawData);

  this->currentCacheItem = nullptr;
  emit loadingFinished();
  DEBUG_WORKER("LoadingWorker::processLoadingJobInternal emit loadingFinished");
}

} // namespace video