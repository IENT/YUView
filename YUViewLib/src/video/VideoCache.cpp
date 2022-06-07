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

#include "VideoCache.h"

#include <QMessageBox>
#include <QPainter>
#include <QScrollArea>
#include <QSettings>
#include <QThread>
#include <algorithm>

#include <common/Functions.h>
#include <playlistitem/playlistItem.h>
#include <ui/PlaybackController.h>

namespace video
{

// This debug setting has two values:
// 1: Basic operation is written to qDebug: If a new item is selected, what is the decision to
// cache/remove next?
//    When is caching of a frame started?
// 2: Show all details. What are the threads doing when? What is removed when? ...
#define CACHING_DEBUG_OUTPUT 0
#if CACHING_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_CACHING qDebug
#if CACHING_DEBUG_OUTPUT == 2
#define DEBUG_CACHING_DETAIL qDebug
#else
#define DEBUG_CACHING_DETAIL(fmt, ...) ((void)0)
#endif
#else
#define DEBUG_CACHING(fmt, ...) ((void)0)
#define DEBUG_CACHING_DETAIL(fmt, ...) ((void)0)
#endif

#define CACHING_THREAD_JOBS_OUTPUT 0
#if CACHING_THREAD_JOBS_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_JOBS qDebug
#else
#define DEBUG_JOBS(fmt, ...) ((void)0)
#endif

/// ------------------------ loadingWorker ------------------------

class loadingWorker : public QObject
{
  Q_OBJECT
public:
  loadingWorker(QObject *parent) : QObject(parent)
  {
    currentCacheItem = nullptr;
    working          = false;
    id               = id_counter++;
  }
  playlistItem *getCacheItem() { return currentCacheItem; }
  int           getCacheFrame() { return currentFrame; }
  void          setJob(playlistItem *item, int frame, bool test = false);
  void          setWorking(bool state) { working = state; }
  bool          isWorking() { return working; }
  QString       getStatus()
  {
    return QString("T%1: %2").arg(id).arg(working ? QString::number(currentFrame) : QString("-"));
  }
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
  playlistItem *currentCacheItem;
  int           currentFrame;
  bool          working;
  bool          testMode;
  int           id; // A static ID of the thread. Only used in getStatus().
  static int    id_counter;
};
// Initially this is 0. The threads will number themselves so that there are never two threads with
// the same id
int loadingWorker::id_counter = 0;

void loadingWorker::setJob(playlistItem *item, int frame, bool test)
{
  Q_ASSERT_X(item != nullptr, Q_FUNC_INFO, "Given item is nullptr");
  Q_ASSERT_X(frame >= 0 || !item->properties().isIndexedByFrame(),
             Q_FUNC_INFO,
             "Given frame index invalid");
  currentCacheItem = item;
  currentFrame     = frame;
  testMode         = test;
}

void loadingWorker::processCacheJob()
{
  DEBUG_JOBS("loadingWorker::processCacheJob invoke processCacheJobInternal");
  QMetaObject::invokeMethod(this, "processCacheJobInternal");
}

void loadingWorker::processLoadingJob(bool playing, bool loadRawData)
{
  DEBUG_JOBS("loadingWorker::processLoadingJob invoke processLoadingJobInternal");
  QMetaObject::invokeMethod(
      this, "processLoadingJobInternal", Q_ARG(bool, playing), Q_ARG(bool, loadRawData));
}

void loadingWorker::processCacheJobInternal()
{
  Q_ASSERT_X(currentCacheItem != nullptr, Q_FUNC_INFO, "Invalid Job - Item is nullptr");
  Q_ASSERT_X(currentFrame >= 0 || !currentCacheItem->properties().isIndexedByFrame(),
             Q_FUNC_INFO,
             "Given frame index invalid");
  DEBUG_JOBS("loadingWorker::processCacheJobInternal");

  // Just cache the frame that was given to us.
  // This is performed in the thread that this worker is currently placed in.
  currentCacheItem->cacheFrame(currentFrame, testMode);

  currentCacheItem = nullptr;
  DEBUG_JOBS("loadingWorker::processCacheJobInternal emit loadingFinished");
  emit loadingFinished();
}

void loadingWorker::processLoadingJobInternal(bool playing, bool loadRawData)
{
  Q_ASSERT_X(currentCacheItem != nullptr, Q_FUNC_INFO, "The set job is nullptr");
  Q_ASSERT_X((!currentCacheItem->properties().isIndexedByFrame() || currentFrame >= 0),
             Q_FUNC_INFO,
             "The set frame index is invalid");
  Q_ASSERT_X(
      !currentCacheItem->taggedForDeletion(), Q_FUNC_INFO, "The set job was tagged for deletion");
  DEBUG_JOBS(Q_FUNC_INFO);

  // Load the frame of the item that was given to us.
  // This is performed in the thread (the loading thread with higher priority.
  currentCacheItem->loadFrame(currentFrame, playing, loadRawData);

  currentCacheItem = nullptr;
  emit loadingFinished();
  DEBUG_JOBS("loadingWorker::processLoadingJobInternal emit loadingFinished");
}

/// -------------------------- VideoCache::loadingThread --------------------

class VideoCache::loadingThread : public QThread
{
  Q_OBJECT
public:
  loadingThread(QObject *parent) : QThread(parent)
  {
    // Create a new worker and move it to this thread
    threadWorker.reset(new loadingWorker(nullptr));
    threadWorker->moveToThread(this);
    quitting = false;
  }
  void quitWhenDone()
  {
    quitting = true;
    if (threadWorker->isWorking())
    {
      // We must wait until the worker is done.
      DEBUG_CACHING("loadingThread::quitWhenDone waiting for worker to finish...");
      connect(worker(), &loadingWorker::loadingFinished, this, [=] {
        DEBUG_CACHING("loadingThread::quitWhenDone worker done -> quit");
        quit();
      });
    }
    else
    {
      DEBUG_CACHING("loadingThread::quitWhenDone quit now");
      quit();
    }
  }
  loadingWorker *worker() { return threadWorker.data(); }
  bool           isQuitting() { return quitting; }

private:
  QScopedPointer<loadingWorker> threadWorker;
  bool quitting; // Are er quitting the job? If yes, do not push new jobs to it.
};

/// ---------------------------------- VideoCache ------------------------------

VideoCache::VideoCache(PlaylistTreeWidget *playlistTreeWidget,
                       PlaybackController *playbackController,
                       splitViewWidget *   view,
                       QWidget *           parent)
    : QObject(parent)
{
  playlist     = playlistTreeWidget;
  playback     = playbackController;
  splitView    = view;
  parentWidget = parent;

  // Create the interactive threads
  for (int i = 0; i < 2; i++)
  {
    interactiveThread[i] = new loadingThread(this);
    interactiveThread[i]->start(QThread::HighPriority);
    connect(interactiveThread[i]->worker(),
            &loadingWorker::loadingFinished,
            this,
            &VideoCache::interactiveLoaderFinished);

    // Clear the slots for queued jobs
    interactiveItemQueued[i]     = nullptr;
    interactiveItemQueued_Idx[i] = -1;
  }

  // Update some values from the QSettings. This will also create the correct number of threads.
  updateSettings();

  connect(playlist.data(),
          &PlaylistTreeWidget::playlistChanged,
          this,
          &VideoCache::scheduleCachingListUpdate);
  connect(playlist.data(),
          &PlaylistTreeWidget::itemAboutToBeDeleted,
          this,
          &VideoCache::itemAboutToBeDeleted);
  connect(
      playlist.data(), &PlaylistTreeWidget::signalItemRecache, this, &VideoCache::itemNeedsRecache);
  connect(playback.data(),
          &PlaybackController::waitForItemCaching,
          this,
          &VideoCache::watchItemForCachingFinished);
  connect(playback.data(),
          &PlaybackController::signalPlaybackStarting,
          this,
          &VideoCache::updateCacheQueue);
  connect(&statusUpdateTimer, &QTimer::timeout, this, [=] { emit updateCacheStatus(); });
  connect(&testProgrssUpdateTimer, &QTimer::timeout, this, [=] { updateTestProgress(); });
}

VideoCache::~VideoCache()
{
  DEBUG_CACHING("VideoCache::~VideoCache Terminate all workers and threads");

  // Tell all threads to quit
  for (loadingThread *t : cachingThreadList)
    t->quitWhenDone();
  interactiveThread[0]->quitWhenDone();
  interactiveThread[1]->quitWhenDone();

  // Wait for the threads to quit, otherwise terminate them
  for (int i = 0; i < 2; i++)
    if (!interactiveThread[i]->wait(3000))
    {
      interactiveThread[i]->terminate();
      interactiveThread[i]->wait();
    }
  for (loadingThread *t : cachingThreadList)
    if (!t->wait(3000))
    {
      t->terminate();
      t->wait();
    }

  // Delete all threads
  for (loadingThread *t : cachingThreadList)
    t->deleteLater();
}

void VideoCache::startWorkerThreads(int nrThreads)
{
  for (int i = 0; i < nrThreads; i++)
  {
    loadingThread *newThread = new loadingThread(this);
    cachingThreadList.append(newThread);

    // Caching should run in the background without interrupting normal operation. Start with lowest
    // priority.
    newThread->start(QThread::LowestPriority);

    // Connect the signals/slots to communicate with the cacheWorker.
    connect(newThread->worker(),
            &loadingWorker::loadingFinished,
            this,
            &VideoCache::threadCachingFinished);

    DEBUG_CACHING("VideoCache::startWorkerThreads Started thread %p", newThread);

    if (workersState == workersRunning)
      // Push the next job to the worker. Otherwise it will not start working if caching is
      // currently running.
      pushNextJobToCachingThread(newThread);
  }
}

void VideoCache::updateSettings()
{
  DEBUG_CACHING("VideoCache::updateSettings");

  // Get if caching is enabled and how much memory we can use for the cache
  QSettings settings;
  settings.beginGroup("VideoCache");
  cachingEnabled = settings.value("Enabled", true).toBool();
  cacheLevelMax  = (int64_t)settings.value("ThresholdValueMB", 49).toUInt() * 1000 * 1000;

  // See if the user changed the number of threads
  int targetNrThreads = functions::getOptimalThreadCount();
  if (settings.value("SetNrThreads", false).toBool())
    targetNrThreads = settings.value("NrThreads", targetNrThreads).toInt();
  if (targetNrThreads <= 0)
    targetNrThreads = 1;
  if (!cachingEnabled)
    targetNrThreads = 0;

  // How many threads should be used when playback is running?
  if (settings.value("PlaybackCachingEnabled", false).toBool())
    nrThreadsPlayback = settings.value("PlaybackCachingThreadLimit", 1).toInt();
  else
    nrThreadsPlayback = 0;

  if (targetNrThreads > cachingThreadList.count())
    // Create new threads
    startWorkerThreads(targetNrThreads - cachingThreadList.count());
  else if (targetNrThreads < cachingThreadList.count())
  {
    // Remove threads. We can only delete workers (and their threads) that are currently not
    // working.
    int nrThreadsToRemove = cachingThreadList.count() - targetNrThreads;

    for (int i = cachingThreadList.count() - 1; i >= 0 && nrThreadsToRemove > 0; i--)
    {
      if (!cachingThreadList[i]->worker()->isWorking())
      {
        // Not working -> delete it now
        QThread *t = cachingThreadList.takeAt(i);
        t->exit();
        t->deleteLater();

        DEBUG_CACHING("VideoCache::updateSettings Deleting thread %p with worker %d", t, i);
        nrThreadsToRemove--;
      }
    }

    if (nrThreadsToRemove > 0)
    {
      // We need to remove more threads but the workers in these threads are still running. Do this
      // when the workers finish.
      DEBUG_CACHING("VideoCache::updateSettings Deleting %d threads later", nrThreadsToRemove);
      deleteNrThreads = nrThreadsToRemove;
    }
  }

  // Also update the cache status and schedule an update of the caching.
  emit updateCacheStatus();
  scheduleCachingListUpdate();

  settings.endGroup();
}

void VideoCache::loadFrame(playlistItem *item, int frameIndex, int loadingSlot)
{
  if (item == nullptr || item->taggedForDeletion() ||
      (frameIndex < 0 && item->properties().isIndexedByFrame()))
    // The item is not loadable (invalid, tagged for deletion, and invalid frame index was given)
    return;

  assert(loadingSlot == 0 || loadingSlot == 1);
  if (interactiveThread[loadingSlot]->worker()->isWorking())
  {
    // The interactive worker is currently busy ...
    if (interactiveThread[loadingSlot]->worker()->getCacheItem() != item ||
        interactiveThread[loadingSlot]->worker()->getCacheFrame() != frameIndex)
    {
      // ... and it is not working on the requested frame. Schedule this load request as the next
      // one.
      DEBUG_CACHING_DETAIL(
          "VideoCache::loadFrame %d queued for later - slot %d", frameIndex, loadingSlot);
      interactiveItemQueued[loadingSlot]     = item;
      interactiveItemQueued_Idx[loadingSlot] = frameIndex;
    }
  }
  else
  {
    // Let the interactive worker work...
    bool loadRawData = splitView->showRawData() && !playback->playing();
    interactiveThread[loadingSlot]->worker()->setJob(item, frameIndex);
    interactiveThread[loadingSlot]->worker()->setWorking(true);
    interactiveThread[loadingSlot]->worker()->processLoadingJob(playback->playing(), loadRawData);
    DEBUG_CACHING_DETAIL("VideoCache::loadFrame %d started - slot %d", frameIndex, loadingSlot);

    emit updateCacheStatus();
  }
}

void VideoCache::interactiveLoaderFinished()
{
  // Get the thread that caused this call
  QObject *      sender   = QObject::sender();
  loadingWorker *worker   = dynamic_cast<loadingWorker *>(sender);
  int            threadID = (interactiveThread[0]->worker() == worker) ? 0 : 1;
  assert(worker == interactiveThread[0]->worker() || worker == interactiveThread[1]->worker());

  // Check the list of items that are scheduled for deletion. Because a loading thread finished,
  // maybe now we can delete the item(s).
  for (auto it = itemsToDelete.begin(); it != itemsToDelete.end();)
  {
    // Is the item still being cached?
    bool itemCaching = false;
    for (loadingThread *t : cachingThreadList)
      if (t->worker()->getCacheItem() == *it)
      {
        itemCaching = true;
        break;
      }
    // Is the item still being loaded?
    bool loadingItem = (interactiveThread[0]->worker()->getCacheItem() == *it ||
                        interactiveThread[1]->worker()->getCacheItem() == *it);

    if (!itemCaching && !loadingItem)
    {
      // Remove the item from the loading queue (if in there)
      if (interactiveItemQueued[threadID] == (*it))
      {
        interactiveItemQueued[threadID]     = nullptr;
        interactiveItemQueued_Idx[threadID] = -1;
      }
      // Delete the item and remove it from the itemsToDelete list
      DEBUG_CACHING("VideoCache::interactiveLoaderFinished delete item now %s",
                    (*it)->getName().toLatin1().data());
      (*it)->deleteLater();
      it = itemsToDelete.erase(it);
    }
    else
      ++it;
  }

  // The worker finished. Is there another loading request in the queue?
  if (interactiveItemQueued[threadID] != nullptr && interactiveItemQueued_Idx[threadID] >= 0)
  {
    // Let the interactive worker work on the queued request.
    bool loadRawData = splitView->showRawData() && !playback->playing();
    interactiveThread[threadID]->worker()->setJob(interactiveItemQueued[threadID],
                                                  interactiveItemQueued_Idx[threadID]);
    interactiveThread[threadID]->worker()->setWorking(true);
    interactiveThread[threadID]->worker()->processLoadingJob(playback->playing(), loadRawData);
    DEBUG_CACHING_DETAIL("VideoCache::interactiveLoaderFinished %d started - slot %d",
                         interactiveItemQueued_Idx[threadID],
                         threadID);

    // Clear the queue slot
    interactiveItemQueued[threadID]     = nullptr;
    interactiveItemQueued_Idx[threadID] = -1;
  }
  else
    // No scheduled job waiting
    interactiveThread[threadID]->worker()->setWorking(false);

  emit updateCacheStatus();
}

void VideoCache::scheduleCachingListUpdate()
{
  // The playlist changed. We have to rethink what to cache next.
  if (workersState == workersRunning)
  {
    // First, the worker has to stop. Request a stop and an update of the queue.
    workersState = workersIntReqRestart;
    DEBUG_CACHING("VideoCache::playlistChanged new state %d (workersIntReqRestart)", workersState);
    return;
  }
  else if (workersState == workersIntReqRestart)
  {
    // The worker is still running but we already requested an interrupt and an update of the queue.
    DEBUG_CACHING("VideoCache::playlistChanged new state %d (workersIntReqRestart)", workersState);
    return;
  }

  assert(workersState == workersIdle);
  if (cachingEnabled)
  {
    // Update the cache queue and restart the worker.
    updateCacheQueue();
    startCaching();
  }
  DEBUG_CACHING("VideoCache::playlistChanged new state %d", workersState);
}

void VideoCache::updateCacheQueue()
{
  if (!cachingEnabled)
    return; // Caching disabled

  // Now calculate the new list of frames to cache and run the cacher
  DEBUG_CACHING("VideoCache::updateCacheQueue");

  // Firstly clear the old cache queues
  cacheQueue.clear();
  cacheDeQueue.clear();

  // Get all items from the playlist. There are two lists. For the caching status (how full is the
  // cache) we have to consider all items in the playlist. However, we only cache top level items
  // and no child items.
  QList<playlistItem *> allItems    = playlist->getAllPlaylistItems();
  QList<playlistItem *> allItemsTop = playlist->getAllPlaylistItems(true);

  if (allItemsTop.count() == 0)
    // No cachable items in the playlist.
    return;

  const bool play = playback->playing();
  DEBUG_CACHING("VideoCache::updateCacheQueue Playback is %srunning", play ? "" : "not ");

  // Our caching priority list is like this:
  // 1: Cache all the frames in the item that is currently selected. In order to achieve this, we
  // will aggressively
  //    delete other frames from other sequences in the cache. This has highest priority.
  // 2: Cache all the frames from the following items (while there is space left in the cache). If
  // case of playback
  //    we will remove all frames from items that were already played back (are before the current
  //    item in the playlist). If playback is not running, we will not remove any frames from other
  //    items from the cache to achieve this.
  //
  // When frames have to be removed to fit the selected sequence into the cache, the following
  // priorities apply to frames from other sequences. (The ones with highest priority get removed
  // last). This priority list differs depending if playback is currently running or not.
  //
  // Playback is not running:
  // 1: The frames from the previous item have highest priority and are removed last. It is very
  // likely that in 'interactive'
  //    (playback is not running) mode, the user will go back to the previous item.
  // 2: The item after this item is next in the priority list.
  // 3: The item after 2 is next and so on (wrap around in the playlist) until the previous item is
  // reached.
  //
  // Playback is running:
  // 1: The item after this item has the highest priority (it will be played next)
  // 2: The item after 2 is next and so on (wrap around in the playlist) until the previous item is
  // reached.

  // Let's start with the currently selected item (if no item is selected, the first item in the
  // playlist is considered as being selected)
  auto selection = playlist->getSelectedItems();
  if (selection[0] == nullptr)
    selection[0] = allItems[0];
  // Get the position of the curretnly selected item
  int itemPos = allItems.indexOf(selection[0]);
  Q_ASSERT_X(
      itemPos >= 0, Q_FUNC_INFO, "The current item is not in the list of all items? No possible.");

  // At first, let's find out how much space in the cache is used.
  // In combination with cacheLevelMax we also know how much space is free.
  // While we are iterating through the list, we will delete all cached frames from the cache that
  // will never be cached (are outside of the items range of frames to show)
  int64_t cacheLevel = 0;
  for (playlistItem *item : allItems)
  {
    IndexRange range         = item->properties().startEndRange;
    QList<int> cached_frames = item->getCachedFrames();
    for (int i : cached_frames)
      if (i < range.first || i > range.second)
        item->removeFrameFromCache(i);

    auto cachingFrameSize = item->getCachingFrameSize();
    cacheLevel += item->getNumberCachedFrames() * cachingFrameSize;
  }
  if (cacheLevel > cacheLevelMax)
  {
    // The cache is overflowing (maybe the user made the cache smaller).
    // Delete items until the cache does not overflow anymore.
    // Start with the item before the currently selected one.
    int initialPos = (itemPos > 0) ? itemPos - 1 : allItems.count() - 1;
    int i          = initialPos;
    do
    {
      // Delete cached frames from this item until the cache is free enough
      QList<int>   cachedFrames = allItems[i]->getCachedFrames();
      unsigned int frameSize    = allItems[i]->getCachingFrameSize();
      for (int f : cachedFrames)
      {
        allItems[i]->removeFrameFromCache(f);
        cacheLevel -= frameSize;
        if (cacheLevel < cacheLevelMax)
          break;
      }

      // Go to the previous item
      i--;
      if (i < 0)
        i = allItems.count() - 1;
      if (i == initialPos)
        // We scanned all items but the cache is still not empty ...
        break;
    } while (cacheLevel >= cacheLevelMax);
  }
  // Save the current level of the cache
  cacheLevelCurrent = cacheLevel;

  // How much space do we need to cache the entire item?
  IndexRange range =
      selection[0]->properties().startEndRange; // These are the frames that we want to cache
  int64_t cachingFrameSize          = selection[0]->getCachingFrameSize();
  int64_t itemSpaceNeeded           = (range.second - range.first + 1) * cachingFrameSize;
  int64_t alreadyCached             = selection[0]->getNumberCachedFrames() * cachingFrameSize;
  int64_t additionalItemSpaceNeeded = itemSpaceNeeded - alreadyCached;

  if (play)
  {
    // Go through the playlist starting with the currently selected item.
    // Add as much of all items as possible. When the cache is full, mark the remaining frames as
    // "can be deleted"
    int     i             = itemPos;
    int64_t newCacheLevel = 0;

    // We start in "adding" mode where items are added. If the cache is full, we switch to
    // "deleting" mode where all frames of all items are removed. This is done for all items in the
    // playlist.
    bool adding = true;
    do
    {
      if (allItems[i]->properties().isIndexedByFrame())
      {
        // How much space do we need to cache the current item?
        auto    itemRange = allItems[i]->properties().startEndRange;
        int64_t itemCacheSize =
            (itemRange.second - itemRange.first + 1) * int64_t(allItems[i]->getCachingFrameSize());

        if (adding && allItems[i]->isCachable())
        {
          if (newCacheLevel + itemCacheSize <= cacheLevelMax)
          {
            // All frames of the item fit and there is even more space. We remain in "adding" mode.
            enqueueCacheJob(allItems[i], itemRange);
            newCacheLevel += itemCacheSize;
          }
          else
          {
            // Not all frames fit. Enqueue the ones that fit and set the ones that don't as "can be
            // deleted".
            int64_t availableSpace   = cacheLevelMax - newCacheLevel;
            int64_t nrFramesCachable = availableSpace / allItems[i]->getCachingFrameSize() + 1;

            // These frames should be added...
            IndexRange addFrames =
                IndexRange(itemRange.first, itemRange.first + nrFramesCachable - 1);
            enqueueCacheJob(allItems[i], addFrames);
            newCacheLevel += nrFramesCachable * allItems[i]->getCachingFrameSize();
            // ... and the rest should be removed (if they are cached)
            QList<int> cachedFrames = allItems[i]->getCachedFrames();
            for (int f : cachedFrames)
              if (f < addFrames.first || f > addFrames.second)
                cacheDeQueue.enqueue(plItemFrame(allItems[i], f));

            // The cache is now full. We switch to "deleting" mode.
            adding = false;
          }
        }
        else
        {
          // Enqueue all frames (that are cached) from the item as "can be deleted".
          QList<int> cachedFrames = allItems[i]->getCachedFrames();
          for (int f : cachedFrames)
            cacheDeQueue.enqueue(plItemFrame(allItems[i], f));
        }
      }

      // Goto the next item in the list
      i++;
      if (i >= allItems.count())
        i = 0;
    } while (i != itemPos);

    // Done. However, the list of frames that can be deleted is sorted the wrong way around. Reverse
    // it.
    std::reverse(cacheDeQueue.begin(), cacheDeQueue.end());
  }
  else // playback is not running
  {
    if (selection[0]->isCachable() && itemSpaceNeeded > cacheLevelMax &&
        additionalItemSpaceNeeded > 0)
    {
      DEBUG_CACHING("VideoCache::updateCacheQueue Item needs more space than cacheLevelMax");
      // All frames of the currently selected item will not fit into the cache
      // Delete all frames from all other items in the playlist from the cache and cache all frames
      // from this item that fit
      for (playlistItem *item : allItems)
      {
        if (item != selection[0])
        {
          // Mark all frames of this item as "can be removed if required"
          QList<int> cachedFrames = item->getCachedFrames();
          for (int f : cachedFrames)
            cacheDeQueue.enqueue(plItemFrame(item, f));
        }
      }

      // Adjust the range so that only the number of frames are cached that will fit
      int64_t nrFramesCachable = cacheLevelMax / selection[0]->getCachingFrameSize();
      range.second             = range.first + nrFramesCachable - 1;

      enqueueCacheJob(selection[0], range);
    }
    else if (selection[0]->isCachable() &&
             additionalItemSpaceNeeded > (cacheLevelMax - cacheLevel) &&
             additionalItemSpaceNeeded > 0)
    {
      DEBUG_CACHING("VideoCache::updateCacheQueue Not enough space for caching, deleting frames");
      // There is currently not enough space in the cache to cache all remaining frames but in
      // general the cache can hold all frames. Delete frames from the cache until it fits.

      // We go through all other items and get the frames that we will delete.
      // We start with the item before the one before the currently selected one and go back through
      // the list, wrap around and keep going until we are at the current selected item. Then (as
      // the last resort) we go to the item before the currently selected one.
      int i = itemPos - 1;
      // Go back in the list to the previous item that is indexed
      while (true)
      {
        if (i < 0)
          i = allItems.count() - 1;
        if (allItems[i]->properties().isIndexedByFrame())
          break;
        i--;
      }
      // Go back one item further to the one before the one before the currently selected one.
      i--;
      while (true)
      {
        if (i < 0)
          i = allItems.count() - 1;
        if (allItems[i]->properties().isIndexedByFrame())
          break;
        i--;
      }

      // Get the cache level without the current item (frames from the current item do not really
      // occupy space in the cache. We want to cache them anyways)
      int64_t cacheLevelWithoutCurrent =
          cacheLevel -
          selection[0]->getNumberCachedFrames() * int64_t(selection[0]->getCachingFrameSize());
      while ((itemSpaceNeeded + cacheLevelWithoutCurrent) > cacheLevelMax)
      {
        if (i == itemPos)
          // We went through the whole list and arrived back at the beginning.
          // If playback is running, we go to the previous item at last.
          i--;
        if (i < 0)
        {
          // There is no previous item or the previous item is the first one in the list
          i = allItems.count() - 1;
        }
        if (allItems[i]->getNumberCachedFrames() == 0)
        {
          i--;
          continue; // Nothing to delete for this item
        }

        // Which frames are cached for the item at position i?
        QList<int> cachedFrames = allItems[i]->getCachedFrames();
        int64_t    cachedFramesSize =
            cachedFrames.count() * int64_t(allItems[i]->getCachingFrameSize());

        if (additionalItemSpaceNeeded < cachedFramesSize)
        {
          // If we delete all frames from item i, there is more than enough space. So we only delete
          // as many frames as needed.
          int64_t nrFrames = additionalItemSpaceNeeded / allItems[i]->getCachingFrameSize() + 1;

          // Delete nrFrames frames from the back
          for (int f = cachedFrames.count() - 1; f >= 0 && nrFrames > 0; f--)
          {
            cacheDeQueue.enqueue(plItemFrame(allItems[i], cachedFrames[f]));
            cacheLevelWithoutCurrent -= allItems[i]->getCachingFrameSize();
            if ((cacheLevelWithoutCurrent + itemSpaceNeeded) <= cacheLevelMax)
              // Now there is enough space
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

        if (i == itemPos - 1)
        {
          // We went through all items and tried to delete frames but there is still not enough
          // space. That is not possible because we determined that the curretn item should fit if
          // we just delete enough frames.
          DEBUG_CACHING("VideoCache::updateCacheQueue ERROR! Deleting loop processed all frames "
                        "but still not enough space in the cache.");
          break;
        }

        // Go to the next (previous) item
        i--;
      }

      // Enqueue the job. This is the only job.
      // We will not delete any frames from any other items to cache frames from other items.
      enqueueCacheJob(selection[0], range);
    }
    else
    {
      if (additionalItemSpaceNeeded > 0)
      {
        DEBUG_CACHING("VideoCache::updateCacheQueue All frames of %s fit.",
                      selection[0]->getName().toLatin1().data());
        // All frames from the current item will fit and there is probably even space for more
        // items. In case of playback, we will continue with the next items and delete all frames
        // that were already played out. Otherwise, we don't delete any frames from the cache but we
        // will cache as many items as possible.
        enqueueCacheJob(selection[0], range);
        cacheLevel = cacheLevel + additionalItemSpaceNeeded;
      }

      // Continue caching with the next item
      int i = itemPos + 1;

      while (true)
      {
        // There is still space
        DEBUG_CACHING("VideoCache::updateCacheQueue Cache not full yet, attempting next item");

        if (i >= allItems.count())
          // Last item. Continue with item 0.
          i = 0;
        if (i == itemPos)
        {
          // We went through all items, wrapped around and are back at the current item. No more
          // items to cache.
          DEBUG_CACHING("VideoCache::updateCacheQueue No more items to cache.");
          break;
        }
        if (!allItems[i]->isCachable())
        {
          // Nothing to cache for this item.
          i++;
          continue;
        }

        DEBUG_CACHING("VideoCache::updateCacheQueue Attempt caching of next item %s.",
                      allItems[i]->getName().toLatin1().data());
        // How much space is there in the cache (excluding what is cached from the current item)?
        // Get the cache level without the current item (frames from the current item do not really
        // occupy space in the cache. We want to cache them anyways)
        int64_t cacheLevelWithoutCurrent =
            cacheLevel -
            allItems[i]->getNumberCachedFrames() * int64_t(allItems[i]->getCachingFrameSize());
        // How much space do we need to cache the entire item?
        range = allItems[i]->properties().startEndRange;
        int64_t itemCacheSize =
            (range.second - range.first + 1) * int64_t(allItems[i]->getCachingFrameSize());

        if ((itemCacheSize + cacheLevelWithoutCurrent) <= cacheLevelMax)
        {
          DEBUG_CACHING("VideoCache::updateCacheQueue Entire next item %s fits.",
                        allItems[i]->getName().toLatin1().data());
          // The entire item fits
          enqueueCacheJob(allItems[i], range);
        }
        else
        {
          // Only a part of the next item fits without deleting frames
          if ((itemCacheSize + cacheLevelWithoutCurrent) > cacheLevelMax)
          {
            // Only a part of the item fits.
            int64_t nrFramesCachable =
                (cacheLevelMax - cacheLevelWithoutCurrent) / allItems[i]->getCachingFrameSize();
            DEBUG_CACHING("VideoCache::updateCacheQueue Only %lld frames of next item %s fit.",
                          nrFramesCachable,
                          allItems[i]->getName().toLatin1().data());
            range.second = range.first + nrFramesCachable - 1;
            enqueueCacheJob(allItems[i], range);

            // The cache is now full
            break;
          }
        }

        i++;
      }
    }
  }

#if CACHING_DEBUG_OUTPUT && !NDEBUG
  if (!cacheQueue.isEmpty())
  {
    qDebug("VideoCache::updateCacheQueue updateCacheQueue summary -- cache:");
    for (const cacheJob &j : cacheQueue)
    {
      QString itemStr = j.plItem->getName();
      itemStr.append(" - ");
      itemStr.append(QString::number(j.frameRange.first) + "-" +
                     QString::number(j.frameRange.second));
      qDebug() << itemStr;
    }
  }
  if (!cacheDeQueue.isEmpty())
  {
    qDebug("VideoCache::updateCacheQueue updateCacheQueue summary -- deQueue:");
    playlistItem *lastItem = nullptr;
    QString       itemStr;
    for (const plItemFrame &f : cacheDeQueue)
    {
      if (f.first != lastItem)
      {
        // New item
        if (lastItem != nullptr)
          // Print the last items frames
          qDebug() << itemStr;
        lastItem = f.first;
        itemStr  = lastItem->getName();
        itemStr.append(" - ");
      }
      itemStr.append(" " + QString::number(f.second));
    }
    qDebug() << itemStr;
  }
#endif
}

void VideoCache::enqueueCacheJob(playlistItem *item, IndexRange range)
{
  // Only schedule frames for caching that were not yet cached.
  QList<int> cachedFrames = item->getCachedFrames();
  int        i            = range.first;
  while (cachedFrames.contains(i) && i < range.second)
    range.first = ++i;
  if (range.first != range.second)
    cacheQueue.append(cacheJob(item, range));
}

void VideoCache::startCaching()
{
  DEBUG_CACHING("VideoCache::startCaching %s", testMode ? "Test mode" : "");
  if (cacheQueue.isEmpty() && !testMode)
  {
    // Nothing in the queue to start caching for.
    workersState = workersIdle;
  }
  else
  {
    // Push a task to all the threads and start them.
    bool jobStarted = false;
    for (int i = 0; i < cachingThreadList.count(); i++)
      jobStarted |= pushNextJobToCachingThread(cachingThreadList[i]);

    workersState = jobStarted ? workersRunning : workersIdle;
  }
}

void VideoCache::watchItemForCachingFinished(playlistItem *item)
{
  watchingItem = item;
  if (watchingItem)
  {
    // Check if any frame of the item is schedueld for caching.
    // If not, there is nothing to wait for and the wait is over now.
    bool waitOver = true;
    for (auto j : cacheQueue)
      if (j.plItem == watchingItem)
      {
        waitOver = false;
        break;
      }
    if (waitOver)
    {
      DEBUG_CACHING("VideoCache::watchItemForCachingFinished item not in cache");
      playback->itemCachingFinished(watchingItem);
      watchingItem = nullptr;
    }
    else if (workersState == workersIdle)
    {
      // If the caching is currently not running, start it. Otherwise we will wait forever.
      DEBUG_CACHING("VideoCache::watchItemForCachingFinished waiting for item. Start caching.");
      startCaching();
    }
  }
}

// One of the workers is done with it's caching operation. Give it a new task if there is one and we
// are not breaking the caching process.
void VideoCache::threadCachingFinished()
{
  // Get the thread that caused this call
  QObject *      sender = QObject::sender();
  loadingWorker *worker = dynamic_cast<loadingWorker *>(sender);
  Q_ASSERT_X(worker->isWorking(), Q_FUNC_INFO, "The worker that just finished was not working?");
  worker->setWorking(false);
  DEBUG_CACHING_DETAIL(
      "VideoCache::threadCachingFinished - state %d - worker %p", workersState, worker);

  // Check if all threads have stopped.
  bool jobsRunning = false;
  for (loadingThread *t : cachingThreadList)
  {
    DEBUG_CACHING_DETAIL("VideoCache::threadCachingFinished WorkerList - worker %p - working %d",
                         t,
                         t->worker()->isWorking());
    if (t->worker()->isWorking())
      // A job is still running. Wait.
      jobsRunning = true;
  }

  if (testMode)
  {
    if (workersState == workersIntReqRestart)
    {
      // The test has not started yet. We are waiting for the normal caching to finish first.
      if (!jobsRunning)
      {
        DEBUG_CACHING("VideoCache::threadCachingFinished Start test now");
        testDuration.start();
        startCaching();
      }
    }
    else if (testLoopCount < 0 || workersState == workersIntReqStop)
    {
      // The test is over or was canceled.
      // We are not going to start any new threads. Wait for the remaining threads to finish.
      if (jobsRunning)
        DEBUG_CACHING("VideoCache::threadCachingFinished Test over - Waiting for jobs to finish");
      else
      {
        // Report the results of the test
        DEBUG_CACHING("VideoCache::threadCachingFinished Test over - All jobs finished");
        testFinished();
        // Restart normal caching
        updateCacheQueue();
        startCaching();
      }
    }
    else if (workersState == workersRunning)
    {
      // The caching performance test is running. Just push another test job.
      DEBUG_CACHING_DETAIL("VideoCache::threadCachingFinished Test mode - start next job");
      for (loadingThread *t : cachingThreadList)
        if (t->worker() == worker)
          jobsRunning |= pushNextJobToCachingThread(t);
    }
    return;
  }

  // Check the list of items that are scheduled for deletion. Because a thread finished, maybe now
  // we can delete the item(s).
  bool itemDeleted = false;
  for (auto it = itemsToDelete.begin(); it != itemsToDelete.end();)
  {
    // Is the item still being cached?
    bool itemCaching = false;
    for (loadingThread *t : cachingThreadList)
      if (t->worker()->getCacheItem() == *it)
      {
        itemCaching = true;
        break;
      }
    // Is the item still being loaded?
    bool loadingItem = (interactiveThread[0]->worker()->getCacheItem() == *it ||
                        interactiveThread[1]->worker()->getCacheItem() == *it);

    if (!itemCaching && !loadingItem)
    {
      // Remove the item from the loading queue (if in there)
      if (interactiveItemQueued[0] == (*it))
      {
        interactiveItemQueued[0]     = nullptr;
        interactiveItemQueued_Idx[0] = -1;
      }
      if (interactiveItemQueued[1] == (*it))
      {
        interactiveItemQueued[1]     = nullptr;
        interactiveItemQueued_Idx[1] = -1;
      }
      // Delete the item and remove it from the itemsToDelete list
      DEBUG_CACHING("VideoCache::threadCachingFinished delete item now %s",
                    (*it)->getName().toLatin1().data());
      (*it)->deleteLater();
      it          = itemsToDelete.erase(it);
      itemDeleted = true;
    }
    else
      ++it;
  }
  if (itemDeleted)
    emit updateCacheStatus();

  // Do the same thing for the items which need to clear their cache
  for (auto it = itemsToClearCache.begin(); it != itemsToClearCache.end();)
  {
    bool itemCaching = false;
    for (loadingThread *t : cachingThreadList)
      if (t->worker()->getCacheItem() == *it)
      {
        itemCaching = true;
        break;
      }

    if (!itemCaching)
    {
      // No job is caching the item anymore. Clear the cache now.
      (*it)->removeAllFramesFromCache();
      it = itemsToClearCache.erase(it);
    }
    else
      ++it;
  }

  if (watchingItem)
  {
    // See if there is more to be done for the item we are waiting for. If not, signal that caching
    // of the item is done.
    bool waitOver = true;
    for (auto j : cacheQueue)
    {
      if (j.plItem == watchingItem)
      {
        waitOver = false;
        break;
      }
    }
    if (waitOver)
    {
      DEBUG_CACHING_DETAIL("VideoCache::threadCachingFinished caching of requested item done");
      playback->itemCachingFinished(watchingItem);
      watchingItem = nullptr;
    }
  }

  // Also check if the worker is in the cachingWorkerList. If not, do not push a new job to it.
  if (deleteNrThreads > 0)
  {
    // We need to delete some threads. So this one has to go.
    int idx = -1;
    for (int i = 0; i < cachingThreadList.count(); i++)
      if (cachingThreadList[i]->worker() == worker)
        idx = i;
    Q_ASSERT_X(
        idx >= 0, Q_FUNC_INFO, "The thread that just finished was not found in the thread list.");
    loadingThread *t = cachingThreadList.takeAt(idx);
    t->exit();
    t->deleteLater();

    DEBUG_CACHING_DETAIL("VideoCache::threadCachingFinished Deleting thread %p", t);
    deleteNrThreads--;
  }
  else if (workersState == workersRunning)
  {
    // Get the thread of the worker and push the next cache job to it
    for (loadingThread *t : cachingThreadList)
      if (t->worker() == worker)
        jobsRunning |= pushNextJobToCachingThread(t);
  }

  if (!jobsRunning)
  {
    // All jobs are done
    DEBUG_CACHING("VideoCache::threadCachingFinished - All jobs done");
    if (workersState == workersIntReqStop || workersState == workersRunning)
      workersState = workersIdle;
    else if (workersState == workersIntReqRestart)
    {
      updateCacheQueue();
      startCaching();
    }
  }

  // Start/stop the timer that will update the caching status widget and the debug stuff
  if (statusUpdateTimer.isActive() && workersState == workersIdle)
    // Stop the timer and update one last time
    statusUpdateTimer.stop();
  else if (!statusUpdateTimer.isActive() && workersState != workersIdle)
    // The timer is not started yet, but it should be.
    // Update now and start the timer to trigger future updates.
    statusUpdateTimer.start(100);

  emit updateCacheStatus();

  DEBUG_CACHING_DETAIL("VideoCache::threadCachingFinished - new state %d", workersState);
}

bool VideoCache::pushNextJobToCachingThread(loadingThread *thread)
{
  if ((cacheQueue.isEmpty() && !testMode) || thread->isQuitting())
    // No more jobs in the cache queue or the thread does not accept new jobs.
    return false;

  if (testMode)
  {
    Q_ASSERT_X(testItem, Q_FUNC_INFO, "Test item invalid");
    auto range = testItem->properties().startEndRange;
    int  frameNr =
        functions::clip((1000 - testLoopCount) % (range.second - range.first) + range.first,
                        range.first,
                        range.second);
    if (frameNr < 0)
      frameNr = 0;
    thread->worker()->setJob(testItem, frameNr, true);
    thread->worker()->setWorking(true);
    thread->worker()->processCacheJob();
    DEBUG_CACHING_DETAIL("VideoCache::pushNextJobToCachingThread - %d of %s",
                         frameNr,
                         testItem->getName().toStdString().c_str());
    testLoopCount--;
    return true;
  }

  // If playback is running and playback is not waiting for a specific item to cache,
  // only start caching of a new job if caching is enabled while playback is running.
  if (playback->playing() && watchingItem == nullptr)
  {
    auto selection = playlist->getSelectedItems();
    if (selection[0] && selection[0]->properties().isIndexedByFrame())
    {
      // Playback is running and the item that is currently being shown is indexed by frame.
      // In this case, obey the restriction on nr threads while playback is running.

      if (nrThreadsPlayback == 0)
      {
        // No caching while playback is running
        DEBUG_CACHING_DETAIL(
            "VideoCache::pushNextJobToCachingThread no new job started nrThreadsPlayback=0");
        return false;
      }

      // Check if there is a limit on the number of threads to use while playback is running.
      int threadsWorking = 0;
      for (loadingThread *t : cachingThreadList)
      {
        if (t->worker()->isWorking())
          threadsWorking++;
      }

      if (nrThreadsPlayback <= threadsWorking)
      {
        // The maximum number (or more) of threads are already working.
        // Do not start another one.
        DEBUG_CACHING_DETAIL("VideoCache::pushNextJobToCachingThread no new job started "
                             "nrThreadsPlayback=%d threadsWorking=%d",
                             nrThreadsPlayback,
                             threadsWorking);
        return false;
      }
    }
  }

  QMutableListIterator<cacheJob> j(cacheQueue);
  playlistItem *                 plItem = nullptr;
  IndexRange                     range;
  while (j.hasNext())
  {
    cacheJob &job = j.next();
    if (!job.plItem->isCachable())
      // Remove the item from the list
      j.remove();
    else
    {
      // We might be able to cache from this item. Check if there is a thread limit for the item.
      int threadLimit = job.plItem->cachingThreadLimit();
      if (threadLimit != -1)
      {
        // How many threads are currently caching the given item?
        int nrThreadsForItem = 0;
        for (loadingThread *t : cachingThreadList)
          if (t->worker()->isWorking() && t->worker()->getCacheItem() == job.plItem)
            nrThreadsForItem++;
        if (nrThreadsForItem >= threadLimit)
          // Go to the next item. We can not add another thread to this one.
          continue;
      }

      // We can start another thread for this item
      plItem = job.plItem;
      range  = job.frameRange;

      // Check if this is the last frame to cache in the item
      if (range.first == range.second)
        j.remove();
      else
        // Update the frame range of the head item in the cache queue
        job.frameRange.first = range.first + 1;

      break;
    }
  }
  if (plItem == nullptr)
    // No item found that we can start another caching thread for.
    return false;

  // Get the size of one frame in bytes
  unsigned int frameSize = plItem->getCachingFrameSize();

  // We found an item that we can cache. Cache the first frame of it.
  int frameToCache = range.first;

  // First check if we need to free up space to cache this frame.
  while (cacheLevelCurrent + frameSize >= cacheLevelMax && !cacheDeQueue.isEmpty())
  {
    plItemFrame  frameToRemove     = cacheDeQueue.dequeue();
    unsigned int frameToRemoveSize = frameToRemove.first->getCachingFrameSize();

    DEBUG_CACHING_DETAIL("VideoCache::pushNextJobToCachingThread Remove frame %d of %s",
                         frameToRemove.second,
                         frameToRemove.first->getName().toStdString().c_str());
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
  Q_ASSERT_X(plItem != nullptr && frameToCache >= 0, Q_FUNC_INFO, "Invalid job.");
  thread->worker()->setJob(plItem, frameToCache);
  thread->worker()->setWorking(true);
  thread->worker()->processCacheJob();
  DEBUG_CACHING_DETAIL("VideoCache::pushNextJobToCachingThread - %d of %s",
                       frameToCache,
                       plItem->getName().toStdString().c_str());

  // Update the cache level
  cacheLevelCurrent += frameSize;

  return true;
}

void VideoCache::itemAboutToBeDeleted(playlistItem *item)
{
  // One of the items is about to be deleted. Let's stop the caching. Then the item can be deleted
  // and then we can re-think our caching strategy.

  // Are we currently loading a frame from this item in one of the interactive loading threads?
  bool loadingItem = (interactiveThread[0]->worker()->getCacheItem() == item ||
                      interactiveThread[1]->worker()->getCacheItem() == item);
  bool cachingItem = false;

  if (workersState != workersIdle)
  {
    // Are we currently caching a frame from this item?
    for (loadingThread *t : cachingThreadList)
      if (t->worker()->getCacheItem() == item)
        cachingItem = true;

    // An item is about to be deleted. We need to rethink what to cache next.
    workersState = workersIntReqRestart;
  }

  if (cachingItem || loadingItem)
  {
    // The item can be deleted when all caching/loading threads of the item returned.
    itemsToDelete.append(item);
    DEBUG_CACHING("VideoCache::itemAboutToBeDeleted delete item later %s",
                  item->getName().toLatin1().data());
  }
  else
  {
    // Remove the item from the loading queue (if in there)
    if (interactiveItemQueued[0] == item)
    {
      interactiveItemQueued[0]     = nullptr;
      interactiveItemQueued_Idx[0] = -1;
    }
    if (interactiveItemQueued[1] == item)
    {
      interactiveItemQueued[1]     = nullptr;
      interactiveItemQueued_Idx[1] = -1;
    }
    // The item can be deleted now.
    item->deleteLater();
    DEBUG_CACHING("VideoCache::itemAboutToBeDeleted delete item now %s",
                  item->getName().toLatin1().data());
  }

  emit updateCacheStatus();
}

void VideoCache::itemNeedsRecache(playlistItem *item, recacheIndicator clearItemCache)
{
  if (clearItemCache == RECACHE_NONE)
    return;
  else if (clearItemCache == RECACHE_UPDATE)
    scheduleCachingListUpdate();
  else
  {
    // Something about the given playlistitem changed and all items in the cache are invalid.
    // If a thread is currently caching the given item, we have to stop caching, clear the cache,
    // rethink what to cache and restart the caching.
    if (workersState != workersIdle)
    {
      // Are we currently caching a frame from this item?
      bool cachingItem = false;
      for (loadingThread *t : cachingThreadList)
        if (t->worker()->getCacheItem() == item)
          cachingItem = true;

      if (cachingItem)
      {
        // The cache of the item needs to be cleared when all threads working on this item finished.
        if (!itemsToClearCache.contains(item))
          itemsToClearCache.append(item);
      }
      else
        // We can clear the cache now
        item->removeAllFramesFromCache();
      workersState = workersIntReqRestart;
    }
    else
    {
      // The worker thread is idle. We can just clear the item cache now.
      item->removeAllFramesFromCache();
      // This also implies that we want to rethink what to cache
      scheduleCachingListUpdate();
    }
  }

  emit updateCacheStatus();
}

void VideoCache::testConversionSpeed()
{
  // Get the item that we will use.
  auto selection = playlist->getSelectedItems();
  if (selection[0] == nullptr)
  {
    QMessageBox::information(parentWidget,
                             "Test error",
                             "Please select an item from the playlist to perform the test on.");
    return;
  }
  testItem = selection.at(0);

  // Stop playback if running
  if (playback->playing())
    playback->on_stopButton_clicked();

  assert(parentWidget != nullptr);
  assert(testProgressDialog.isNull());
  testProgressDialog =
      new QProgressDialog("Running conversion test...", "Cancel", 0, 1000, parentWidget);
  testProgressDialog->setWindowModality(Qt::WindowModal);

  testLoopCount = 1000;
  testMode      = true;
  testProgrssUpdateTimer.start(200);

  if (workersState == workersIdle)
  {
    // Start caching (in test mode)
    testDuration.start();
    startCaching();
  }
  else
    // Request a restart (in test mode)
    workersState = workersIntReqRestart;
}

QStringList VideoCache::getCacheStatusText()
{
  QStringList txt;
  txt.append("Interactive:");
  txt.append(interactiveThread[0]->worker()->getStatus());
  txt.append(interactiveThread[1]->worker()->getStatus());
  txt.append("Caching:");
  for (loadingThread *t : cachingThreadList)
    txt.append(t->worker()->getStatus());
  return txt;
}

void VideoCache::updateTestProgress()
{
  if (testProgressDialog.isNull())
    return;

  // Check if the dialog was canceled
  if (testProgressDialog->wasCanceled())
    workersState = workersIntReqStop;

  // Update the dialog progress
  testProgressDialog->setValue(1000 - testLoopCount);
}

void VideoCache::testFinished()
{
  DEBUG_CACHING("VideoCache::testFinished");

  // Quit test mode
  testMode = false;
  testProgrssUpdateTimer.stop();
  delete testProgressDialog;
  testProgressDialog.clear();

  if (workersState == workersIntReqStop)
    // The test was canceled
    return;

  // Calculate and report the time
  int64_t msec = testDuration.elapsed();
  double  rate = 1000.0 * 1000 / msec;
  QMessageBox::information(
      parentWidget,
      "Test results",
      QString("We cached 1000 frames in %1 msec. The conversion rate is %2 frames per second.")
          .arg(msec)
          .arg(rate));
}

#include "VideoCache.moc"

} // namespace video
