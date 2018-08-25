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

#ifndef VIDEOCACHE_H
#define VIDEOCACHE_H

#include <QDockWidget>
#include <QElapsedTimer>
#include <QLabel>
#include <QPointer>
#include <QProgressDialog>
#include <QQueue>
#include <QTimer>
#include <QWidget>
#include "playlistTreeWidget.h"

class videoHandler;
class videoCache;

class videoCacheStatusWidget : public QWidget
{
  Q_OBJECT

public:
  videoCacheStatusWidget(QWidget *parent) : QWidget(parent), cacheLevelMB(0), cacheRateInBytesPerMs(0), cacheLevelMaxMB(0) {}
  // Override the paint event
  virtual void paintEvent(QPaintEvent *event) Q_DECL_OVERRIDE;
  void updateStatus(PlaylistTreeWidget *playlistWidget, unsigned int cacheRate);
private:
  // The floating point values (0 to 1) of the end positions of the blocks to draw
  QList<float> relativeValsEnd;
  unsigned int cacheLevelMB;
  unsigned int cacheRateInBytesPerMs;
  qint64 cacheLevelMaxMB;
};

class videoCache : public QObject
{
  Q_OBJECT

public:
  // The video cache interfaces with the playlist to see which items are going to be played next and the
  // playback controller to get the position in the video and the current state (playback/stop).
  videoCache(PlaylistTreeWidget *playlistTreeWidget, PlaybackController *playbackController, splitViewWidget *view, QWidget *parent);
  ~videoCache();

  // The user might have changed the settings. Update.
  void updateSettings();

  // Load the given frame of the given object. This also includes a queue with only one slot. If a frame is currently being
  // loaded, the next call will be saved and started as soon as the running loading request is done. If a request is waiting
  // and another one arrives, the waiting request will be discarded. There are two slots for loading requests. One for each
  // item that can be visible at the same time.
  void loadFrame(playlistItem *item, int frameIndex, int loadingSlot);

  // Setup the caching info dock widget here.
  void setupControls(QDockWidget *dock);

  // Test the conversion speed with the currently selected item
  void testConversionSpeed();

private slots:

  // This signal is sent from the playlistTreeWidget if something changed (another item was selected ...)
  // The video Cache will then re-evaluate what to cache next and start the cache worker. If caching is
  // currently running, the update will be performed when the currently running caching jobs are done.
  void scheduleCachingListUpdate();

  // The cacheThread finished. If we requested the interruption, update the cache queue and restart.
  // If the thread finished by itself, push the next item into it or goto idle state if there is no more things
  // to cache
  void threadCachingFinished();

  // The interactiveWorker finished loading a frame
  void interactiveLoaderFinished();

  // An item is about to be deleted. If we are currently caching something (especially from this item),
  // abort that operation immediately.
  void itemAboutToBeDeleted(playlistItem* item);

  // Something about the given playlistitem changed so that all items in the cache are now invalid.
  void itemNeedsRecache(playlistItem* item, recacheIndicator itemNeedsRecache);

  // update the caching rate at the video cache controller every 1s
  void updateCachingRate(unsigned int cacheRate) { cacheRateInBytesPerMs = cacheRate; }

  // Call the function playback->itemCachingFinished(item) when caching of this item is done.
  void watchItemForCachingFinished(playlistItem *item);

  // Analyze the current situation and decide which items are to be cached next (in which order) and
  // which frames can be removed from the cache.
  void updateCacheQueue();
 
private:
  // A cache job. Has a pointer to a playlist item and a range of frames to be cached.
  struct cacheJob
  {
    cacheJob() {}
    cacheJob(playlistItem *item, indexRange range);
    QPointer<playlistItem> plItem;
    indexRange frameRange;
  };
  typedef QPair<QPointer<playlistItem>, int> plItemFrame;

  // When the cache queue is updated, this function will start the background caching.
  void startCaching();

  QPointer<PlaylistTreeWidget> playlist;
  QPointer<PlaybackController> playback;
  QPointer<splitViewWidget>    splitView;
  QPointer<QWidget>            parentWidget;

  // Is caching even enabled?
  bool cachingEnabled;
  // The queue of caching jobs that are scheduled
  QQueue<cacheJob> cacheQueue;
  // The queue with a list of frames/items that can be removed from the queue if necessary
  QQueue<plItemFrame> cacheDeQueue;
  // If a frame is removed can be determined by the following cache states:
  qint64 cacheLevelMax;
  qint64 cacheLevelCurrent;

  // Enqueue the job in the queue. If all frames within the range are already cached in the item, do nothing.
  void enqueueCacheJob(playlistItem* item, indexRange range);

  unsigned int cacheRateInBytesPerMs;

  // Start the given number of worker threads (if caching is running, also new jobs will be pushed to the workers)
  void startWorkerThreads(int nrThreads);
  // If this number is > 0, the indicated number of threads will be deleted when a worker finishes (threadCachingFinished() is called)
  int deleteNrThreads;
  // How many threads are to be used when playback is running?
  int nrThreadsPlayback;

  // Our tiny internal state machine for the worker
  enum workerStateEnum
  {
    workerIdle,         // The worker is idle. We can update the cacheQuene and go to workerRunning
    workerRunning,      // The worker is running. If it finishes by itself goto workerIdle. If an interrupt is requested, goto workerInterruptRequested.
    workerIntReqStop,   // The worker is running but an interrupt was requested. Next goto workerIdle.
    workerIntReqRestart // The worker is running but an interrupt was requested because the queue needs updating. If the worker finished, we will update the queue and goto workerRunning.
  };
  workerStateEnum workerState;
  
  // This list contains the items that are scheduled for deletion. 
  // All items in this list will be deleted (->deleteLate()) when caching has halted.
  QList<playlistItem*> itemsToDelete;
  // This list contains the items that are scheduled for clearing the cache.
  // The cache of these items will be cleared when caching has halted.
  QList<playlistItem*> itemsToClearCache;

  // A simple QObject (to move to threads) that gets a pointer to a playlist item and loads a frame in that item.
  class loadingThread;

  // A list of caching threads that process caching of frames in parallel in the background
  QList<loadingThread*> cachingThreadList;

  // Two threads with a higher priority that performs interactive loading (if the user is the source of the request)
  loadingThread *interactiveThread[2];
  playlistItem  *interactiveItemQueued[2];
  int            interactiveItemQueued_Idx[2];

  // Get the next item and frame to cache from the queue and push it to the given worker.
  // Return false if there are no more jobs to be pushed.
  bool pushNextJobToCachingThread(loadingThread *thread);
  
  bool updateCacheQueueAndRestartWorker;

  // This item is watched. When caching of it is done, we will notify the playback controller.
  playlistItem *watchingItem;

  // If visible, we will show the current status of the threads in here
  QPointer<QLabel> cachingInfoLabel;
  
  // Keep a pointer to the status widget so we can update it when necessary
  QPointer<videoCacheStatusWidget> statusWidget;
  // A timer that is used to update the status widget and the info panel when caching is running
  QTimer statusUpdateTimer;
  void updateCachingInfoLabel(bool forceNotVisible=false);

  // When caching is running, the cache will update the status widget itself but there are some
  // cases when this must be triggered externally (for example if an item is deleted).
  // forceNonVisible: Also update the widgets if the controls are not visible (done when the controls are created).
  void updateCacheStatus(bool forceNonVisible=false);

  // Things for testing the caching speed
  QPointer<QProgressDialog> testProgressDialog;
  QPointer<playlistItem> testItem;              //< The item to use for the test
  bool testMode;                                //< Set to true when the test is running
  int testLoopCount;                            //< Set before the test starts. Count down to 0. Then the test is over.
  QTimer testProgrssUpdateTimer;                //< Periodically update the progress dialog
  void updateTestProgress();
  QElapsedTimer testDuration;                   //< Used to obtain the duration of the test
  void testFinished();                          //< Report the test results and stop the testProgrssUpdateTimer
};

#endif // VIDEOCACHE_H
