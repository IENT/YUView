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

#include "videoCache.h"

#define CACHING_DEBUG_OUTPUT 1

void cacheWorkerThread::run()
{
  for (int i=0; i<range.second - range.first; i++)
  {
    // Check if the video cache want's to abort the current process
    if (interruptionRequest)
    {
      emit cachingFinished();
      return;
    }

    // Cache the frame
#if CACHING_DEBUG_OUTPUT
    qDebug() << "Caching frame " << i << " of " << plItem->getName();
#endif

    plItem->cacheFrame(i);
  }

  // Done
  emit cachingFinished();
}

videoCache::videoCache(PlaylistTreeWidget *playlistTreeWidget, PlaybackController *playbackController, QObject *parent)
  : QObject(parent)
{
  playlist = playlistTreeWidget;
  playback = playbackController;

  connect(playlist, SIGNAL(playlistChanged()), this, SLOT(playlistChanged()));

  // Setup a new Thread. Create a new cacheWorker and push it to the new thread.
  // Connect the signals/slots to communicate with the cacheWorker.  
  connect(&cacheThread, &cacheWorkerThread::cachingFinished, this, &videoCache::workerCachingFinished);
  cacheThread.start();

  workerState = workerIdle;
}

videoCache::~videoCache()
{
  //clearCache();
}

void videoCache::playlistChanged()
{
  // The playlist changed. We have to rethink what to cache next.
#if CACHING_DEBUG_OUTPUT
  qDebug() << "videoCache::playlistChanged";
#endif

  if (workerState == workerRunning)
  {
    // First the worker has to stop. Request a stop and an update of the queue.
    cacheThread.requestInterruption();
    workerState = workerIntReqRestart;
    return;
  }
  else if (workerState == workerIntReqRestart)
  {
    // The worker is still running but we already requrested an interrupt and an update of the queue.
    return;
  }

  assert(workerState == workerIdle);
  // Otherwise (the worker is idle), update the cache queue and restart the worker.
  updateCacheQueue();
  
  pushNextTaskToWorker();
}

void videoCache::updateCacheQueue()
{
  // Now calculate the new list of frames to cache and run the cacher
#if CACHING_DEBUG_OUTPUT
  qDebug() << "videoCache::updateCacheQueue()";
#endif
  
  int nrItems = playlist->topLevelItemCount();
  if (nrItems == 0)
    // No items in the playlist to cache
    return;

  QList<QTreeWidgetItem*> selectedItems = playlist->selectedItems();
  
  if (selectedItems.count() == 0)
  {
    // No item is selected. What should be buffered in this case?
    // Just buffer the playlist from the beginning.

    for (int i=0; i<nrItems; i++)
    {
      playlistItem *item = dynamic_cast<playlistItem*>(playlist->topLevelItem(i));
      if (item && item->isCachable())
      {
        indexRange range = item->getFrameIndexRange();

        // Add this item to the queue
        cacheQueue.enqueue( cacheJob(item, range) );
      }
    }
  }
  else
  {
    // An item is selected. Buffer this item and then continue with the next items in the playlist.
    int selectionIdx = playlist->indexOfTopLevelItem(selectedItems[0]);

    for (int i=selectionIdx; i<nrItems; i++)
    {
      playlistItem *item = dynamic_cast<playlistItem*>(playlist->topLevelItem(i));
      if (item && item->isCachable())
      {
        indexRange range = item->getFrameIndexRange();

        // Add this item to the queue
        cacheQueue.enqueue( cacheJob(item, range) );
      }
    }

    // Wrap aroung at the end and continue at the beginning
    for (int i=0; i<selectionIdx; i++)
    {
      playlistItem *item = dynamic_cast<playlistItem*>(playlist->topLevelItem(i));
      if (item && item->isCachable())
      {
        indexRange range = item->getFrameIndexRange();

        // Add this item to the queue
        cacheQueue.enqueue( cacheJob(item, range) );
      }
    }
  }
}

void videoCache::pushNextTaskToWorker()
{
  if (cacheQueue.isEmpty())
  {
    // No more jobs to work on
    workerState = workerIdle;
  }
  else
  {
    cacheJob nextJob = cacheQueue.dequeue();
    cacheThread.setJob(nextJob.plItem, nextJob.frameRange);
    cacheThread.start();
    workerState = workerRunning;
  }
}

void videoCache::workerCachingFinished()
{
  if (workerState == workerRunning)
  {
    // Process the next item from the queue
    pushNextTaskToWorker();
  }
  else if (workerState == workerIntReqStop)
  {
    // We were asked to stop.
    workerState = workerIdle;
    cacheThread.resetInterruptionRequest();
  }
  else if (workerState == workerIntReqRestart)
  {
    // We were asked to stop so that the cache queue can be updated and the worker could be restated.
    cacheThread.resetInterruptionRequest();
    updateCacheQueue();
    pushNextTaskToWorker();
  }

#if CACHING_DEBUG_OUTPUT
  qDebug() << "videoCache::workerCachingFinished - new state " << workerState;
#endif
}