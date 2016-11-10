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

// Unfortunately this cannot be declared as a nested class because of the Q_OBJECT macro.
class cacheWorkerThread : public QThread
{
  Q_OBJECT
public:
  void run() Q_DECL_OVERRIDE;
  cacheWorkerThread() : QThread() { interruptionRequest = false; plItem = NULL; }
  void requestInterruption() { interruptionRequest = true; }
  void resetInterruptionRequest() { interruptionRequest = false; }
  void setJob(playlistItem *item, indexRange cacheFrames) { plItem = item; range = cacheFrames; }
signals:
  void cachingFinished();
private:
  bool interruptionRequest;
  playlistItem *plItem;
  indexRange    range;
};

class videoCache : public QObject
{
  Q_OBJECT

public:
  // The video cache interfaces with the playlist to see which items are going to be played next and the
  // playback controller to get the position in the video and the current state (playback/stop).
  videoCache(PlaylistTreeWidget *playlistTreeWidget, PlaybackController *playbackController, QObject *parent = 0);
  virtual ~videoCache();

private slots:

  // This signal is sent from the playlisttreewidget if something changed (another item was selected ...)
  // The video Cache will then re-evaluate what to cache next and start the cache worker.
  void playlistChanged();

  // The cacheThread finished. If we requested the interruption, update the cache queue and restart.
  // If the thread finished by itself, push the next item into it or goto idle state if there is no more things
  // to cache
  void workerCachingFinished();

  // An item is about to be deleted. If we are currently caching something (especially from this item),
  // abort that operation immediately. 
  void itemAboutToBeDeleted(playlistItem*);

private:
  void updateCacheQueue();
  void pushNextTaskToWorker();

  PlaylistTreeWidget *playlist;
  PlaybackController *playback;

  // A cache job. Has a pointer to a playlist item and a range of frames to be cached.
  class cacheJob
  {
  public:
    cacheJob() {}
    cacheJob(playlistItem *item, indexRange range) { plItem = item; frameRange = range; }
    QPointer<playlistItem> plItem;
    indexRange frameRange;
  };

  // The queue of caching jobs that are schedueled
  QQueue<cacheJob> cacheQueue;

  // Our tiny internal state machine for the worker
  enum workerStateEnum
  {
    workerIdle,         // The worker is idle. We can update the cacheQuene and go to workerRunning
    workerRunning,      // The worker is running. If it finishes by itself goto workerIdle. If an interrupt is requested, goto workerInterruptRequested.
    workerIntReqStop,   // The worker is running but an interrupt was requested. Next goto workerIdle.
    workerIntReqRestart // The worker is running bit an interrupz was requested because the queue needs updating. If the worker finished, we will update the queue and goto workerRunning.
  };
  workerStateEnum workerState;

  cacheWorkerThread cacheThread;

  bool updateCacheQueueAndRestartWorker;
};

#endif // VIDEOCACHE_H
