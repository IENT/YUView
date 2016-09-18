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

#ifndef VIDEOCACHE_H
#define VIDEOCACHE_H

#include <QtCore>
#include <QPixmap>
#include <QCache>
#include <QMutex>
#include <QMutexLocker>
#include <QQueue>
#include "typedef.h"
#include "videoHandler.h"

#include "playlistTreeWidget.h"
#include "playbackController.h"

class videoHandler;
class videoCache;

class videoCacheStatusWidget : public QWidget
{
public:
  videoCacheStatusWidget(QWidget *parent) : QWidget(parent) {};
  // Override the paint event
  virtual void paintEvent(QPaintEvent * event) Q_DECL_OVERRIDE;
  void setPlaylist (PlaylistTreeWidget *playlistWidget) { playlist = playlistWidget; }
  void setCache (videoCache *someCache) { cache = someCache; }
private:
  PlaylistTreeWidget *playlist;
  videoCache *cache;
};

// A cache job. Has a pointer to a playlist item and a range of frames to be cached.
class cacheJob
{
public:
  cacheJob() { plItem = NULL; }
  cacheJob(playlistItem *item, indexRange range) { plItem = item; frameRange = range; }
  playlistItem *plItem;
  indexRange frameRange;
};
typedef QPair<playlistItem*, int> plItemFrame;

// Unfortunately this cannot be declared as a nested class because of the Q_OBJECT macro.
class cachingThread : public QThread
{
  Q_OBJECT
public:
  cachingThread(QObject *parent) : QThread(parent) { clearCacheJob(); }
  void run() Q_DECL_OVERRIDE;
  void setCacheJob(playlistItem *item, int frame) { plItem = item; frameToCache = frame; };
  void clearCacheJob() { plItem = NULL; frameToCache = -1; }
  playlistItem *getCacheItem() { return plItem; }
private:
  playlistItem *plItem;
  int frameToCache;
};

class videoCache : public QObject
{
  Q_OBJECT

public:
  // The video cache interfaces with the playlist to see which items are going to be played next and the
  // playback controller to get the position in the video and the current state (playback/stop).
  videoCache(PlaylistTreeWidget *playlistTreeWidget, PlaybackController *playbackController, QObject *parent = 0);
  virtual ~videoCache();
  unsigned int cacheRateInBytesPerMs;

private slots:

  // This signal is sent from the playlisttreewidget if something changed (another item was selected ...)
  // The video Cache will then re-evaluate what to cache next and start the cache worker.
  void playlistChanged();

  // The cacheThread finished. If we requested the interruption, update the cache queue and restart.
  // If the thread finished by itself, push the next item into it or goto idle state if there is no more things
  // to cache
  void threadCachingFinished();

  // An item is about to be deleted. If we are currently caching something (especially from this item),
  // abort that operation immediately.
  void itemAboutToBeDeleted(playlistItem*);

  // update the caching rate at the video cache controller every 1s
  void updateCachingRate(unsigned int cacheRate);

private:
  // Analyze the current situation and decide which items are to be cached next (in which order) and
  // which frames can be removed from the cache.
  void updateCacheQueue();
  // Whe the cache queue is updated, this function will start the background caching.
  void startCaching();

  PlaylistTreeWidget *playlist;
  PlaybackController *playback;

  // The queue of caching jobs that are schedueled
  QQueue<cacheJob> cacheQueue;
  // The queue with a list of frames/items that can be removed from the queue if necessary
  QQueue<plItemFrame> cacheDeQueue;
  // If a frame is removed can be determined by the following cache states:
  qint64 cacheLevelMax;
  qint64 cacheLevelCurrent;

  // Our tiny internal state machine for the worker
  enum workerStateEnum
  {
    workerIdle,         // The worker is idle. We can update the cacheQuene and go to workerRunning
    workerRunning,      // The worker is running. If it finishes by itself goto workerIdle. If an interrupt is requested, goto workerInterruptRequested.
    workerIntReqStop,   // The worker is running but an interrupt was requested. Next goto workerIdle.
    workerIntReqRestart // The worker is running but an interrupt was requested because the queue needs updating. If the worker finished, we will update the queue and goto workerRunning.
  };
  workerStateEnum workerState;
  
  // This list contains the items that are schedueled for deletion. All items in this list will be deleted (->deleteLate()) when caching of a frame of this item is done.
  QList<playlistItem*> itemsToDelete;

  // A list of caching threads that process caching of frames in parallel
  QList<cachingThread*> cachingThreadList;
  // Get the next item and frame to cache from the queue and push it to the given thread.
  // Return false if there are no more jobs to be pushed.
  bool pushNextJobToThread(cachingThread *thread);
  
  bool updateCacheQueueAndRestartWorker;
};

#endif // VIDEOCACHE_H
