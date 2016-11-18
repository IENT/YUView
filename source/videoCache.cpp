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

#include <algorithm>
#include <QPainter>
#include <QSettings>
#include <QThread>
#include "playbackController.h"
#include "playlistItem.h"

#define CACHING_DEBUG_OUTPUT 0
#if CACHING_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_CACHING qDebug
#else
#define DEBUG_CACHING(fmt,...) ((void)0)
#endif

videoCache::cacheJob::cacheJob(playlistItem *item, indexRange range) :
  plItem(item),
  frameRange(range)
{
}

void videoCacheStatusWidget::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);

  if (!cache)
    return;

  // Draw
  QPainter painter(this);
  QSize s = size();

  // Get all items from the playlist
  QList<playlistItem*> allItems = playlist->getAllPlaylistItems();

  // Get if caching is enabled and how much memory we can use for the cache
  QSettings settings;
  settings.beginGroup("VideoCache");
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

class videoCache::cachingThread : public QThread
{
  Q_OBJECT
public:
  cachingThread(QObject *parent) : QThread(parent) { clearCacheJob(); }
  void run() Q_DECL_OVERRIDE;
  void setCacheJob(playlistItem *item, int frame) { plItem = item; frameToCache = frame; }
  void clearCacheJob() { plItem = NULL; frameToCache = -1; }
  playlistItem *getCacheItem() { return plItem; }
private:
  QPointer<playlistItem> plItem;
  int frameToCache;
};

void videoCache::cachingThread::run()
{
  if (plItem != NULL && frameToCache >= 0)
    // Just cache the frame that was given to us
    plItem->cacheFrame(frameToCache);

  // When we are done, we clear the variables and wait for the next job to be given to us
  frameToCache = -1;
  plItem = NULL;
}

videoCache::videoCache(PlaylistTreeWidget *playlistTreeWidget, PlaybackController *playbackController, QObject *parent)
  : QObject(parent)
{
  playlist = playlistTreeWidget;
  playback = playbackController;
  cacheRateInBytesPerMs = 0;

  connect(playlist, &PlaylistTreeWidget::playlistChanged, this, &videoCache::playlistChanged);
  connect(playlist, &PlaylistTreeWidget::itemAboutToBeDeleted, this, &videoCache::itemAboutToBeDeleted);

  // Create a bunch of new threads that we can use to cache frames in parallel
  for (int i = 0; i < QThread::idealThreadCount(); i++)
    cachingThreadList.append( new cachingThread(parent) );

  // Connect the signals/slots to communicate with the cacheWorker.
  for (int i = 0; i < cachingThreadList.count(); i++)
    connect(cachingThreadList.at(i), &QThread::finished, this, &videoCache::threadCachingFinished);

  workerState = workerIdle;
}

videoCache::~videoCache()
{
  // Delete all caching threads
  for (int i = 0; i < QThread::idealThreadCount(); i++)
    delete cachingThreadList[i];

  cachingThreadList.clear();
}

void videoCache::playlistChanged()
{
  // The playlist changed. We have to rethink what to cache next.
  DEBUG_CACHING("videoCache::playlistChanged");

  if (workerState == workerRunning)
  {
    // First the worker has to stop. Request a stop and an update of the queue.
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

  startCaching();
}

void videoCache::updateCacheQueue()
{
  // Now calculate the new list of frames to cache and run the cacher
  DEBUG_CACHING("videoCache::updateCacheQueue");

  // Get if caching is enabled and how much memory we can use for the cache
  QSettings settings;
  settings.beginGroup("VideoCache");
  bool cachingEnabled = settings.value("Enabled", true).toBool();
  cacheLevelMax = (qint64)settings.value("ThresholdValueMB", 49).toUInt() * 1024 * 1024;
  settings.endGroup();

  if (!cachingEnabled)
    return;  // Caching disabled

  // Firstly clear the old cache queues
  cacheQueue.clear();
  cacheDeQueue.clear();

  // Get all items from the playlist. There are two lists. For the caching status (how full is the cache) we have to consider
  // all items in the playlist. However, we only cache top level items and no child items.
  QList<playlistItem*> allItems = playlist->getAllPlaylistItems();
  QList<playlistItem*> allItemsTop = playlist->getAllPlaylistItems(true);

  // Remove all playlist items which do not allow caching
  std::remove_if(allItems.begin(), allItems.end(), [](playlistItem *item){ return !item->isCachable(); });
  std::remove_if(allItemsTop.begin(), allItemsTop.end(), [](playlistItem *item){ return !item->isCachable(); });

  if (allItemsTop.count() == 0)
    // No cachable items in the playlist.
    return;

  // At first, let's find out how much space in the cache is used.
  // In combination with cacheLevelMax we also know how much space is free.
  qint64 cacheLevel = 0;
  for (playlistItem *item : allItems)
  {
    cacheLevel += item->getCachedFrames().count() * item->getCachingFrameSize();
  }
  cacheLevelCurrent = cacheLevel;

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
    auto selection = playlist->getSelectedItems();
    if (selection[0] == NULL)
      selection[0] = allItems[0];

    // caching was requested for a non-cachable item.
    if (!selection[0]->isCachable())
      return;

    // How much space do we need to cache the entire item?
    indexRange range = selection[0]->getFrameIndexRange(); // These are the frames that we want to cache
    qint64 cachingFrameSize = selection[0]->getCachingFrameSize();
    qint64 itemSpaceNeeded = (range.second - range.first + 1) * cachingFrameSize;
    qint64 alreadyCached = selection[0]->getCachedFrames().count() * cachingFrameSize;
    itemSpaceNeeded -= alreadyCached;

    if (itemSpaceNeeded > cacheLevelMax && itemSpaceNeeded>0)
    {
      DEBUG_CACHING("videoCache::Item needs more space than cacheLevelMax");
      // All frames of the currently selected item will not fit into the cache
      // Delete all frames from all other items in the playlist from the cache and cache all frames from this item that fit
      for (playlistItem *item : allItems)
      {
        if (item != selection[0])
        {
          // Mark all frames of this item as "can be removed if required"
          QList<int> cachedFrames = item->getCachedFrames();
          for (int f : cachedFrames)
          {
            cacheDeQueue.enqueue(plItemFrame(item, f));
          }
        }
      }

      // Adjust the range so that only the number of frames are cached that will fit
      qint64 nrFramesCachable = cacheLevelMax / selection[0]->getCachingFrameSize();
      range.second = range.first + nrFramesCachable;

      // Enqueue the job. This is the only job.
      cacheQueue.enqueue( cacheJob(selection[0], range) );
    }
    else if (itemSpaceNeeded > (cacheLevelMax - cacheLevel) && itemSpaceNeeded>0)
    {
      DEBUG_CACHING("videoCache::No enough space for caching, deleting frames");
      // There is currently not enough space in the cache to cache all remaining frames but in general the cache can hold all frames.
      // Delete frames from the cache until it fits.

      // We start from the item before the current item and go backwards in the list of all items.
      // The previous item is then last.
      int itemPos = allItems.indexOf(selection[0]);
      assert(itemPos >= 0); // The current item is not in the list of all items? No possible.
      int i = itemPos - 2;

      // Get the cache level without the current item (frames from the current item do not relly occupy space in the cache. We want to cache them anyways)
      qint64 cacheLevelWithoutCurrent = cacheLevel - selection[0]->getCachedFrames().count() * selection[0]->getCachingFrameSize();
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
            cacheDeQueue.enqueue(plItemFrame(allItems[i], cachedFrames[f]));
            cacheLevelWithoutCurrent -= allItems[i]->getCachingFrameSize();
            if ((cacheLevelWithoutCurrent+itemSpaceNeeded)<=cacheLevelMax)
              break;
          }
        }
        else
        {
          // Deleting all frames from this item will not be enough.
          // Mark all frames of this item as "can be removed if required"
          QList<int> cachedFrames = allItems[i]->getCachedFrames();
          for (int f : cachedFrames)
          {
            cacheDeQueue.enqueue(plItemFrame(allItems[i], f));
          }
          cacheLevelWithoutCurrent -= cachedFramesSize;
        }

        // Go to the next (previous) item
        i--;
      }

      // Enqueue the job. This is the only job.
      // We will not delete any frames from any other items to cache frames from other items.
      cacheQueue.enqueue( cacheJob(selection[0], range) );
    }
    else
    {
      if (itemSpaceNeeded>0)
      {
        DEBUG_CACHING("videoCache::All frames of %s fit.", selection[0]->getName().toLatin1().data());
        // All frames from the current item will fit and there is probably even space for more items.
        // We don't delete any frames from the cache but we will cache as many items as possible.
        cacheQueue.enqueue( cacheJob(selection[0], range) );
        cacheLevel = cacheLevel + itemSpaceNeeded;
      }
      // Continue caching with the next item
      int itemPos = allItems.indexOf(selection[0]);
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

#if CACHING_DEBUG_OUTPUT
  if (!cacheQueue.isEmpty())
  {
    qDebug("updateCacheQueue summary -- cache:");
    for (const cacheJob &j : cacheQueue)
    {
      QString itemStr = j.plItem->getName();
      itemStr.append(" - ");
      itemStr.append(QString::number(j.frameRange.first) + "-" + QString::number(j.frameRange.second));
      qDebug() << itemStr;
    }
  }
  if (!cacheDeQueue.isEmpty())
  {
    qDebug("updateCacheQueue summary -- deQueue:");
    playlistItem *lastItem = NULL;
    QString itemStr;
    for (const plItemFrame &f : cacheDeQueue)
    {
      if (f.first != lastItem)
      {
        // New item
        if (lastItem != NULL)
          // Print the last items frames
          qDebug() << itemStr;
        lastItem = f.first;
        itemStr = lastItem->getName();
        itemStr.append(" - ");
      }
      else
      {
        // Same ite. Just append frame number
        itemStr.append(" " + QString::number(f.second));
      }
    }
    qDebug() << itemStr;
  }
#endif
}

void videoCache::startCaching()
{
  DEBUG_CACHING("videoCache::startCaching");

  if (cacheQueue.isEmpty())
  {
    // Nothing in the queue to start caching for.
    workerState = workerIdle;
  }
  else
  {
    // Push a task to all the threads and start them.
    for (int i = 0; i < cachingThreadList.count(); i++)
    {
      pushNextJobToThread(cachingThreadList[i]);
      cachingThreadList[i]->start();
    }

    workerState = workerRunning;
  }
}

// One of the threads is done with it's caching operation. Give it a new task if there is one and we are not
// breaking the caching process.
void videoCache::threadCachingFinished()
{
  DEBUG_CACHING("videoCache::threadCachingFinished - state %d", workerState);

  // Get the thread that caused this call
  QObject *sender = QObject::sender();
  cachingThread *thread = dynamic_cast<cachingThread*>(sender);
  thread->clearCacheJob();

  // Check the list of items that are sheduled for deletion. Because a thread finished, maybe now we can delete the item(s).
  for (auto it = itemsToDelete.begin(); it != itemsToDelete.end(); )
  {
    bool itemCaching = false;
    for (cachingThread *t : cachingThreadList)
      if (t->getCacheItem() == *it)
      {
        itemCaching = true;
        break;
      }

    if (!itemCaching)
    {
      // Delete the item and remove it from the itemsToDelete list
      (*it)->deleteLater();
      it = itemsToDelete.erase(it);
    } else
      ++it;
  }

  if (workerState == workerRunning)
    // Push the next job to the cache
    pushNextJobToThread(thread);

  // Check if all threads have stopped.
  bool jobsRunning = false;
  for (int i = 0; i < cachingThreadList.count(); i++)
  {
    if (cachingThreadList[i]->isRunning())
      // A job is still running. Wait.
      jobsRunning = true;
  }

  if (!jobsRunning)
  {
    // All jobs are done
    DEBUG_CACHING("videoCache::threadCachingFinished - All jobs done");
    if (workerState == workerIntReqStop || workerState == workerRunning)
      workerState = workerIdle;
    else if (workerState == workerIntReqRestart)
    {
      updateCacheQueue();
      startCaching();
    }
  }

  DEBUG_CACHING("videoCache::threadCachingFinished - new state %d", workerState);
}

bool videoCache::pushNextJobToThread(cachingThread *thread)
{
  if (cacheQueue.isEmpty())
    // No more jobs in the cache queue. Nothing further to cache.
    return false;

  // Get the top item from the queue but don't remove it yet.
  playlistItem *plItem   = cacheQueue.head().plItem;
  indexRange    range    = cacheQueue.head().frameRange;
  unsigned int frameSize = plItem->getCachingFrameSize();

  // Remove all the items from the queue that are not cachable (anymore)
  while (!plItem->isCachable())
  {
    cacheQueue.dequeue();

    if (cacheQueue.isEmpty())
      // No more items.
      return false;

    plItem = cacheQueue.head().plItem;
    range  = cacheQueue.head().frameRange;
    frameSize = plItem->getCachingFrameSize();
  }

  // We found an item. Cache the first frame of it.
  int frameToCache = range.first;

  // Update the cache queue
  if (range.first == range.second)
    // All frames of the item are now cached.
    cacheQueue.dequeue();
  else
    // Update the frame range of the head item in the cache queue
    cacheQueue.head().frameRange.first = range.first + 1;

  // First check if we need to free up space to cache this frame.
  while (cacheLevelCurrent + frameSize > cacheLevelMax && !cacheDeQueue.isEmpty())
  {
    plItemFrame frameToRemove = cacheDeQueue.dequeue();
    unsigned int frameToRemoveSize = frameToRemove.first->getCachingFrameSize();

    DEBUG_CACHING("Remove frame %d of %s", frameToRemove.second, frameToRemove.first->getName().toStdString().c_str());
    frameToRemove.first->removeFrameFromCache(frameToRemove.second);
    cacheLevelCurrent -= frameToRemoveSize;
  }

  if (cacheDeQueue.isEmpty() && cacheLevelCurrent + frameSize > cacheLevelMax)
  {
    // There is still not enough space but there are no more frames that we can remove.
    // The updateCacheQueue function should never create a situation where this is possible ...
    // We are done here.
    return false;
  }

  // Cache the frame
  QString tmp = plItem->getName();
  DEBUG_CACHING("videoCache::pushNextJobToThread - %d of %s", frameToCache, plItem->getName().toStdString().c_str());

  // Push the job to the thread
  thread->setCacheJob(plItem, frameToCache);
  thread->start();

  return true;
}

void videoCache::itemAboutToBeDeleted(playlistItem* item)
{
  // One of the items is about to be deleted. Let's stop the caching. Then the item can be deleted
  // and then we can re-think our caching strategy.
  if (workerState != workerIdle)
  {
    // Are we currently caching a frame from this item?
    bool cachingItem = false;
    for (int i = 0; i < cachingThreadList.count(); i++)
    {
      if (cachingThreadList[i]->getCacheItem() == item)
        cachingItem = true;
    }

    if (cachingItem)
      // The item can be deleted when all caching threads of the item returned.
      itemsToDelete.append(item);
    else
      // The item can be deleted now.
      item->deleteLater();

    workerState = workerIntReqRestart;
  }
  else
  {
    // The worker thread is idle. We can just delete the item (late).
    item->deleteLater();
  }
}

void videoCache::updateCachingRate(unsigned int cacheRate)
{
  cacheRateInBytesPerMs = cacheRate;
}

#include "videoCache.moc"
