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

#define CACHING_DEBUG_OUTPUT 1
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

class videoCache::loadingWorker : public QObject
{
  Q_OBJECT
public:
  loadingWorker(QObject *parent) : QObject(parent) { currentCacheItem = nullptr; working = false; }
  playlistItem *getCacheItem() { return currentCacheItem; }
  void setJob(playlistItem *item, int frame) { currentCacheItem = item; currentFrame = frame; }
  void setWorking(bool state) { working = state; }
  bool isWorking() { return working; }
  // Process the job in the thread that this worker was moved to. This function can be directly
  // called from the main thread. It will still process the call in the separate thread.
  void processCacheJob() { QMetaObject::invokeMethod(this, "processCacheJobInternal"); }
  void processLoadingJob() { QMetaObject::invokeMethod(this, "processLoadingJobInternal"); }
signals:
  void loadingFinished();
private slots:
  void processCacheJobInternal();
  void processLoadingJobInternal();
private:
  playlistItem *currentCacheItem;
  int currentFrame;
  bool working;
};

void videoCache::loadingWorker::processCacheJobInternal()
{
  if (currentCacheItem != nullptr && currentFrame >= 0)
    // Just cache the frame that was given to us.
    // This is performed in the thread that this worker is currently placed in.
    currentCacheItem->cacheFrame(currentFrame);

  emit loadingFinished();
  currentCacheItem = nullptr;
}

void videoCache::loadingWorker::processLoadingJobInternal()
{
  if (currentCacheItem != nullptr && currentFrame >= 0)
    // Load the frame of the item that was given to us.
    // This is performed in the thread (the loading thread with higher priority.
    currentCacheItem->loadFrame(currentFrame);

  emit loadingFinished();
  currentCacheItem = nullptr;
}

// ------- Video Cache ----------

videoCache::videoCache(PlaylistTreeWidget *playlistTreeWidget, PlaybackController *playbackController, QObject *parent)
  : QObject(parent)
{
  playlist = playlistTreeWidget;
  playback = playbackController;
  cacheRateInBytesPerMs = 0;
  deleteNrThreads = 0;
  interactiveItemQueued = nullptr;
  interactiveItemQueued_Idx = -1;

  // Update some values from the QSettings. This will also create the correct number of threads.
  updateSettings();

  // Create the interactive threads
  interactiveWorker = new loadingWorker(nullptr);
  interactiveWorkerThread = new QThread(this);
  interactiveWorker->moveToThread(interactiveWorkerThread);
  interactiveWorkerThread->start(QThread::HighPriority);
  connect(interactiveWorker, &loadingWorker::loadingFinished, this, &videoCache::interactiveLoaderFinished);

  connect(playlist, &PlaylistTreeWidget::playlistChanged, this, &videoCache::playlistChanged);
  connect(playlist, &PlaylistTreeWidget::itemAboutToBeDeleted, this, &videoCache::itemAboutToBeDeleted);

  workerState = workerIdle;
}

videoCache::~videoCache()
{
  // Stop all threads before destroying them
  for (QThread *t : cachingThreadList)
  {
    t->exit();
  }
}

void videoCache::startWorkerThreads(int nrThreads)
{
  for (int i = 0; i < nrThreads; i++)
  {
    QThread *newThread = new QThread(this);
    cachingThreadList.append(newThread);

    loadingWorker *newWorker = new loadingWorker(nullptr);
    newWorker->moveToThread(newThread);
    cachingWorkerList.append(newWorker);
    // Caching should run in the background without interrupting normal operation. Start with lowest priority.
    newThread->start(QThread::LowestPriority);

    // Connect the signals/slots to communicate with the cacheWorker.
    connect(newWorker, &loadingWorker::loadingFinished, this, &videoCache::threadCachingFinished);

    DEBUG_CACHING("Started thread %p with worker %p", newThread, newWorker);

    if (workerState == workerRunning)
      // Push the next job to the worker. Otherwise it will not start working if caching is currently running.
      pushNextJobToThread(newWorker);
  }
}

void videoCache::updateSettings()
{
  // Get if caching is enabled and how much memory we can use for the cache
  QSettings settings;
  settings.beginGroup("VideoCache");
  cachingEnabled = settings.value("Enabled", true).toBool();
  cacheLevelMax = (qint64)settings.value("ThresholdValueMB", 49).toUInt() * 1024 * 1024;
  
  // See if the user changed the number of threads
  int targetNrThreads = getOptimalThreadCount();
  if (settings.value("SetNrThreads", false).toBool())
  {
    targetNrThreads = settings.value("NrThreads", targetNrThreads).toInt();
  }
  if (targetNrThreads <= 0)
    targetNrThreads = 1;
  if (!cachingEnabled)
    targetNrThreads = 0;

  if (targetNrThreads > cachingThreadList.count())
    // Create new threads
    startWorkerThreads(targetNrThreads - cachingThreadList.count());
  else if (targetNrThreads < cachingThreadList.count())
  {
    // Remove threads. We can only delete workers (and their threads) that are currently not working.
    int nrThreadsToRemove = cachingThreadList.count() - targetNrThreads;

    for (int i = cachingWorkerList.count()-1; i >= 0  && nrThreadsToRemove > 0; i--)
    {
      if (!cachingWorkerList[i]->isWorking())
      {
        // Not working -> delete it now
        loadingWorker *w = cachingWorkerList.takeAt(i);
        w->deleteLater();
        QThread *t = cachingThreadList.takeAt(i);
        t->exit();
        t->deleteLater();

        DEBUG_CACHING("Deleting thread %p with worker %p", t, w);
        nrThreadsToRemove --;
      }
    }

    if (nrThreadsToRemove > 0)
    {
      // We need to remove more threads but the workers in these threads are still running. Do this when the workers finish.
      DEBUG_CACHING("Deleting %d threads later", nrThreadsToRemove);
      deleteNrThreads = nrThreadsToRemove;
    }
  }

  settings.endGroup();
}

void videoCache::loadFrame(playlistItem * item, int frameIndex)
{
  if (interactiveWorker->isWorking())
  {
    // The interactive worker is currently busy. Schedule this load request as the next one.
    DEBUG_CACHING("videoCache::loadFrame %d queued for later", frameIndex);
    interactiveItemQueued = item;
    interactiveItemQueued_Idx = frameIndex;
  }
  else
  {
    // Let the interactive worker work...
    interactiveWorker->setJob(item, frameIndex);
    interactiveWorker->processLoadingJob();
    DEBUG_CACHING("videoCache::loadFrame %d started", frameIndex);
  }
}

void videoCache::interactiveLoaderFinished()
{
  // The worker finished. Is there another loading request in the queue?
  if (interactiveItemQueued && interactiveItemQueued_Idx != -1)
  {
    // Let the interactive worker work on the queued request.
    interactiveWorker->setJob(interactiveItemQueued, interactiveItemQueued_Idx);
    interactiveWorker->processLoadingJob();
    DEBUG_CACHING("videoCache::interactiveLoaderFinished %d started", interactiveItemQueued_Idx);

    interactiveItemQueued = nullptr;
    interactiveItemQueued_Idx = -1;
  }
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
    // The worker is still running but we already requested an interrupt and an update of the queue.
    return;
  }

  assert(workerState == workerIdle);
  if (cachingEnabled)
  {
    // Update the cache queue and restart the worker.
    updateCacheQueue();
    startCaching();
  }
}

void videoCache::updateCacheQueue()
{
  if (!cachingEnabled)
    return;  // Caching disabled

  // Now calculate the new list of frames to cache and run the cacher
  DEBUG_CACHING("videoCache::updateCacheQueue");

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
    // 1: Cache all the frames in the item that is currently selected. In order to achieve this, we will aggressively
    //    delete other frames from other sequences in the cache. This has highest priority.
    // 2: Cache all the frames from the following items (while there is space left in the cache). We will not remove any frames
    //    from other items from the cache to achieve this. This is not the highest priority.
    //
    // When frames have to be removed to fit the selected sequence into the cache, the following priorities apply to frames from
    // other sequences. (The ones with highest priority get removed last)
    // 1: The frames from the previous item have highest priority and are removed last. It is very likely that in 'interactive'
    //    (playback is not running) mode, the user will go back to the previous item.
    // 2: The item after this item is next in the priority list.
    // 3: The item after 2 is next and so on (wrap around in the playlist) until the previous item is reached.

    // Let's start with the currently selected item (if no item is selected, the first item in the playlist is considered as being selected)
    auto selection = playlist->getSelectedItems();
    if (selection[0] == nullptr)
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

      // Get the cache level without the current item (frames from the current item do not really occupy space in the cache. We want to cache them anyways)
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
        // Get the cache level without the current item (frames from the current item do not really occupy space in the cache. We want to cache them anyways)
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
    playlistItem *lastItem = nullptr;
    QString itemStr;
    for (const plItemFrame &f : cacheDeQueue)
    {
      if (f.first != lastItem)
      {
        // New item
        if (lastItem != nullptr)
          // Print the last items frames
          qDebug() << itemStr;
        lastItem = f.first;
        itemStr = lastItem->getName();
        itemStr.append(" - ");
      }
      else
      {
        // Same item. Just append frame number
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
    for (int i = 0; i < cachingWorkerList.count(); i++)
    {
      pushNextJobToThread(cachingWorkerList[i]);
    }

    workerState = workerRunning;
  }
}

// One of the workers is done with it's caching operation. Give it a new task if there is one and we are not
// breaking the caching process.
void videoCache::threadCachingFinished()
{
  // Get the thread that caused this call
  QObject *sender = QObject::sender();
  loadingWorker *worker = dynamic_cast<loadingWorker*>(sender);
  worker->setWorking(false);
  DEBUG_CACHING("videoCache::threadCachingFinished - state %d - worker %p", workerState, worker);

  // Check the list of items that are scheduled for deletion. Because a thread finished, maybe now we can delete the item(s).
  for (auto it = itemsToDelete.begin(); it != itemsToDelete.end(); )
  {
    bool itemCaching = false;
    for (loadingWorker *w : cachingWorkerList)
      if (w->getCacheItem() == *it)
      {
        itemCaching = true;
        break;
      }

    if (!itemCaching)
    {
      // Delete the item and remove it from the itemsToDelete list
      (*it)->deleteLater();
      it = itemsToDelete.erase(it);
    } 
    else
      ++it;
  }

  // Also check if the worker is in the cachingWorkerList. If not, do not push a new job to it.
  int idx = cachingWorkerList.indexOf(worker);
  Q_ASSERT_X(idx >= 0, Q_FUNC_INFO, "Worker not in the list. All workers must be in the list.");
  if (deleteNrThreads > 0)
  {
    // We need to delete some threads. So this one has to go.
    loadingWorker *w = cachingWorkerList.takeAt(idx);
    w->deleteLater();
    QThread *t = cachingThreadList.takeAt(idx);
    t->exit();
    t->deleteLater();

    DEBUG_CACHING("Deleting thread %p with worker %p", t, w);
    deleteNrThreads--;
  }
  else if (workerState == workerRunning)
    // Push the next job to the cache
    pushNextJobToThread(worker);

  // Check if all threads have stopped.
  bool jobsRunning = false;
  for (loadingWorker *w : cachingWorkerList)
  {
    DEBUG_CACHING("WorkerList - worker %p - working %d", w, w->isWorking());
    if (w->isWorking())
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

bool videoCache::pushNextJobToThread(loadingWorker *worker)
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

  // Push the job to the thread
  worker->setJob(plItem, frameToCache);
  worker->setWorking(true);
  worker->processCacheJob();
  DEBUG_CACHING("videoCache::pushNextJobToThread - %d of %s - worker %p", frameToCache, plItem->getName().toStdString().c_str(), worker);

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
    for (int i = 0; i < cachingWorkerList.count(); i++)
    {
      if (cachingWorkerList[i]->getCacheItem() == item)
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
