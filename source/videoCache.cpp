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
#include <QPainter>

#define CACHING_DEBUG_OUTPUT 1
#if CACHING_DEBUG_OUTPUT
#include <QDebug>
#define DEBUG_CACHING qDebug
#else
#define DEBUG_CACHING(fmt,...) ((void)0)
#endif

void videoCacheStatusWidget::paintEvent(QPaintEvent * event)
{
  // Draw
  QPainter painter(this);
  QSize s = size();

  // Get all items from the playlist
  QList<playlistItem*> allItems = playlist->getAllPlaylistItems();

  // Get if caching is enabled and how much memory we can use for the cache
  QSettings settings;
  settings.beginGroup("VideoCache");
  bool cachingEnabled = settings.value("Enabled", true).toBool();
  qint64 cacheLevelMaxMB = settings.value("ThresholdValueMB", 49).toUInt();
  qint64 cacheLevelMax = cacheLevelMaxMB * 1024 * 1024;
  settings.endGroup();

  // Let's find out how much space in the cache is used.
  // In combination with cacheLevelMax we also know how much space is free.
  qint64 cacheLevel = 0;
  static QList<QColor> colors = {Qt::red, Qt::green, Qt::blue, Qt::cyan, Qt::magenta, Qt::yellow,Qt::darkRed, Qt::darkGreen, Qt::darkBlue, Qt::darkCyan, Qt::darkMagenta, Qt::darkYellow};
  for (int i = 0; i < allItems.count(); i++)
  {
    playlistItem *item = allItems.at(i);
    qint64 itemCacheSize = item->getCachedFrames().count() * item->getCachingFrameSize();

    // Draw a bow representing the items cache size in the buffer
    QColor c = colors.at(i % colors.count());
    int xStart = (int)((float)cacheLevel / cacheLevelMax * s.width());
    int xEnd   = (int)((float)(cacheLevel + itemCacheSize) / cacheLevelMax * s.width());
    painter.fillRect(xStart, 0, xEnd - xStart, s.height(), c);

    cacheLevel += itemCacheSize;
  }

  // Draw the fill status as text
  painter.setPen(Qt::black);
  unsigned int cacheLevelMB = cacheLevel / 1000000;
  QString pTxt = QString("%1 MB / %2 MB / %3 KB/s").arg(cacheLevelMB).arg(cacheLevelMaxMB).arg(cache->cacheRateInBytesPerMs);
  painter.drawText(0, 0, s.width(), s.height(), Qt::AlignCenter, pTxt);

  // Only draw the border
  painter.drawRect(0, 0, s.width()-1, s.height()-1);
}

void cacheWorkerThread::run()
{
  if (plItem == NULL)
    return;

  QTime cacheRateUpdater;
  cacheRateUpdater.start();

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

    QTime cacheRateTimer;
    cacheRateTimer.start();

    plItem->cacheFrame(i);

    int nMilliseconds = cacheRateTimer.elapsed();
    if (nMilliseconds>0)
    {
      int cachingRateNew = plItem->getCachingFrameSize() / nMilliseconds;
      cachingRateBytePerms = (cachingRateNew + cachingRateBytePerms * cachingRateMeasurementPoints)/(++cachingRateMeasurementPoints);
      if (cacheRateUpdater.elapsed()>1000)
      {
        emit updateCachingRate(cachingRateBytePerms);
        cacheRateUpdater.restart();
      }
    }
    DEBUG_CACHING("Current Rate: %i Byte / ms",cachingRateBytePerms);
  }
  cachingRateMeasurementPoints=1;
  // Done
  emit cachingFinished();
}

videoCache::videoCache(PlaylistTreeWidget *playlistTreeWidget, PlaybackController *playbackController, QObject *parent)
  : QObject(parent)
{
  playlist = playlistTreeWidget;
  playback = playbackController;
  cacheRateInBytesPerMs = 0;

  connect(playlist, SIGNAL(playlistChanged()), this, SLOT(playlistChanged()));
  connect(playlist, SIGNAL(itemAboutToBeDeleted(playlistItem*)), this, SLOT(itemAboutToBeDeleted(playlistItem*)));

  // Setup a new Thread. Create a new cacheWorker and push it to the new thread.
  // Connect the signals/slots to communicate with the cacheWorker.
  connect(&cacheThread, &cacheWorkerThread::cachingFinished, this, &videoCache::workerCachingFinished);
  connect(&cacheThread, SIGNAL(updateCachingRate(unsigned int)),this,SLOT(updateCachingRate(unsigned int)));
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

  // Get if caching is enabled and how much memory we can use for the cache
  QSettings settings;
  settings.beginGroup("VideoCache");
  bool cachingEnabled = settings.value("Enabled", true).toBool();
  qint64 cacheLevelMax = (qint64)settings.value("ThresholdValueMB", 49).toUInt() * 1024 * 1024;
  settings.endGroup();

  if (!cachingEnabled)
    return;  // Caching disabled

  //// At first determine which items will go into the cache (and in which order)
  //QList<QTreeWidgetItem*> selectedItems = playlist->selectedItems();
  //QList<playlistItem*>    cachingItems;
  //if (selectedItems.count() == 0)
  //{
  //  // No item is selected. What should be buffered in this case?
  //  // Just buffer the playlist from the beginning.
  //  for (int i=0; i<nrItems; i++)
  //  {
  //    playlistItem *item = dynamic_cast<playlistItem*>(playlist->topLevelItem(i));
  //    if (item && item->isCachable())
  //      cachingItems.append(item);
  //  }
  //}
  //else
  //{
  //  // An item is selected. Buffer this item and then continue with the next items in the playlist.
  //  int selectionIdx = playlist->indexOfTopLevelItem(selectedItems[0]);

  //  for (int i=selectionIdx; i<nrItems; i++)
  //  {
  //    playlistItem *item = dynamic_cast<playlistItem*>(playlist->topLevelItem(i));
  //    if (item && item->isCachable())
  //      cachingItems.append(item);
  //  }

  //  // Wrap aroung at the end and continue from the beginning of the playlist
  //  for (int i=0; i<selectionIdx; i++)
  //  {
  //    playlistItem *item = dynamic_cast<playlistItem*>(playlist->topLevelItem(i));
  //    if (item && item->isCachable())
  //      cachingItems.append(item);
  //  }
  //}

  // Get all items from the playlist
  QList<playlistItem*> allItems = playlist->getAllPlaylistItems();
  if (allItems.count() == 0)
    // No items in the playlist to cache
    return;

  // At first, let's find out how much space in the cache is used.
  // In combination with cacheLevelMax we also know how much space is free.
  qint64 cacheLevel = 0;
  foreach(playlistItem *item, allItems)
  {
    cacheLevel += item->getCachedFrames().count() * item->getCachingFrameSize();
  }

  // Now our caching strategy depends on the current mode we are in (is playback running or not?).
  if (!playback->playing())
  {
    DEBUG_CACHING("videoCache::NoPlaybackType");
    // Playback is not running. Our caching priority list is like this:
    // 1: Cache all the frames in the item that is currently selected. In order to achieve this, we will agressively
    //    delete other frames from other sequences in the cache. This has highest priority.
    // 2: Cache all the frames from the following items (while there is space left in the cache). We will not remove any frames
    //    from other items from the cache to achieve this. This is not the highest priority.
    //
    // When frames have to be removed to fit the selected sequence into the cache, the following priorities apply to frames from
    // other sequences. (The ones with highest priotity get removed last)
    // 1: The frames from the previous item have highest priority and are removed last. It is very likely that in 'interactive'
    //    (playback is not running) mode, the user will go back to the previous item.
    // 2: The item after this item is next in the priority list.
    // 3: The item after 2 is next and so on (wrap around in the playlist) until the previous item is reached.

    // Let's start with the currently selected item (if no item is selected, the first item in the playlist is considered as being selected)
    playlistItem *firstSelection, *secondSelection;
    playlist->getSelectedItems(firstSelection, secondSelection);
    if (firstSelection == NULL)
      firstSelection = allItems[0];

    // How much space do we need to cache the entire item?
    indexRange range = firstSelection->getFrameIndexRange(); // These are the frames that we want to cache
    qint64 itemSpaceNeeded = (range.second - range.first + 1) * firstSelection->getCachingFrameSize();
    qint64 alreadyCached = firstSelection->getCachedFrames().count()*firstSelection->getCachingFrameSize();
    itemSpaceNeeded -= alreadyCached;

    if (itemSpaceNeeded > cacheLevelMax && itemSpaceNeeded>0)
    {
      DEBUG_CACHING("videoCache::Item needs more space than cacheLevelMax");
      // All frames of the currently selected item will not fit into the cache
      // Delete all frames from all other items in the playlist from the cache and cache all frames from this item that fit
      foreach(playlistItem *item, allItems)
      {
        if (item != firstSelection)
          item->removeFrameFromCache(-1);
      }

      // Adjust the range so that only the number of frames are cached that will fit
      qint64 nrFramesCachable = cacheLevelMax / firstSelection->getCachingFrameSize();
      range.second = range.first + nrFramesCachable;

      // Enqueue the job. This is the only job.
      cacheQueue.enqueue( cacheJob(firstSelection, range) );
    }
    else if (itemSpaceNeeded > (cacheLevelMax - cacheLevel) && itemSpaceNeeded>0)
    {
      DEBUG_CACHING("videoCache::No enough space for caching, deleting frames");
      // There is currently not enough space in the cache to cache all remaining frames but in general the cache can hold all frames.
      // Delete frames from the cache until it fits.

      // We start from the item before the current item and go backwards in the list of all items.
      // The previous item is then last.
      int itemPos = allItems.indexOf(firstSelection);
      assert(itemPos >= 0); // The current item is not in the list of all items? No possible.
      int i = itemPos - 2;

      // Get the cache level without the current item (frames from the current item do not relly occupy space in the cache. We want to cache them anyways)
      qint64 cacheLevelWithoutCurrent = cacheLevel - firstSelection->getCachedFrames().count() * firstSelection->getCachingFrameSize();
      while ((itemSpaceNeeded + cacheLevelWithoutCurrent) > cacheLevelMax)
      {
        if (i < 0)
        {
          // There is no previous item or the previous item is the first one in the list
          i = allItems.count() - 1;
        }
        if (i == itemPos)
          // We went through the whole list and arrived back at the beginning. Got to the previous item.
          i--;
        if (allItems[i]->getCachedFrames().count() == 0)
        {
          i--;
          continue;  // Nothing to delete for this item
        }

        // Which frames are cached?
        QList<int> cachedFrames = allItems[i]->getCachedFrames();
        qint64 cachedFramesSize = cachedFrames.count() * allItems[i]->getCachingFrameSize();

        if ((itemSpaceNeeded + cacheLevelWithoutCurrent - cachedFramesSize) <= cacheLevelMax )
        {
          // If we delete all frames from item i, there is more than enough space. So we only delete as many frames as needed.
          qint64 additionalSpaceNeeded = itemSpaceNeeded - (cacheLevelMax - cacheLevelWithoutCurrent);
          qint64 nrFrames = additionalSpaceNeeded / allItems[i]->getCachingFrameSize() + 1;

          // Delete nrFrames frames from the back
          for (int f = cachedFrames.count() - 1; f >= 0 && nrFrames > 0 ; f--)
          {
            DEBUG_CACHING("videoCache::Deleting frame %d of %s from cache", f,  allItems[i]->getName().toLatin1().data() );
            allItems[i]->removeFrameFromCache(cachedFrames[f]);
            cacheLevelWithoutCurrent -= allItems[i]->getCachingFrameSize();
            if ((cacheLevelWithoutCurrent+itemSpaceNeeded)<=cacheLevelMax)
              break;
          }
        }
        else
        {
          // Deleting all frames from this item will ne be enough. Delete all and go on.
          DEBUG_CACHING("videoCache::Deleting all frames of %s from cache", allItems[i]->getName().toLatin1().data() );
          allItems[i]->removeFrameFromCache(-1);
          cacheLevelWithoutCurrent -= cachedFramesSize;
        }

        // Go to the next (previous) item
        i--;
      }

      // Enqueue the job. This is the only job.
      // We will not delete any frames from any other items to cache frames from other items.
      cacheQueue.enqueue( cacheJob(firstSelection, range) );
    }
    else
    {
      if (itemSpaceNeeded>0)
      {
        DEBUG_CACHING("videoCache::All frames of %s fit.", firstSelection->getName().toLatin1().data());
        // All frames from the current item will fit and there is probably even space for more items.
        // We don't delete any frames from the cache but we will cache as many items as possible.
        cacheQueue.enqueue( cacheJob(firstSelection, range) );
        cacheLevel = cacheLevel + itemSpaceNeeded;
      }
      // Continue caching with the next item
      int itemPos = allItems.indexOf(firstSelection);
      assert(itemPos >= 0); // The current item is not in the list of all items? No possible.
      int i = itemPos + 1;

      while (cacheLevel < cacheLevelMax)
      {
        DEBUG_CACHING("videoCache::Cache not full yet, attempting next item");
        // There is still space
        if (i >= allItems.count())
          i = 0;
        if (i == itemPos)
        {
          DEBUG_CACHING("videoCache::No more items to cache.");
          // No more items to cache
          break;
        }
        if (!allItems[i]->isCachable())
        {
          i++;
          continue; // Nothing to cache for this item
        }

        DEBUG_CACHING("videoCache::Attempt caching of next item %s.", allItems[i]->getName().toLatin1().data());
        // How much space is there in the cache (excluding what is cached from the current item)?
        // Get the cache level without the current item (frames from the current item do not relly occupy space in the cache. We want to cache them anyways)
        qint64 cacheLevelWithoutCurrent = cacheLevel - allItems[i]->getCachedFrames().count() * allItems[i]->getCachingFrameSize();
        // How much space do we need to cache the entire item?
        range = allItems[i]->getFrameIndexRange();
        qint64 itemCacheSize = (range.second - range.first + 1) * allItems[i]->getCachingFrameSize();

        if ((itemCacheSize + cacheLevelWithoutCurrent) < cacheLevelMax )
        {
          DEBUG_CACHING("videoCache::Entire next item %s fits.", allItems[i]->getName().toLatin1().data());
          // The entire item fits
          cacheQueue.enqueue( cacheJob(allItems[i], range) );
        }
        else
        {
          // Only a part of the item fits.
          qint64 nrFramesCachable = (cacheLevelMax - cacheLevelWithoutCurrent) / allItems[i]->getCachingFrameSize();
          DEBUG_CACHING("videoCache::Only %d frames of next item %s fit.",nrFramesCachable, allItems[i]->getName().toLatin1().data());
          range.second = range.first + nrFramesCachable;

          cacheQueue.enqueue( cacheJob(allItems[i], range) );

          break; // The cache is full
        }

        i++;
      }

    }
  }
  else
  {
    // Playback is running. The difference to the case where playback is not running is: If something has been played back,
    // we consider it least important for future playback.

  }

  //// Now walk through these items and fill the list of queue jobs.
  //// Consider the maximum size of the cache
  //qint64 cacheSize = 0;
  //bool cacheFull = false;
  //for (int i = 0; i < cachingItems.count() && !cacheFull; i++)
  //{
  //  playlistItem *item = cachingItems[i];
  //  indexRange range = item->getFrameIndexRange();
  //  if (range.first == -1 || range.second == -1)
  //    // Invalid range. Probably the item is not set up correctly (width/height not set). Skip this item
  //    continue;

  //  // Is there enough space in the cache for all frames?
  //  qint64 itemCacheSize = item->getCachingFrameSize() * (range.second - range.first);
  //  if (cacheSize + itemCacheSize > maxCacheSize)
  //  {
  //    // Not all frames of this item will fit. See how many frames will fit.
  //    qint64 remainingCacheSize = maxCacheSize - cacheSize;
  //    qint64 nrFramesCachable = remainingCacheSize / item->getCachingFrameSize();
  //    range.second = range.first + nrFramesCachable;

  //    // The cache is now full
  //    cacheFull = true;
  //  }

  //  // Add this item to the queue
  //  cacheQueue.enqueue( cacheJob(item, range) );
  //}

  // We filled the cacheQueue (as full as the cache size allows).
  // Now we have to check if we have to delete frames from already cached items.


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

void videoCache::updateCachingRate(unsigned int cacheRate)
{
  cacheRateInBytesPerMs = cacheRate;
}
