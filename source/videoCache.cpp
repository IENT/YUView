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

#define CACHING_DEBUG_OUTPUT 0
#if CACHING_DEBUG_OUTPUT
#include <QDebug>
#define DEBUG_CACHING qDebug
#else
#define DEBUG_CACHING(fmt,...) ((void)0)
#endif

void cacheWorkerThread::run()
{
  if (plItem == NULL)
    return;

  for (int i=0; i<=range.second - range.first; i++)
  {
    // Check if the video cache want's to abort the current process or if the item is not cachable anymore.
    if (interruptionRequest || !plItem->isCachable())
    {
      emit cachingFinished();
      return;
    }

    // Cache the frame
    DEBUG_CACHING( "Caching frame %d of %s", i, plItem->getName().toLatin1().data() );

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
  connect(playlist, SIGNAL(itemAboutToBeDeleted(playlistItem*)), this, SLOT(itemAboutToBeDeleted(playlistItem*)));

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
  DEBUG_CACHING("videoCache::playlistChanged");

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
  DEBUG_CACHING("videoCache::updateCacheQueue");

  QSettings settings;
  settings.beginGroup("VideoCache");
  bool cachingEnabled = settings.value("Enabled", true).toBool();
  qint64 maxCacheSize = settings.value("ThresholdValueMB", 49).toUInt() * 1024 * 1024;
  settings.endGroup();

  if (!cachingEnabled)
    return;

  int nrItems = playlist->topLevelItemCount();
  if (nrItems == 0)
    // No items in the playlist to cache
    return;

  // At first determine which items will go into the cache (and in which order)
  QList<QTreeWidgetItem*> selectedItems = playlist->selectedItems();
  QList<playlistItem*>    cachingItems;
  if (selectedItems.count() == 0)
  {
    // No item is selected. What should be buffered in this case?
    // Just buffer the playlist from the beginning.
    for (int i=0; i<nrItems; i++)
    {
      playlistItem *item = dynamic_cast<playlistItem*>(playlist->topLevelItem(i));
      if (item && item->isCachable())
        cachingItems.append(item);
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
        cachingItems.append(item);
    }

    // Wrap aroung at the end and continue at the beginning
    for (int i=0; i<selectionIdx; i++)
    {
      playlistItem *item = dynamic_cast<playlistItem*>(playlist->topLevelItem(i));
      if (item && item->isCachable())
        cachingItems.append(item);
    }
  }

  // Now walk through these items and fill the list of queue jobs.
  // Consider the maximum size of the cache
  qint64 cacheSize = 0;
  bool cacheFull = false;
  for (int i = 0; i < cachingItems.count() && !cacheFull; i++)
  {
    playlistItem *item = cachingItems[i];
    indexRange range = item->getFrameIndexRange();
    if (range.first == -1 || range.second == -1)
      // Invalid range. Probably the item is not set up correctly (width/height not set). Skip this item
      continue;

    // Is there enough space in the cache for all frames?
    qint64 itemCacheSize = item->getCachingFrameSize() * (range.second - range.first);
    if (cacheSize + itemCacheSize > maxCacheSize)
    {
      // See how many frames will fit
      qint64 remainingCacheSize = maxCacheSize - cacheSize;
      qint64 nrFramesCachable = remainingCacheSize / item->getCachingFrameSize();
      range.second = range.first + nrFramesCachable;

      // The cache is now full
      cacheFull = true;
    }

    // Add this item to the queue
    cacheQueue.enqueue( cacheJob(item, range) );
  }

  // We filled the cacheQueue (as full as the cache size allows).
  // Now we have to check if we have to delete frames from already cached items.
  // TODO

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

  DEBUG_CACHING("videoCache::workerCachingFinished - new state %d", workerState);
}

void videoCache::itemAboutToBeDeleted(playlistItem*)
{
  if (workerState != workerIdle)
  {
    DEBUG_CACHING("videoCache::itemAboutToBeDeleted - waiting for cacheThread");
    cacheThread.requestInterruption();
    cacheThread.wait();
    cacheThread.resetInterruptionRequest();
    DEBUG_CACHING("videoCache::itemAboutToBeDeleted - waiting for cacheThread done");
  }

  workerState = workerIdle;
}